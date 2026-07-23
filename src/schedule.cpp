// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/schedule.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pkgapply {
namespace {

bool
active_mechanism_required(pkgplan::planned_active_outcome outcome)
{
  switch (outcome) {
    case pkgplan::planned_active_outcome::activate_incoming:
    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      return true;
    case pkgplan::planned_active_outcome::retain_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      return false;
  }
  throw std::invalid_argument("invalid planned active outcome");
}

std::optional<pkgimage::entry_id>
decision_incoming_entry(const pkgplan::removal_path_decision&)
{
  return std::nullopt;
}

template<class Decision>
std::optional<pkgimage::entry_id>
decision_incoming_entry(const Decision& decision)
{
  return decision.incoming_entry();
}

template<class Decision>
const pkgimage::package_entry*
incoming_entry(const Decision& decision,
               const pkgimage::package_image& image)
{
  const std::optional<pkgimage::entry_id> incoming =
      decision_incoming_entry(decision);
  if (!incoming)
    return nullptr;
  const pkgimage::package_entry* entry = image.entry(*incoming);
  if (entry == nullptr || entry->path.string() != decision.path().string())
    throw std::invalid_argument(
        "application schedule incoming entry binding mismatch");
  return entry;
}

struct active_node final {
  pkgplan::package_path path;
  pkgplan::planned_active_outcome outcome;
  std::optional<pkgimage::entry_id> incoming;
  const pkgimage::package_entry* entry;
};

template<class Plan>
std::vector<active_node>
active_nodes(const Plan& plan, const pkgimage::package_image* image)
{
  std::vector<active_node> nodes;
  for (const auto& decision : plan.paths()) {
    if (!active_mechanism_required(decision.active()))
      continue;
    const pkgimage::package_entry* entry = nullptr;
    if (decision.active() ==
        pkgplan::planned_active_outcome::activate_incoming) {
      if (image == nullptr)
        throw std::invalid_argument(
            "incoming active effect lacks package image authority");
      entry = incoming_entry(decision, *image);
      if (entry == nullptr)
        throw std::invalid_argument(
            "incoming active effect lacks image entry identifier");
    }
    nodes.push_back({decision.path(), decision.active(),
                     decision_incoming_entry(decision), entry});
  }
  return nodes;
}

bool
parent_before_child(const active_node& parent)
{
  if (parent.outcome != pkgplan::planned_active_outcome::activate_incoming)
    return false;
  if (parent.entry == nullptr)
    throw std::logic_error("incoming active node lacks image entry");
  return parent.entry->type == pkgimage::entry_type::directory;
}

void
add_edge(std::vector<std::set<std::size_t>>& edges,
         std::vector<std::size_t>& indegree,
         std::size_t before,
         std::size_t after)
{
  if (before == after)
    return;
  if (edges[before].insert(after).second)
    ++indegree[after];
}

std::vector<active_node>
order_active_nodes(std::vector<active_node> nodes,
                   const pkgimage::package_image* image)
{
  const std::size_t count = nodes.size();
  std::vector<std::set<std::size_t>> edges(count);
  std::vector<std::size_t> indegree(count, 0);

  for (std::size_t parent = 0; parent < count; ++parent) {
    for (std::size_t child = 0; child < count; ++child) {
      if (parent == child ||
          !nodes[parent].path.is_ancestor_of(nodes[child].path))
        continue;
      if (parent_before_child(nodes[parent]))
        add_edge(edges, indegree, parent, child);
      else
        add_edge(edges, indegree, child, parent);
    }
  }

  if (image != nullptr) {
    for (std::size_t hardlink = 0; hardlink < count; ++hardlink) {
      const pkgimage::package_entry* entry = nodes[hardlink].entry;
      if (entry == nullptr || entry->type != pkgimage::entry_type::hardlink)
        continue;
      if (!entry->hardlink_target)
        throw std::invalid_argument(
            "active hard link lacks its regular anchor");
      const std::string& anchor = entry->hardlink_target->string();
      for (std::size_t candidate = 0; candidate < count; ++candidate) {
        if (nodes[candidate].path.string() == anchor)
          add_edge(edges, indegree, candidate, hardlink);
      }
    }
  }

  struct ready_compare final {
    const std::vector<active_node>* nodes;
    bool operator()(std::size_t lhs, std::size_t rhs) const
    {
      if ((*nodes)[lhs].path != (*nodes)[rhs].path)
        return (*nodes)[lhs].path < (*nodes)[rhs].path;
      return lhs < rhs;
    }
  };

  std::set<std::size_t, ready_compare> ready(ready_compare{&nodes});
  for (std::size_t index = 0; index < count; ++index)
    if (indegree[index] == 0)
      ready.insert(index);

  std::vector<active_node> ordered;
  ordered.reserve(count);
  while (!ready.empty()) {
    const std::size_t current = *ready.begin();
    ready.erase(ready.begin());
    ordered.push_back(nodes[current]);
    for (const std::size_t next : edges[current]) {
      if (--indegree[next] == 0)
        ready.insert(next);
    }
  }

  if (ordered.size() != count)
    throw std::invalid_argument(
        "application active-effect dependencies contain a cycle");
  return ordered;
}

void
append_step(std::vector<application_effect_step>& steps,
            application_effect_step_kind kind,
            pkgplan::package_path path,
            std::optional<pkgimage::entry_id> incoming = std::nullopt)
{
  steps.emplace_back(
      static_cast<std::uint64_t>(steps.size()),
      kind,
      std::move(path),
      incoming);
}

template<class Plan>
application_effect_schedule
prepare(const Plan& plan,
        const pkgimage::package_image* image,
        const incoming_payload_plan* payloads,
        const old_object_capture_plan& captures)
{
  std::vector<application_effect_step> steps;

  for (const old_object_capture_request& capture : captures.requests())
    append_step(steps,
                application_effect_step_kind::capture_old_object,
                capture.path());

  if (payloads != nullptr) {
    if (image == nullptr || payloads->image() != image->identity())
      throw std::invalid_argument(
          "payload closure belongs to another package image");
    std::vector<pkgimage::entry_id> entries;
    for (const auto& requirement : payloads->requirements()) {
      if (requirement.regular_payload_entry())
        entries.push_back(*requirement.regular_payload_entry());
    }
    std::sort(entries.begin(), entries.end());
    entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
    for (const pkgimage::entry_id id : entries) {
      const pkgimage::package_entry* entry = image->entry(id);
      if (entry == nullptr || entry->type != pkgimage::entry_type::regular)
        throw std::invalid_argument(
            "payload closure regular entry is absent or non-regular");
      append_step(
          steps,
          application_effect_step_kind::stage_regular_payload,
          pkgplan::package_path::parse(entry->path.string()),
          id);
    }
  }

  for (const auto& decision : plan.paths()) {
    if (decision.rejected() == pkgplan::planned_rejected_outcome::none)
      continue;
    append_step(steps,
                application_effect_step_kind::publish_rejected_object,
                decision.path(),
                decision_incoming_entry(decision));
  }

  for (const active_node& node :
       order_active_nodes(active_nodes(plan, image), image))
  {
    append_step(steps,
                application_effect_step_kind::publish_active_object,
                node.path,
                node.incoming);
  }

  for (const auto& decision : plan.paths())
    append_step(steps,
                application_effect_step_kind::observe_result,
                decision.path());

  return application_effect_schedule(std::move(steps));
}

} // namespace

