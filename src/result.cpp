// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/result.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace pkgapply {
namespace {

std::uint8_t tag(pkgplan::operation_kind value)
{
  switch (value) {
    case pkgplan::operation_kind::install: return 1;
    case pkgplan::operation_kind::upgrade: return 2;
    case pkgplan::operation_kind::remove: return 3;
  }
  throw std::invalid_argument("invalid application operation kind");
}

std::uint8_t tag(application_attempt_outcome value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(application_recovery_state value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(application_durability_domain value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(application_durability_status value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(application_path_role value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(application_effect_status value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(ownership_publication_status value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(fact_state value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(completed_object_kind value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(object_fact_provenance value)
{
  return static_cast<std::uint8_t>(value);
}
std::uint8_t tag(object_fact_completeness value)
{
  return static_cast<std::uint8_t>(value);
}

std::uint8_t tag(pkgplan::planned_active_outcome value)
{
  switch (value) {
    case pkgplan::planned_active_outcome::activate_incoming: return 1;
    case pkgplan::planned_active_outcome::retain_observed: return 2;
    case pkgplan::planned_active_outcome::remove_observed: return 3;
    case pkgplan::planned_active_outcome::remove_directory_if_empty: return 4;
    case pkgplan::planned_active_outcome::remain_absent: return 5;
  }
  throw std::invalid_argument("invalid planned active outcome");
}

std::uint8_t tag(pkgplan::planned_rejected_outcome value)
{
  switch (value) {
    case pkgplan::planned_rejected_outcome::none: return 1;
    case pkgplan::planned_rejected_outcome::stage_incoming: return 2;
    case pkgplan::planned_rejected_outcome::stage_old: return 3;
  }
  throw std::invalid_argument("invalid planned rejected outcome");
}

template<class Value, class Encoder>
void append_fact(detail::canonical_record& record,
                 const qualified_fact<Value>& fact,
                 Encoder encode)
{
  record.append_u8(tag(fact.state()));
  if (fact.state() == fact_state::known)
    encode(record, *fact.value());
}

void append_object(detail::canonical_record& record,
                   const completed_object_fact& object)
{
  record.append_bytes(object.path().string());
  record.append_u8(tag(object.kind()));
  append_fact(record, object.mode(),
              [](auto& out, std::uint32_t value) { out.append_u32(value); });
  append_fact(record, object.uid(),
              [](auto& out, std::uint64_t value) { out.append_u64(value); });
  append_fact(record, object.gid(),
              [](auto& out, std::uint64_t value) { out.append_u64(value); });
  append_fact(record, object.size(),
              [](auto& out, std::uint64_t value) { out.append_u64(value); });
  append_fact(record, object.mtime(),
              [](auto& out, const completed_object_timestamp& value) {
                out.append_u64(static_cast<std::uint64_t>(value.seconds));
                out.append_u32(value.nanoseconds);
              });
  append_fact(record, object.regular_content(),
              [](auto& out, const completed_regular_content_identity& value) {
                out.append_digest(value);
              });
  append_fact(record, object.symlink_target(),
              [](auto& out, const std::string& value) {
                out.append_bytes(value);
              });
  append_fact(record, object.device(),
              [](auto& out, const completed_device_number& value) {
                out.append_u64(value.major);
                out.append_u64(value.minor);
              });
  append_fact(record, object.hardlink(),
              [](auto& out, const completed_hardlink_relation& value) {
                out.append_bytes(value.anchor().string());
              });
  record.append_u8(tag(object.provenance()));
  record.append_u8(tag(object.completeness()));
}

void append_observation(detail::canonical_record& record,
                        const application_path_observation& observation)
{
  record.append_bytes(observation.path().string());
  record.append_u8(tag(observation.state()));
  if (observation.object())
    append_object(record, *observation.object());
}

void
append_path(detail::canonical_record& record,
            const application_path_consequence& path)
{
  record.append_bytes(path.path().string());
  record.append_u8(tag(path.role()));
  record.append_u8(tag(path.requested_active()));
  record.append_u8(tag(path.requested_rejected()));
  record.append_bool(path.incoming_entry().has_value());
  if (path.incoming_entry())
    record.append_u64(*path.incoming_entry());
  record.append_u64(path.ownership().before_existing_owners().size());
  for (const auto& owner : path.ownership().before_existing_owners())
    record.append_bytes(owner.string());
  record.append_u64(path.ownership().after_existing_owners().size());
  for (const auto& owner : path.ownership().after_existing_owners())
    record.append_bytes(owner.string());
  record.append_bool(path.ownership().incoming_package_owns_after());
  record.append_u8(tag(path.active_status()));
  record.append_u8(tag(path.rejected_status()));
  append_observation(record, path.before());
  append_observation(record, path.after());
  record.append_bool(path.rejected_object().has_value());
  if (path.rejected_object())
    record.append_digest(*path.rejected_object());
  record.append_u8(tag(path.publication()));
}

void normalize_paths(std::vector<application_path_consequence>& paths)
{
  std::sort(paths.begin(), paths.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.path() < rhs.path();
            });
  const auto duplicate = std::adjacent_find(
      paths.begin(), paths.end(),
      [](const auto& lhs, const auto& rhs) {
        return lhs.path() == rhs.path();
      });
  if (duplicate != paths.end())
    throw std::invalid_argument("duplicate path in application result");
}

void normalize_backend_evidence(
    std::vector<application_backend_evidence_identity>& evidence)
{
  std::sort(evidence.begin(), evidence.end());
  const auto duplicate = std::adjacent_find(evidence.begin(), evidence.end());
  if (duplicate != evidence.end())
    throw std::invalid_argument("duplicate backend application evidence");
}

void append_durability(detail::canonical_record& record,
                       const application_durability_profile& durability)
{
  for (const auto& fact : durability.facts()) {
    record.append_u8(tag(fact.domain()));
    record.append_u8(tag(fact.status()));
  }
}

void append_paths(detail::canonical_record& record,
                  const std::vector<application_path_consequence>& paths)
{
  record.append_u64(paths.size());
  for (const auto& path : paths)
    append_path(record, path);
}

void append_backend_evidence(
    detail::canonical_record& record,
    const std::vector<application_backend_evidence_identity>& evidence)
{
  record.append_u64(evidence.size());
  for (const auto& item : evidence)
    record.append_digest(item);
}

bool
same_ownership(const pkgplan::path_ownership_transition& lhs,
               const pkgplan::path_ownership_transition& rhs) noexcept
{
  return lhs.before_existing_owners() == rhs.before_existing_owners() &&
         lhs.after_existing_owners() == rhs.after_existing_owners() &&
         lhs.incoming_package_owns_after() ==
             rhs.incoming_package_owns_after();
}

application_path_role
application_role(pkgplan::installation_path_role role)
{
  switch (role) {
    case pkgplan::installation_path_role::incoming_entry:
      return application_path_role::incoming_entry;
    case pkgplan::installation_path_role::structural_parent:
      return application_path_role::structural_parent;
  }
  throw std::invalid_argument("invalid installation path role");
}

application_path_role
application_role(pkgplan::upgrade_path_role role)
{
  switch (role) {
    case pkgplan::upgrade_path_role::incoming_entry:
      return application_path_role::incoming_entry;
    case pkgplan::upgrade_path_role::obsolete_old_path:
      return application_path_role::obsolete_old_path;
    case pkgplan::upgrade_path_role::structural_parent:
      return application_path_role::structural_parent;
  }
  throw std::invalid_argument("invalid upgrade path role");
}

template<class Decision>
bool
matches_decision(const application_path_consequence& result,
                 const Decision& decision,
                 application_path_role role)
{
  return result.path() == decision.path() && result.role() == role &&
         result.requested_active() == decision.active() &&
         result.requested_rejected() == decision.rejected() &&
         result.incoming_entry() == decision.incoming_entry() &&
         same_ownership(result.ownership(), decision.ownership());
}

bool
matches_decision(const application_path_consequence& result,
                 const pkgplan::removal_path_decision& decision)
{
  return result.path() == decision.path() &&
         result.role() == application_path_role::installed_owned_path &&
         result.requested_active() == decision.active() &&
         result.requested_rejected() == decision.rejected() &&
         !result.incoming_entry().has_value() &&
         same_ownership(result.ownership(), decision.ownership());
}

template<class Decision, class Matcher>
void
validate_path_projection(const std::vector<Decision>& decisions,
                         const std::vector<application_path_consequence>& paths,
                         bool complete,
                         Matcher matches)
{
  if (complete && paths.size() != decisions.size())
    throw std::invalid_argument("completed evidence path universe mismatch");

  for (const auto& path : paths) {
    const auto decision = std::lower_bound(
        decisions.begin(), decisions.end(), path.path(),
        [](const auto& item, const pkgplan::package_path& wanted) {
          return item.path() < wanted;
        });
    if (decision == decisions.end() || decision->path() != path.path() ||
        !matches(path, *decision))
      throw std::invalid_argument("application result differs from accepted plan");
  }
}

void
validate_plan_paths(const pkgplan::installation_plan& plan,
                    const std::vector<application_path_consequence>& paths,
                    bool complete)
{
  validate_path_projection(
      plan.paths(), paths, complete,
      [](const auto& result, const auto& decision) {
        return matches_decision(
            result, decision, application_role(decision.role()));
      });
}

void
validate_plan_paths(const pkgplan::upgrade_plan& plan,
                    const std::vector<application_path_consequence>& paths,
                    bool complete)
{
  validate_path_projection(
      plan.paths(), paths, complete,
      [](const auto& result, const auto& decision) {
        return matches_decision(
            result, decision, application_role(decision.role()));
      });
}

void
validate_plan_paths(const pkgplan::removal_plan& plan,
                    const std::vector<application_path_consequence>& paths,
                    bool complete)
{
  validate_path_projection(
      plan.paths(), paths, complete,
      [](const auto& result, const auto& decision) {
        return matches_decision(result, decision);
      });
}

bool
visible_or_confirmed(application_durability_status status) noexcept
{
  return status == application_durability_status::visible ||
         status == application_durability_status::confirmed ||
         status == application_durability_status::not_attempted;
}

void
validate_completed_durability(
    const application_execution_control& control,
    const application_durability_profile& durability)
{
  if (durability.status(application_durability_domain::journal) !=
          application_durability_status::confirmed ||
      durability.status(application_durability_domain::completed_evidence) !=
          application_durability_status::confirmed)
  {
    throw std::invalid_argument(
        "completed application requires durable journal and evidence");
  }

  switch (control.durability()) {
    case application_durability_requirement::visibility_only:
      if (!std::all_of(durability.facts().begin(), durability.facts().end(),
                       [](const auto& fact) {
                         return visible_or_confirmed(fact.status());
                       }))
      {
        throw std::invalid_argument(
            "visibility-only application has unresolved durability");
      }
      return;

    case application_durability_requirement::journal_and_recovery:
      if (durability.status(application_durability_domain::recovery_staging) !=
              application_durability_status::not_attempted &&
          durability.status(application_durability_domain::recovery_staging) !=
              application_durability_status::confirmed)
      {
        throw std::invalid_argument(
            "required recovery staging durability is not confirmed");
      }
      if (!visible_or_confirmed(
              durability.status(application_durability_domain::active_namespace)) ||
          !visible_or_confirmed(durability.status(
              application_durability_domain::rejected_object_store)))
      {
        throw std::invalid_argument(
            "completed application visibility is not established");
      }
      return;

    case application_durability_requirement::all_application_domains:
      if (!std::all_of(durability.facts().begin(), durability.facts().end(),
                       [](const auto& fact) {
                         return fact.status() ==
                                    application_durability_status::not_attempted ||
                                fact.status() ==
                                    application_durability_status::confirmed;
                       }))
      {
        throw std::invalid_argument(
            "application domain durability is not confirmed");
      }
      return;
  }

  throw std::invalid_argument("invalid application durability requirement");
}

bool completed_path(const application_path_consequence& path)
{
  if (path.before().state() == fact_state::unknown ||
      path.after().state() == fact_state::unknown ||
      path.publication() != ownership_publication_status::eligible)
    return false;

  const bool rejected =
      path.requested_rejected() == pkgplan::planned_rejected_outcome::none
          ? path.rejected_status() == application_effect_status::not_attempted &&
                !path.rejected_object().has_value()
          : path.rejected_status() == application_effect_status::completed &&
                path.rejected_object().has_value();
  if (!rejected)
    return false;

  switch (path.requested_active()) {
    case pkgplan::planned_active_outcome::activate_incoming:
    case pkgplan::planned_active_outcome::retain_observed:
      return path.active_status() == application_effect_status::completed &&
             path.after().state() == fact_state::known;

    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      return path.active_status() == application_effect_status::completed &&
             path.after().state() == fact_state::not_applicable;

    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      return (path.active_status() == application_effect_status::completed &&
              path.after().state() == fact_state::not_applicable) ||
             (path.active_status() ==
                  application_effect_status::conditional_retained &&
              path.after().state() == fact_state::known);
  }
  return false;
}

void
validate_failed_paths(
    application_attempt_outcome outcome,
    const std::vector<application_path_consequence>& paths)
{
  for (const auto& path : paths) {
    if (path.publication() != ownership_publication_status::ineligible)
      throw std::invalid_argument(
          "failed application path is publication eligible");

    if (outcome == application_attempt_outcome::precondition_refused) {
      if (path.active_status() != application_effect_status::not_attempted ||
          path.rejected_status() != application_effect_status::not_attempted)
      {
        throw std::invalid_argument(
            "precondition refusal contains attempted application effects");
      }
    }

    if (outcome ==
            application_attempt_outcome::failed_before_target_mutation &&
        path.active_status() != application_effect_status::not_attempted &&
        path.active_status() != application_effect_status::failed)
    {
      throw std::invalid_argument(
          "pre-target-mutation failure contains a possibly applied active effect");
    }
  }
}


void validate_failed_outcome(application_attempt_outcome outcome,
                             application_recovery_state recovery,
                             const std::optional<application_journal_identity>& journal,
                             const application_durability_profile& durability)
{
  if (outcome == application_attempt_outcome::completed)
    throw std::invalid_argument("failed receipt cannot report completed outcome");

  if (outcome == application_attempt_outcome::precondition_refused &&
      recovery != application_recovery_state::unchanged)
    throw std::invalid_argument(
        "precondition refusal must leave active target unchanged");

  if (outcome ==
          application_attempt_outcome::failed_before_target_mutation &&
      recovery != application_recovery_state::unchanged)
  {
    throw std::invalid_argument(
        "pre-target-mutation failure must leave active target unchanged");
  }

  if (outcome == application_attempt_outcome::precondition_refused && journal)
    throw std::invalid_argument("precondition refusal must not create a journal");

  if (outcome == application_attempt_outcome::failed_fully_recovered &&
      recovery != application_recovery_state::exact_prior_state_restored)
    throw std::invalid_argument("fully recovered outcome requires exact restoration");

  if (outcome == application_attempt_outcome::failed_with_partial_effects &&
      recovery != application_recovery_state::known_residual_effects)
    throw std::invalid_argument("partial effects require residual-effect recovery state");

  if (outcome == application_attempt_outcome::indeterminate &&
      recovery != application_recovery_state::requires_authoritative_observation)
    throw std::invalid_argument("indeterminate outcome requires authoritative observation");

  if ((outcome == application_attempt_outcome::failed_fully_recovered ||
       outcome == application_attempt_outcome::failed_with_partial_effects ||
       outcome == application_attempt_outcome::effects_visible_durability_unconfirmed ||
       outcome == application_attempt_outcome::indeterminate) && !journal)
    throw std::invalid_argument("post-mutation receipt requires application journal");

  if (outcome ==
          application_attempt_outcome::effects_visible_durability_unconfirmed &&
      std::none_of(durability.facts().begin(), durability.facts().end(),
                   [](const auto& fact) {
                     return fact.status() ==
                                application_durability_status::visible ||
                            fact.status() ==
                                application_durability_status::unconfirmed;
                   }))
  {
    throw std::invalid_argument(
        "durability-unconfirmed outcome has no visible or unconfirmed domain");
  }
}

application_receipt_identity identify_receipt(
    pkgplan::operation_kind kind,
    const application_request_identity& request,
    const pkgplan::operation_plan_identity& plan,
    const application_attempt_identity& attempt,
    const application_target_context_identity& target,
    const application_execution_control_identity& control,
    const lease_bound_state_projection_identity& state_projection,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    const application_durability_profile& durability,
    const std::vector<application_path_consequence>& paths,
    const std::optional<application_journal_identity>& journal,
    const std::optional<completed_application_evidence>& completed,
    const std::vector<application_backend_evidence_identity>& backend_evidence)
{
  detail::canonical_record record(application_receipt_identity::canonical_domain());
  record.append_u16(application_receipt_schema_version);
  record.append_u8(tag(kind));
  record.append_digest(request);
  record.append_bytes(plan.string());
  record.append_digest(attempt);
  record.append_digest(target);
  record.append_digest(control);
  record.append_digest(state_projection);
  record.append_u8(tag(outcome));
  record.append_u8(tag(recovery));
  append_durability(record, durability);
  append_paths(record, paths);
  record.append_bool(journal.has_value());
  if (journal) record.append_digest(*journal);
  record.append_bool(completed.has_value());
  if (completed) record.append_digest(completed->identity());
  append_backend_evidence(record, backend_evidence);
  return detail::identity_factory::from_sha256<application_receipt_identity>(
      record.sha256());
}

} // namespace

completed_application_evidence
completed_application_evidence::make(
    pkgplan::operation_kind kind,
    application_request_identity request,
    pkgplan::operation_plan_identity plan,
    application_attempt_identity attempt,
    application_target_context_identity target,
    application_execution_control_identity control,
    lease_bound_state_projection_identity state_projection,
    application_journal_identity journal,
    std::vector<application_path_consequence> paths,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  normalize_backend_evidence(backend_evidence);
  if (!std::all_of(paths.begin(), paths.end(), completed_path))
    throw std::invalid_argument(
        "completed application evidence contains incomplete path effects");
  detail::canonical_record record(
      completed_application_evidence_identity::canonical_domain());
  record.append_u16(completed_application_evidence_schema_version);
  record.append_u8(tag(kind));
  record.append_digest(request);
  record.append_bytes(plan.string());
  record.append_digest(attempt);
  record.append_digest(target);
  record.append_digest(control);
  record.append_digest(state_projection);
  record.append_digest(journal);
  append_paths(record, paths);
  append_durability(record, durability);
  append_backend_evidence(record, backend_evidence);

  auto identity = detail::identity_factory::from_sha256<
      completed_application_evidence_identity>(record.sha256());
  return completed_application_evidence(
      std::move(identity),
      kind,
      std::move(request),
      std::move(plan),
      std::move(attempt),
      std::move(target),
      std::move(control),
      std::move(state_projection),
      std::move(journal),
      std::move(paths),
      std::move(durability),
      std::move(backend_evidence));
}


application_durability_fact::application_durability_fact(
    application_durability_domain domain,
    application_durability_status status)
    : domain_(domain), status_(status)
{
  if (static_cast<std::uint8_t>(domain_) < 1 ||
      static_cast<std::uint8_t>(domain_) > 6)
    throw std::invalid_argument("invalid application durability domain");
  if (static_cast<std::uint8_t>(status_) < 1 ||
      static_cast<std::uint8_t>(status_) > 5)
    throw std::invalid_argument("invalid application durability status");
}
application_durability_domain
application_durability_fact::domain() const noexcept { return domain_; }
application_durability_status
application_durability_fact::status() const noexcept { return status_; }
bool operator==(const application_durability_fact& lhs,
                const application_durability_fact& rhs) noexcept
{ return lhs.domain_ == rhs.domain_ && lhs.status_ == rhs.status_; }
bool operator!=(const application_durability_fact& lhs,
                const application_durability_fact& rhs) noexcept
{ return !(lhs == rhs); }
bool operator<(const application_durability_fact& lhs,
               const application_durability_fact& rhs) noexcept
{ return lhs.domain_ < rhs.domain_; }

application_durability_profile::application_durability_profile(
    std::vector<application_durability_fact> facts)
    : facts_(std::move(facts))
{
  std::sort(facts_.begin(), facts_.end());
  if (facts_.size() != 6)
    throw std::invalid_argument("application durability profile requires six domains");
  const auto duplicate = std::adjacent_find(
      facts_.begin(), facts_.end(),
      [](const auto& lhs, const auto& rhs) {
        return lhs.domain() == rhs.domain();
      });
  if (duplicate != facts_.end())
    throw std::invalid_argument("duplicate application durability domain");
  for (std::uint8_t expected = 1; expected <= 6; ++expected)
    if (static_cast<std::uint8_t>(facts_[expected - 1].domain()) != expected)
      throw std::invalid_argument("missing application durability domain");
}
const std::vector<application_durability_fact>&
application_durability_profile::facts() const noexcept { return facts_; }
application_durability_status
application_durability_profile::status(application_durability_domain domain) const
{
  const auto value = static_cast<std::uint8_t>(domain);
  if (value < 1 || value > facts_.size())
    throw std::invalid_argument("invalid application durability domain");
  return facts_[value - 1].status();
}
bool operator==(const application_durability_profile& lhs,
                const application_durability_profile& rhs) noexcept
{ return lhs.facts_ == rhs.facts_; }
bool operator!=(const application_durability_profile& lhs,
                const application_durability_profile& rhs) noexcept
{ return !(lhs == rhs); }

completed_application_evidence
completed_application_evidence::installation(
    const installation_application_request& request,
    application_attempt_identity attempt,
    lease_bound_state_projection_identity state_projection,
    application_journal_identity journal,
    std::vector<application_path_consequence> paths,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  validate_plan_paths(request.plan(), paths, true);
  validate_completed_durability(request.control(), durability);
  return make(pkgplan::operation_kind::install,
              request.identity(), request.plan().identity(),
              std::move(attempt), request.target().identity(),
              request.control().identity(), std::move(state_projection),
              std::move(journal), std::move(paths), std::move(durability),
              std::move(backend_evidence));
}

completed_application_evidence
completed_application_evidence::upgrade(
    const upgrade_application_request& request,
    application_attempt_identity attempt,
    lease_bound_state_projection_identity state_projection,
    application_journal_identity journal,
    std::vector<application_path_consequence> paths,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  validate_plan_paths(request.plan(), paths, true);
  validate_completed_durability(request.control(), durability);
  return make(pkgplan::operation_kind::upgrade,
              request.identity(), request.plan().identity(),
              std::move(attempt), request.target().identity(),
              request.control().identity(), std::move(state_projection),
              std::move(journal), std::move(paths), std::move(durability),
              std::move(backend_evidence));
}

completed_application_evidence
completed_application_evidence::removal(
    const removal_application_request& request,
    application_attempt_identity attempt,
    lease_bound_state_projection_identity state_projection,
    application_journal_identity journal,
    std::vector<application_path_consequence> paths,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  validate_plan_paths(request.plan(), paths, true);
  validate_completed_durability(request.control(), durability);
  return make(pkgplan::operation_kind::remove,
              request.identity(), request.plan().identity(),
              std::move(attempt), request.target().identity(),
              request.control().identity(), std::move(state_projection),
              std::move(journal), std::move(paths), std::move(durability),
              std::move(backend_evidence));
}

completed_application_evidence::completed_application_evidence(
    completed_application_evidence_identity identity,
    pkgplan::operation_kind kind,
    application_request_identity request,
    pkgplan::operation_plan_identity plan,
    application_attempt_identity attempt,
    application_target_context_identity target,
    application_execution_control_identity control,
    lease_bound_state_projection_identity state_projection,
    application_journal_identity journal,
    std::vector<application_path_consequence> paths,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : identity_(std::move(identity)), kind_(kind), request_(std::move(request)),
      plan_(std::move(plan)), attempt_(std::move(attempt)),
      target_(std::move(target)), control_(std::move(control)),
      state_projection_(std::move(state_projection)), journal_(std::move(journal)),
      paths_(std::move(paths)), durability_(std::move(durability)),
      backend_evidence_(std::move(backend_evidence))
{
}

std::uint16_t completed_application_evidence::schema_version() const noexcept
{ return schema_version_; }
const completed_application_evidence_identity&
completed_application_evidence::identity() const noexcept { return identity_; }
pkgplan::operation_kind completed_application_evidence::kind() const noexcept
{ return kind_; }
const application_request_identity& completed_application_evidence::request() const noexcept
{ return request_; }
const pkgplan::operation_plan_identity& completed_application_evidence::plan() const noexcept
{ return plan_; }
const application_attempt_identity& completed_application_evidence::attempt() const noexcept
{ return attempt_; }
const application_target_context_identity& completed_application_evidence::target() const noexcept
{ return target_; }
const application_execution_control_identity& completed_application_evidence::control() const noexcept
{ return control_; }
const lease_bound_state_projection_identity&
completed_application_evidence::state_projection() const noexcept
{ return state_projection_; }
const application_journal_identity& completed_application_evidence::journal() const noexcept
{ return journal_; }
const std::vector<application_path_consequence>&
completed_application_evidence::paths() const noexcept { return paths_; }
const application_durability_profile&
completed_application_evidence::durability() const noexcept { return durability_; }
const std::vector<application_backend_evidence_identity>&
completed_application_evidence::backend_evidence() const noexcept
{ return backend_evidence_; }

application_receipt
application_receipt::completed(
    completed_application_evidence evidence,
    application_recovery_state recovery,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  if (recovery != application_recovery_state::recovery_assets_retained &&
      recovery != application_recovery_state::unchanged)
    throw std::invalid_argument("completed receipt has invalid recovery state");
  normalize_backend_evidence(backend_evidence);
  auto identity = identify_receipt(
      evidence.kind(), evidence.request(), evidence.plan(), evidence.attempt(),
      evidence.target(), evidence.control(), evidence.state_projection(),
      application_attempt_outcome::completed, recovery, evidence.durability(),
      evidence.paths(), evidence.journal(), evidence, backend_evidence);
  return application_receipt(
      std::move(identity), evidence.kind(), evidence.request(), evidence.plan(),
      evidence.attempt(), evidence.target(), evidence.control(),
      evidence.state_projection(),
      application_attempt_outcome::completed, recovery, evidence.durability(),
      evidence.paths(), evidence.journal(), std::move(evidence),
      std::move(backend_evidence));
}

application_receipt
application_receipt::failed(
    const installation_application_request& request,
    application_attempt_identity attempt,
    lease_bound_state_projection_identity state_projection,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    std::vector<application_path_consequence> paths,
    std::optional<application_journal_identity> journal,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  validate_plan_paths(request.plan(), paths, false);
  return make_failed(
      pkgplan::operation_kind::install, request.identity(),
      request.plan().identity(), std::move(attempt), request.target().identity(),
      request.control().identity(), std::move(state_projection), outcome,
      recovery, std::move(durability), std::move(paths), std::move(journal),
      std::move(backend_evidence));
}

application_receipt
application_receipt::failed(
    const upgrade_application_request& request,
    application_attempt_identity attempt,
    lease_bound_state_projection_identity state_projection,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    std::vector<application_path_consequence> paths,
    std::optional<application_journal_identity> journal,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  validate_plan_paths(request.plan(), paths, false);
  return make_failed(
      pkgplan::operation_kind::upgrade, request.identity(),
      request.plan().identity(), std::move(attempt), request.target().identity(),
      request.control().identity(), std::move(state_projection), outcome,
      recovery, std::move(durability), std::move(paths), std::move(journal),
      std::move(backend_evidence));
}

application_receipt
application_receipt::failed(
    const removal_application_request& request,
    application_attempt_identity attempt,
    lease_bound_state_projection_identity state_projection,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    std::vector<application_path_consequence> paths,
    std::optional<application_journal_identity> journal,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  normalize_paths(paths);
  validate_plan_paths(request.plan(), paths, false);
  return make_failed(
      pkgplan::operation_kind::remove, request.identity(),
      request.plan().identity(), std::move(attempt), request.target().identity(),
      request.control().identity(), std::move(state_projection), outcome,
      recovery, std::move(durability), std::move(paths), std::move(journal),
      std::move(backend_evidence));
}

application_receipt
application_receipt::make_failed(
    pkgplan::operation_kind kind,
    application_request_identity request,
    pkgplan::operation_plan_identity plan,
    application_attempt_identity attempt,
    application_target_context_identity target,
    application_execution_control_identity control,
    lease_bound_state_projection_identity state_projection,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    std::vector<application_path_consequence> paths,
    std::optional<application_journal_identity> journal,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  validate_failed_outcome(outcome, recovery, journal, durability);
  validate_failed_paths(outcome, paths);
  normalize_paths(paths);
  normalize_backend_evidence(backend_evidence);
  auto identity = identify_receipt(
      kind, request, plan, attempt, target, control, state_projection, outcome,
      recovery, durability, paths, journal, std::nullopt, backend_evidence);
  return application_receipt(
      std::move(identity), kind, std::move(request), std::move(plan),
      std::move(attempt), std::move(target), std::move(control),
      std::move(state_projection), outcome, recovery, std::move(durability),
      std::move(paths), std::move(journal), std::nullopt,
      std::move(backend_evidence));
}

application_receipt::application_receipt(
    application_receipt_identity identity,
    pkgplan::operation_kind kind,
    application_request_identity request,
    pkgplan::operation_plan_identity plan,
    application_attempt_identity attempt,
    application_target_context_identity target,
    application_execution_control_identity control,
    lease_bound_state_projection_identity state_projection,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    std::vector<application_path_consequence> paths,
    std::optional<application_journal_identity> journal,
    std::optional<completed_application_evidence> completed_evidence,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : identity_(std::move(identity)), kind_(kind), request_(std::move(request)),
      plan_(std::move(plan)), attempt_(std::move(attempt)),
      target_(std::move(target)), control_(std::move(control)),
      state_projection_(std::move(state_projection)), outcome_(outcome),
      recovery_(recovery), durability_(std::move(durability)),
      paths_(std::move(paths)), journal_(std::move(journal)),
      completed_evidence_(std::move(completed_evidence)),
      backend_evidence_(std::move(backend_evidence))
{
}

std::uint16_t application_receipt::schema_version() const noexcept { return schema_version_; }
const application_receipt_identity& application_receipt::identity() const noexcept { return identity_; }
pkgplan::operation_kind application_receipt::kind() const noexcept { return kind_; }
const application_request_identity& application_receipt::request() const noexcept { return request_; }
const pkgplan::operation_plan_identity& application_receipt::plan() const noexcept { return plan_; }
const application_attempt_identity& application_receipt::attempt() const noexcept { return attempt_; }
const application_target_context_identity& application_receipt::target() const noexcept { return target_; }
const application_execution_control_identity& application_receipt::control() const noexcept { return control_; }
const lease_bound_state_projection_identity& application_receipt::state_projection() const noexcept { return state_projection_; }
application_attempt_outcome application_receipt::outcome() const noexcept { return outcome_; }
application_recovery_state application_receipt::recovery() const noexcept { return recovery_; }
const application_durability_profile& application_receipt::durability() const noexcept { return durability_; }
const std::vector<application_path_consequence>& application_receipt::paths() const noexcept { return paths_; }
const std::optional<application_journal_identity>& application_receipt::journal() const noexcept { return journal_; }
const std::optional<completed_application_evidence>& application_receipt::completed_evidence() const noexcept { return completed_evidence_; }
const std::vector<application_backend_evidence_identity>& application_receipt::backend_evidence() const noexcept { return backend_evidence_; }

} // namespace pkgapply
