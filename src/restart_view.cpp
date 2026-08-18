// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "restart_view.h"

#include "replay_fact.h"

#include <libpkgapply/capture.h>
#include <libpkgapply/precondition.h>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pkgapply::detail {
namespace {

std::size_t durability_index(application_durability_domain domain)
{
  switch (domain) {
    case application_durability_domain::journal: return 0;
    case application_durability_domain::incoming_staging: return 1;
    case application_durability_domain::recovery_staging: return 2;
    case application_durability_domain::active_namespace: return 3;
    case application_durability_domain::rejected_object_store: return 4;
    case application_durability_domain::completed_evidence: return 5;
  }
  throw std::invalid_argument("invalid application durability domain");
}

application_durability_profile durability_profile(
    const std::array<application_durability_status, 6>& values)
{
  return application_durability_profile({
      {application_durability_domain::journal, values[0]},
      {application_durability_domain::incoming_staging, values[1]},
      {application_durability_domain::recovery_staging, values[2]},
      {application_durability_domain::active_namespace, values[3]},
      {application_durability_domain::rejected_object_store, values[4]},
      {application_durability_domain::completed_evidence, values[5]},
  });
}

struct backend_evidence_hash final {
  [[nodiscard]] std::size_t operator()(
      const application_backend_evidence_identity& identity) const noexcept
  {
    std::size_t value = sizeof(std::size_t) == 8
        ? static_cast<std::size_t>(1469598103934665603ULL)
        : static_cast<std::size_t>(2166136261U);
    const std::size_t prime = sizeof(std::size_t) == 8
        ? static_cast<std::size_t>(1099511628211ULL)
        : static_cast<std::size_t>(16777619U);
    for (const auto byte : identity.bytes()) {
      value ^= static_cast<std::size_t>(byte);
      value *= prime;
    }
    return value;
  }
};

void append_unique_evidence(
    std::vector<application_backend_evidence_identity>& target,
    std::unordered_set<application_backend_evidence_identity,
                       backend_evidence_hash>& seen,
    const std::vector<application_backend_evidence_identity>& source)
{
  for (const auto& identity : source) {
    if (seen.insert(identity).second)
      target.push_back(identity);
  }
}

using path_ordinal_index = std::unordered_map<std::string_view, std::size_t>;

template<class Request>
path_ordinal_index plan_path_ordinals(const Request& request)
{
  path_ordinal_index index;
  const auto& paths = request.plan().paths();
  index.reserve(paths.size());
  for (std::size_t ordinal = 0; ordinal < paths.size(); ++ordinal) {
    if (ordinal != 0 &&
        !(paths[ordinal - 1].path() < paths[ordinal].path()))
    {
      throw std::invalid_argument(
          "application plan replay paths are not in canonical order");
    }
    if (!index.emplace(paths[ordinal].path().string(), ordinal).second)
      throw std::invalid_argument("application plan repeats a replay path");
  }
  return index;
}

std::size_t path_ordinal(const path_ordinal_index& index,
                         const pkgplan::package_path& path)
{
  const auto found = index.find(path.string());
  if (found == index.end())
    throw std::invalid_argument("application replay fact cites an unplanned path");
  return found->second;
}

template<class Value>
std::vector<Value> materialize_path_slots(
    std::vector<std::optional<Value>>& slots)
{
  std::vector<Value> values;
  values.reserve(static_cast<std::size_t>(std::count_if(
      slots.begin(), slots.end(), [](const auto& value) { return value.has_value(); })));
  for (auto& value : slots) {
    if (value)
      values.push_back(std::move(*value));
  }
  return values;
}

template<class Value>
void retain_path_fact(std::vector<std::optional<Value>>& slots,
                      std::size_t ordinal,
                      const Value& value,
                      const char* duplicate)
{
  if (slots[ordinal])
    throw std::invalid_argument(duplicate);
  slots[ordinal] = value;
}

application_journal_event_kind terminal_event(backend_operation_outcome outcome)
{
  switch (outcome) {
    case backend_operation_outcome::completed:
      return application_journal_event_kind::completed;
    case backend_operation_outcome::failed:
      return application_journal_event_kind::failed;
    case backend_operation_outcome::indeterminate:
      return application_journal_event_kind::indeterminate;
    case backend_operation_outcome::conditional_retained:
      throw std::invalid_argument(
          "conditional retained outcome is not valid for this replay fact");
  }
  throw std::invalid_argument("invalid backend operation outcome");
}

application_journal_event_kind active_terminal_event(
    backend_operation_outcome outcome)
{
  return outcome == backend_operation_outcome::conditional_retained
      ? application_journal_event_kind::completed
      : terminal_event(outcome);
}

application_journal_event_kind terminal_event(application_durability_status status)
{
  switch (status) {
    case application_durability_status::confirmed:
      return application_journal_event_kind::completed;
    case application_durability_status::visible:
    case application_durability_status::unconfirmed:
      return application_journal_event_kind::failed;
    case application_durability_status::indeterminate:
      return application_journal_event_kind::indeterminate;
    case application_durability_status::not_attempted:
      throw std::invalid_argument(
          "replay synchronization fact is not attempted");
  }
  throw std::invalid_argument("invalid application durability status");
}

application_durability_domain synchronization_domain(
    application_journal_effect_kind kind)
{
  switch (kind) {
    case application_journal_effect_kind::synchronize_incoming_staging:
      return application_durability_domain::incoming_staging;
    case application_journal_effect_kind::synchronize_recovery_staging:
      return application_durability_domain::recovery_staging;
    case application_journal_effect_kind::synchronize_active_namespace:
    case application_journal_effect_kind::synchronize_recovered_namespace:
      return application_durability_domain::active_namespace;
    case application_journal_effect_kind::synchronize_rejected_store:
      return application_durability_domain::rejected_object_store;
    case application_journal_effect_kind::synchronize_completed_evidence:
      return application_durability_domain::completed_evidence;
    default:
      break;
  }
  throw std::invalid_argument(
      "application replay synchronization fact names a non-synchronization effect");
}

template<class Decision>
bool active_result_changed_target(
    const Decision& decision,
    const application_restart_active_effect& fact)
{
  if (fact.result().outcome() != backend_operation_outcome::completed)
    return false;
  switch (decision.active()) {
    case pkgplan::planned_active_outcome::activate_incoming:
    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      return true;
    case pkgplan::planned_active_outcome::retain_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      return false;
  }
  throw std::invalid_argument("application replay fact has invalid active outcome");
}

template<class T>
const T& require_fact(
    const application_replay_fact& fact,
    const char* mismatch)
{
  const auto* value = std::get_if<T>(&fact);
  if (value == nullptr)
    throw std::invalid_argument(mismatch);
  return *value;
}

template<class Request>
application_restart_view build_view(
    const application_journal_history& history,
    const Request& request)
{
  backend_observation_batch admitted =
      decode_replay_seed(history.declaration().replay_seed());
  application_precondition_check preconditions =
      application_precondition_check::make(
          request.plan().preconditions(), admitted);
  if (!preconditions.satisfied())
    throw std::invalid_argument(
        "application journal replay seed does not contain admitted preconditions");
  const old_object_capture_plan capture_plan =
      prepare_old_object_captures(request.plan(), request.control());
  const path_ordinal_index path_ordinals = plan_path_ordinals(request);
  const std::size_t path_count = request.plan().paths().size();
  std::vector<const old_object_capture_request*> capture_requests(path_count);
  for (const auto& capture : capture_plan.requests()) {
    const std::size_t ordinal = path_ordinal(path_ordinals, capture.path());
    if (capture_requests[ordinal] != nullptr)
      throw std::logic_error("application capture plan repeats a path");
    capture_requests[ordinal] = &capture;
  }

  std::optional<backend_operation_result> incoming_payload;
  std::vector<std::optional<application_restart_capture>> captures_by_path(
      path_count);
  std::vector<std::optional<application_restart_rejected_effect>>
      rejected_by_path(path_count);
  std::vector<std::optional<application_restart_active_effect>> active_by_path(
      path_count);
  std::vector<std::optional<application_restart_recovery_effect>>
      recovery_by_path(path_count);
  std::array<std::optional<application_restart_synchronization>, 6>
      synchronization_by_domain;
  std::optional<completed_application_evidence> completed_evidence;
  std::unordered_set<application_backend_evidence_identity, backend_evidence_hash>
      backend_evidence_seen;
  std::vector<application_backend_evidence_identity> backend_evidence;
  backend_evidence_seen.reserve(admitted.evidence().size() + history.steps().size());
  backend_evidence.reserve(admitted.evidence().size() + history.steps().size());
  append_unique_evidence(
      backend_evidence, backend_evidence_seen, admitted.evidence());
  std::array<application_durability_status, 6> durability = {
      application_durability_status::confirmed,
      application_durability_status::not_attempted,
      application_durability_status::not_attempted,
      application_durability_status::not_attempted,
      application_durability_status::not_attempted,
      application_durability_status::not_attempted,
  };

  for (const auto& step : history.steps()) {
    if (!step.event() ||
        step.event()->kind() == application_journal_event_kind::intent)
    {
      continue;
    }
    const auto& event = *step.event();
    const auto& effect = history.effect(event.effect());
    append_unique_evidence(
        backend_evidence, backend_evidence_seen, event.backend_evidence());

    if (step.replay_fact().empty()) {
      const bool allowed =
          effect.kind() == application_journal_effect_kind::observe_result ||
          effect.kind() == application_journal_effect_kind::seal_receipt ||
          (effect.kind() ==
               application_journal_effect_kind::publish_completed_evidence &&
           event.kind() != application_journal_event_kind::completed);
      if (!allowed)
        throw std::invalid_argument(
            "terminal application journal step lacks its replay fact");
      continue;
    }

    const application_replay_fact decoded =
        decode_replay_fact(step.replay_fact(), request);
    switch (effect.kind()) {
      case application_journal_effect_kind::stage_incoming_payload: {
        const auto& fact = require_fact<backend_operation_result>(
            decoded, "incoming staging step has another replay fact type");
        if (event.kind() != terminal_event(fact.outcome()) ||
            event.backend_evidence() != fact.evidence())
        {
          throw std::invalid_argument(
              "incoming staging replay fact contradicts its terminal event");
        }
        if (incoming_payload &&
            (incoming_payload->outcome() != fact.outcome() ||
             incoming_payload->evidence() != fact.evidence()))
        {
          throw std::invalid_argument(
              "incoming staging effects retain conflicting replay facts");
        }
        incoming_payload = fact;
        durability[durability_index(
            application_durability_domain::incoming_staging)] =
            fact.outcome() == backend_operation_outcome::completed
                ? application_durability_status::visible
                : application_durability_status::indeterminate;
        break;
      }
      case application_journal_effect_kind::capture_old_object: {
        const auto& fact = require_fact<application_restart_capture>(
            decoded, "capture step has another replay fact type");
        if (!effect.path() || fact.path() != *effect.path() ||
            event.backend_evidence() != fact.result().evidence())
        {
          throw std::invalid_argument(
              "capture replay fact contradicts its journal effect");
        }
        backend_operation_outcome semantic = fact.result().outcome();
        if (semantic == backend_operation_outcome::conditional_retained)
          throw std::invalid_argument("capture replay fact is conditional");
        const std::size_t ordinal = path_ordinal(path_ordinals, fact.path());
        const old_object_capture_request* capture_request =
            capture_requests[ordinal];
        if (capture_request == nullptr)
          throw std::invalid_argument(
              "capture replay fact cites an unplanned capture");
        if (semantic == backend_operation_outcome::completed &&
            capture_request->for_recovery() &&
            request.control().recovery() ==
                application_recovery_requirement::exact_prior_state &&
            !fact.result().exact_recovery_possible())
        {
          semantic = backend_operation_outcome::failed;
        }
        if (event.kind() != terminal_event(semantic))
          throw std::invalid_argument(
              "capture replay fact contradicts its terminal event");
        retain_path_fact(
            captures_by_path, ordinal, fact,
            "duplicate application replay capture path");
        auto& status = durability[durability_index(
            application_durability_domain::recovery_staging)];
        status = fact.result().outcome() == backend_operation_outcome::completed
            ? application_durability_status::visible
            : application_durability_status::indeterminate;
        break;
      }
      case application_journal_effect_kind::publish_rejected_object: {
        const auto& fact = require_fact<application_restart_rejected_effect>(
            decoded, "rejected step has another replay fact type");
        if (!effect.path() || fact.path() != *effect.path() ||
            event.kind() != terminal_event(fact.result().outcome()) ||
            event.backend_evidence() != fact.result().evidence())
        {
          throw std::invalid_argument(
              "rejected replay fact contradicts its terminal event");
        }
        const std::size_t ordinal = path_ordinal(path_ordinals, fact.path());
        retain_path_fact(
            rejected_by_path, ordinal, fact,
            "duplicate application replay rejected path");
        auto& status = durability[durability_index(
            application_durability_domain::rejected_object_store)];
        if (fact.result().outcome() == backend_operation_outcome::completed)
          status = application_durability_status::visible;
        else if (fact.result().outcome() ==
                 backend_operation_outcome::indeterminate)
          status = application_durability_status::indeterminate;
        break;
      }
      case application_journal_effect_kind::publish_active_object: {
        const auto& fact = require_fact<application_restart_active_effect>(
            decoded, "active step has another replay fact type");
        if (!effect.path() || fact.path() != *effect.path() ||
            event.kind() != active_terminal_event(fact.result().outcome()) ||
            event.backend_evidence() != fact.result().evidence())
        {
          throw std::invalid_argument(
              "active replay fact contradicts its terminal event");
        }
        const std::size_t ordinal = path_ordinal(path_ordinals, fact.path());
        retain_path_fact(
            active_by_path, ordinal, fact,
            "duplicate application replay active path");
        auto& status = durability[durability_index(
            application_durability_domain::active_namespace)];
        if (active_result_changed_target(
                request.plan().paths()[ordinal], fact))
          status = application_durability_status::visible;
        if (fact.result().outcome() == backend_operation_outcome::indeterminate)
          status = application_durability_status::indeterminate;
        break;
      }
      case application_journal_effect_kind::recover_active_object: {
        const auto& fact = require_fact<application_restart_recovery_effect>(
            decoded, "recovery step has another replay fact type");
        if (!effect.path() || fact.path() != *effect.path() ||
            event.kind() != terminal_event(fact.result().outcome()) ||
            event.backend_evidence() != fact.result().evidence())
        {
          throw std::invalid_argument(
              "recovery replay fact contradicts its terminal event");
        }
        const std::size_t ordinal = path_ordinal(path_ordinals, fact.path());
        retain_path_fact(
            recovery_by_path, ordinal, fact,
            "duplicate application replay recovery path");
        auto& status = durability[durability_index(
            application_durability_domain::active_namespace)];
        if (fact.result().outcome() == backend_operation_outcome::completed)
          status = application_durability_status::visible;
        else if (fact.result().outcome() ==
                 backend_operation_outcome::indeterminate)
          status = application_durability_status::indeterminate;
        break;
      }
      case application_journal_effect_kind::synchronize_incoming_staging:
      case application_journal_effect_kind::synchronize_recovery_staging:
      case application_journal_effect_kind::synchronize_active_namespace:
      case application_journal_effect_kind::synchronize_recovered_namespace:
      case application_journal_effect_kind::synchronize_rejected_store:
      case application_journal_effect_kind::synchronize_completed_evidence: {
        const auto& fact = require_fact<application_restart_synchronization>(
            decoded, "synchronization step has another replay fact type");
        const auto domain = synchronization_domain(effect.kind());
        if (fact.domain() != domain ||
            event.kind() != terminal_event(fact.result().status()) ||
            !event.backend_evidence().empty())
        {
          throw std::invalid_argument(
              "synchronization replay fact contradicts its terminal event");
        }
        const std::size_t domain_index = durability_index(domain);
        synchronization_by_domain[domain_index] = fact;
        durability[domain_index] = fact.result().status();
        break;
      }
      case application_journal_effect_kind::publish_completed_evidence: {
        const auto& fact = require_fact<completed_application_evidence>(
            decoded, "completed-evidence step has another replay fact type");
        if (event.kind() != application_journal_event_kind::completed ||
            fact.request() != request.identity() ||
            fact.plan() != request.plan().identity() ||
            fact.attempt() != history.header().attempt().identity() ||
            fact.target() != request.target().identity() ||
            fact.control() != request.control().identity() ||
            fact.journal() != history.header().identity())
        {
          throw std::invalid_argument(
              "completed-evidence replay fact belongs to another authority");
        }
        if (completed_evidence && completed_evidence->identity() != fact.identity())
          throw std::invalid_argument(
              "application replay has conflicting completed evidence");
        completed_evidence = fact;
        durability[durability_index(
            application_durability_domain::completed_evidence)] =
            application_durability_status::visible;
        break;
      }
      case application_journal_effect_kind::observe_result:
      case application_journal_effect_kind::seal_receipt:
        throw std::invalid_argument(
            "non-mechanism application step carries a replay fact");
    }
  }

  std::vector<application_restart_capture> captures =
      materialize_path_slots(captures_by_path);
  std::vector<application_restart_rejected_effect> rejected_effects =
      materialize_path_slots(rejected_by_path);
  std::vector<application_restart_active_effect> active_effects =
      materialize_path_slots(active_by_path);
  std::vector<application_restart_recovery_effect> recovery_effects =
      materialize_path_slots(recovery_by_path);
  std::vector<application_restart_synchronization> synchronizations;
  synchronizations.reserve(synchronization_by_domain.size());
  for (auto& synchronization : synchronization_by_domain) {
    if (synchronization)
      synchronizations.push_back(std::move(*synchronization));
  }

  if (completed_evidence) {
    append_unique_evidence(
        backend_evidence, backend_evidence_seen,
        completed_evidence->backend_evidence());
  }

  return application_restart_view_builder::construct(
      history.declaration().identity(), history.header().attempt(),
      std::move(admitted), std::move(incoming_payload), std::move(captures),
      std::move(rejected_effects), std::move(active_effects),
      std::move(recovery_effects), std::move(synchronizations),
      durability_profile(durability), std::move(backend_evidence),
      std::move(completed_evidence));
}

} // namespace
} // namespace pkgapply::detail