application_effect_step::application_effect_step(
    std::uint64_t ordinal,
    application_effect_step_kind kind,
    pkgplan::package_path path,
    std::optional<pkgimage::entry_id> incoming_entry)
    : ordinal_(ordinal),
      kind_(kind),
      path_(std::move(path)),
      incoming_entry_(incoming_entry)
{
  const auto kind_value = static_cast<std::uint8_t>(kind_);
  if (kind_value < 1 || kind_value > 5)
    throw std::invalid_argument("invalid application effect step kind");
  const bool incoming_applicable =
      kind_ == application_effect_step_kind::stage_regular_payload ||
      kind_ == application_effect_step_kind::publish_rejected_object ||
      kind_ == application_effect_step_kind::publish_active_object;
  if (!incoming_applicable && incoming_entry_)
    throw std::invalid_argument(
        "application effect step has an inapplicable incoming entry");
  if (kind_ == application_effect_step_kind::stage_regular_payload &&
      !incoming_entry_)
  {
    throw std::invalid_argument(
        "regular payload staging step lacks an incoming entry");
  }
}

std::uint64_t
application_effect_step::ordinal() const noexcept
{
  return ordinal_;
}

application_effect_step_kind
application_effect_step::kind() const noexcept
{
  return kind_;
}

const pkgplan::package_path&
application_effect_step::path() const noexcept
{
  return path_;
}

const std::optional<pkgimage::entry_id>&
application_effect_step::incoming_entry() const noexcept
{
  return incoming_entry_;
}

application_effect_schedule::application_effect_schedule(
    std::vector<application_effect_step> steps)
    : steps_(std::move(steps))
{
  for (std::size_t index = 0; index < steps_.size(); ++index) {
    if (steps_[index].ordinal() != index)
      throw std::invalid_argument(
          "application effect ordinals are not consecutive");
  }
}

const std::vector<application_effect_step>&
application_effect_schedule::steps() const noexcept
{
  return steps_;
}

application_effect_schedule
prepare_application_schedule(
    const pkgplan::installation_plan& plan,
    const pkgimage::package_image& image,
    const incoming_payload_plan& payloads,
    const old_object_capture_plan& captures)
{
  return prepare(plan, &image, &payloads, captures);
}

application_effect_schedule
prepare_application_schedule(
    const pkgplan::upgrade_plan& plan,
    const pkgimage::package_image& image,
    const incoming_payload_plan& payloads,
    const old_object_capture_plan& captures)
{
  return prepare(plan, &image, &payloads, captures);
}

application_effect_schedule
prepare_application_schedule(
    const pkgplan::removal_plan& plan,
    const old_object_capture_plan& captures)
{
  return prepare(plan, nullptr, nullptr, captures);
}

} // namespace pkgapply
