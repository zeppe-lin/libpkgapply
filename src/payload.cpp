// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/payload.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

std::optional<pkgimage::entry_id>
regular_payload_entry(const pkgimage::package_image& image,
                      const pkgimage::package_entry& entry)
{
  switch (entry.type) {
    case pkgimage::entry_type::regular:
      return entry.id;

    case pkgimage::entry_type::hardlink: {
      if (!entry.hardlink_target)
        throw std::invalid_argument(
            "incoming hard link lacks its regular payload anchor");
      const pkgimage::package_entry* target = image.find(*entry.hardlink_target);
      if (target == nullptr || target->type != pkgimage::entry_type::regular)
        throw std::invalid_argument(
            "incoming hard-link payload anchor is not a regular entry");
      return target->id;
    }

    case pkgimage::entry_type::directory:
    case pkgimage::entry_type::symlink:
    case pkgimage::entry_type::fifo:
    case pkgimage::entry_type::character_device:
    case pkgimage::entry_type::block_device:
      return std::nullopt;
  }
  throw std::invalid_argument("invalid incoming image entry type");
}

struct prepared_payloads final {
  pkgimage::entry_selection selection;
  std::vector<incoming_payload_requirement> requirements;
};

template<class Plan>
prepared_payloads
prepare(const Plan& plan, const pkgimage::package_image& image)
{
  std::vector<incoming_payload_requirement> requirements;
  std::vector<pkgimage::entry_id> regular_entries;

  for (const auto& decision : plan.paths()) {
    const bool active = decision.active() ==
        pkgplan::planned_active_outcome::activate_incoming;
    const bool rejected = decision.rejected() ==
        pkgplan::planned_rejected_outcome::stage_incoming;
    if (!active && !rejected)
      continue;
    if (!decision.incoming_entry())
      throw std::invalid_argument(
          "incoming effect lacks an image entry identifier");

    const pkgimage::package_entry* entry =
        image.entry(*decision.incoming_entry());
    if (entry == nullptr)
      throw std::invalid_argument(
          "incoming effect cites an absent image entry");
    if (entry->path.string() != decision.path().string())
      throw std::invalid_argument(
          "incoming effect entry resolves to another logical path");

    std::optional<pkgimage::entry_id> payload =
        regular_payload_entry(image, *entry);
    if (payload)
      regular_entries.push_back(*payload);
    requirements.emplace_back(
        decision.path(), entry->id, payload, active, rejected);
  }

  std::sort(regular_entries.begin(), regular_entries.end());
  regular_entries.erase(
      std::unique(regular_entries.begin(), regular_entries.end()),
      regular_entries.end());

  return {
      pkgimage::entry_selection::from_ids(image, std::move(regular_entries)),
      std::move(requirements),
  };
}

} // namespace

incoming_payload_requirement::incoming_payload_requirement(
    pkgplan::package_path path,
    pkgimage::entry_id image_entry,
    std::optional<pkgimage::entry_id> regular_payload_entry,
    bool required_for_active,
    bool required_for_rejected)
    : path_(std::move(path)),
      image_entry_(image_entry),
      regular_payload_entry_(regular_payload_entry),
      required_for_active_(required_for_active),
      required_for_rejected_(required_for_rejected)
{
  if (!required_for_active_ && !required_for_rejected_)
    throw std::invalid_argument("incoming payload requirement has no consumer");
}

const pkgplan::package_path&
incoming_payload_requirement::path() const noexcept
{
  return path_;
}

pkgimage::entry_id
incoming_payload_requirement::image_entry() const noexcept
{
  return image_entry_;
}

const std::optional<pkgimage::entry_id>&
incoming_payload_requirement::regular_payload_entry() const noexcept
{
  return regular_payload_entry_;
}

bool
incoming_payload_requirement::required_for_active() const noexcept
{
  return required_for_active_;
}

bool
incoming_payload_requirement::required_for_rejected() const noexcept
{
  return required_for_rejected_;
}

incoming_payload_plan::incoming_payload_plan(
    pkgimage::package_image_identity image,
    pkgimage::entry_selection selection,
    std::vector<incoming_payload_requirement> requirements)
    : image_(std::move(image)),
      selection_(std::move(selection)),
      requirements_(std::move(requirements))
{
}

const pkgimage::package_image_identity&
incoming_payload_plan::image() const noexcept
{
  return image_;
}

const pkgimage::entry_selection&
incoming_payload_plan::selection() const noexcept
{
  return selection_;
}

const std::vector<incoming_payload_requirement>&
incoming_payload_plan::requirements() const noexcept
{
  return requirements_;
}

incoming_payload_plan
prepare_incoming_payloads(
    const pkgplan::installation_plan& plan,
    const pkgimage::package_image& image)
{
  prepared_payloads prepared = prepare(plan, image);
  return incoming_payload_plan(
      image.identity(),
      std::move(prepared.selection),
      std::move(prepared.requirements));
}

incoming_payload_plan
prepare_incoming_payloads(
    const pkgplan::upgrade_plan& plan,
    const pkgimage::package_image& image)
{
  prepared_payloads prepared = prepare(plan, image);
  return incoming_payload_plan(
      image.identity(),
      std::move(prepared.selection),
      std::move(prepared.requirements));
}

} // namespace pkgapply