namespace pkgapply {

application_restart_view::application_restart_view(
    application_journal_declaration_identity declaration,
    application_attempt attempt,
    backend_observation_batch admitted_observations,
    std::optional<backend_operation_result> incoming_payload,
    std::vector<application_restart_capture> captures,
    std::vector<application_restart_rejected_effect> rejected_effects,
    std::vector<application_restart_active_effect> active_effects,
    std::vector<application_restart_recovery_effect> recovery_effects,
    std::vector<application_restart_synchronization> synchronizations,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence,
    std::optional<completed_application_evidence> completed_evidence)
    : declaration_(std::move(declaration)),
      attempt_(std::move(attempt)),
      admitted_observations_(std::move(admitted_observations)),
      incoming_payload_(std::move(incoming_payload)),
      captures_(std::move(captures)),
      rejected_effects_(std::move(rejected_effects)),
      active_effects_(std::move(active_effects)),
      recovery_effects_(std::move(recovery_effects)),
      synchronizations_(std::move(synchronizations)),
      durability_(std::move(durability)),
      backend_evidence_(std::move(backend_evidence)),
      completed_evidence_(std::move(completed_evidence))
{
}

} // namespace pkgapply

namespace pkgapply::detail {

application_restart_view application_restart_view_builder::construct(
    application_journal_declaration_identity declaration,
    application_attempt attempt,
    backend_observation_batch admitted_observations,
    std::optional<backend_operation_result> incoming_payload,
    std::vector<application_restart_capture> captures,
    std::vector<application_restart_rejected_effect> rejected_effects,
    std::vector<application_restart_active_effect> active_effects,
    std::vector<application_restart_recovery_effect> recovery_effects,
    std::vector<application_restart_synchronization> synchronizations,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence,
    std::optional<completed_application_evidence> completed_evidence)
{
  return application_restart_view(
      std::move(declaration), std::move(attempt),
      std::move(admitted_observations), std::move(incoming_payload),
      std::move(captures), std::move(rejected_effects),
      std::move(active_effects), std::move(recovery_effects),
      std::move(synchronizations), std::move(durability),
      std::move(backend_evidence), std::move(completed_evidence));
}

application_restart_view application_restart_view_builder::build(
    const application_journal_history& history,
    const installation_application_request& request)
{
  return build_view(history, request);
}

application_restart_view application_restart_view_builder::build(
    const application_journal_history& history,
    const upgrade_application_request& request)
{
  return build_view(history, request);
}

application_restart_view application_restart_view_builder::build(
    const application_journal_history& history,
    const removal_application_request& request)
{
  return build_view(history, request);
}

} // namespace pkgapply::detail
