// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/backend.h>
#include <libpkgapply/restart.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

void
normalize_evidence(
    std::vector<application_backend_evidence_identity>& evidence)
{
  std::sort(evidence.begin(), evidence.end());
  if (std::adjacent_find(evidence.begin(), evidence.end()) != evidence.end())
    throw std::invalid_argument("duplicate backend evidence identity");
}

void
normalize_paths(std::vector<pkgplan::package_path>& paths)
{
  std::sort(paths.begin(), paths.end());
  if (std::adjacent_find(paths.begin(), paths.end()) != paths.end())
    throw std::invalid_argument("duplicate backend observation request path");
}

bool
valid_outcome(backend_operation_outcome outcome) noexcept
{
  switch (outcome) {
    case backend_operation_outcome::completed:
    case backend_operation_outcome::conditional_retained:
    case backend_operation_outcome::failed:
    case backend_operation_outcome::indeterminate:
      return true;
  }
  return false;
}

} // namespace

backend_operation_result::backend_operation_result(
    backend_operation_outcome outcome,
    std::vector<application_backend_evidence_identity> evidence)
    : outcome_(outcome), evidence_(std::move(evidence))
{
  if (!valid_outcome(outcome_))
    throw std::invalid_argument("invalid backend operation outcome");
  normalize_evidence(evidence_);
}

backend_operation_outcome
backend_operation_result::outcome() const noexcept
{ return outcome_; }
const std::vector<application_backend_evidence_identity>&
backend_operation_result::evidence() const noexcept
{ return evidence_; }

rejected_object_publication_result::rejected_object_publication_result(
    backend_operation_outcome outcome,
    std::optional<rejected_object_record_identity> record,
    std::vector<application_backend_evidence_identity> evidence)
    : outcome_(outcome), record_(std::move(record)),
      evidence_(std::move(evidence))
{
  if (!valid_outcome(outcome_) ||
      outcome_ == backend_operation_outcome::conditional_retained)
  {
    throw std::invalid_argument(
        "invalid rejected-object publication outcome");
  }
  if ((outcome_ == backend_operation_outcome::completed) !=
      record_.has_value())
  {
    throw std::invalid_argument(
        "rejected-object publication record applicability mismatch");
  }
  normalize_evidence(evidence_);
}

backend_operation_outcome
rejected_object_publication_result::outcome() const noexcept
{ return outcome_; }
const std::optional<rejected_object_record_identity>&
rejected_object_publication_result::record() const noexcept
{ return record_; }
const std::vector<application_backend_evidence_identity>&
rejected_object_publication_result::evidence() const noexcept
{ return evidence_; }

completed_evidence_publication_result::completed_evidence_publication_result(
    backend_operation_outcome outcome,
    std::optional<completed_application_evidence_identity> record,
    std::vector<application_backend_evidence_identity> evidence)
    : outcome_(outcome), record_(std::move(record)),
      evidence_(std::move(evidence))
{
  if (!valid_outcome(outcome_) ||
      outcome_ == backend_operation_outcome::conditional_retained)
  {
    throw std::invalid_argument(
        "invalid completed-evidence publication outcome");
  }
  if ((outcome_ == backend_operation_outcome::completed) !=
      record_.has_value())
  {
    throw std::invalid_argument(
        "completed-evidence publication record applicability mismatch");
  }
  normalize_evidence(evidence_);
}

backend_operation_outcome
completed_evidence_publication_result::outcome() const noexcept
{ return outcome_; }
const std::optional<completed_application_evidence_identity>&
completed_evidence_publication_result::record() const noexcept
{ return record_; }
const std::vector<application_backend_evidence_identity>&
completed_evidence_publication_result::evidence() const noexcept
{ return evidence_; }

backend_observation_batch
backend_observation_batch::make(
    std::vector<pkgplan::package_path> requested,
    std::vector<application_path_observation> observations,
    std::vector<application_backend_evidence_identity> evidence)
{
  normalize_paths(requested);
  std::sort(observations.begin(), observations.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.path() < rhs.path();
            });
  if (observations.size() != requested.size())
    throw std::invalid_argument("backend observation closure is incomplete");
  for (std::size_t index = 0; index < requested.size(); ++index) {
    if (observations[index].path() != requested[index])
      throw std::invalid_argument("backend observation closure path mismatch");
  }
  normalize_evidence(evidence);
  return backend_observation_batch(
      std::move(requested), std::move(observations), std::move(evidence));
}

backend_observation_batch::backend_observation_batch(
    std::vector<pkgplan::package_path> requested,
    std::vector<application_path_observation> observations,
    std::vector<application_backend_evidence_identity> evidence)
    : requested_(std::move(requested)),
      observations_(std::move(observations)), evidence_(std::move(evidence))
{
}

const std::vector<pkgplan::package_path>&
backend_observation_batch::requested() const noexcept { return requested_; }
const std::vector<application_path_observation>&
backend_observation_batch::observations() const noexcept
{ return observations_; }
const std::vector<application_backend_evidence_identity>&
backend_observation_batch::evidence() const noexcept { return evidence_; }
const application_path_observation*
backend_observation_batch::find(const pkgplan::package_path& path) const noexcept
{
  const auto item = std::lower_bound(
      observations_.begin(), observations_.end(), path,
      [](const auto& observation, const auto& wanted) {
        return observation.path() < wanted;
      });
  return item != observations_.end() && item->path() == path
      ? &*item
      : nullptr;
}

old_object_capture_request::old_object_capture_request(
    pkgplan::package_path path,
    bool for_rejected_object,
    bool for_recovery)
    : path_(std::move(path)), for_rejected_object_(for_rejected_object),
      for_recovery_(for_recovery)
{
  if (!for_rejected_object_ && !for_recovery_)
    throw std::invalid_argument("old object capture has no purpose");
}

const pkgplan::package_path&
old_object_capture_request::path() const noexcept { return path_; }
bool old_object_capture_request::for_rejected_object() const noexcept
{ return for_rejected_object_; }
bool old_object_capture_request::for_recovery() const noexcept
{ return for_recovery_; }

old_object_capture_result::old_object_capture_result(
    backend_operation_outcome outcome,
    application_path_observation captured,
    bool exact_recovery_possible,
    std::vector<application_backend_evidence_identity> evidence)
    : outcome_(outcome), captured_(std::move(captured)),
      exact_recovery_possible_(exact_recovery_possible),
      evidence_(std::move(evidence))
{
  if (!valid_outcome(outcome_))
    throw std::invalid_argument("invalid old-object capture outcome");
  if (outcome_ == backend_operation_outcome::completed &&
      captured_.state() != fact_state::known)
  {
    throw std::invalid_argument(
        "completed old-object capture lacks a present object");
  }
  if (exact_recovery_possible_ &&
      outcome_ != backend_operation_outcome::completed)
  {
    throw std::invalid_argument(
        "failed old-object capture claims exact recoverability");
  }
  normalize_evidence(evidence_);
}

backend_operation_outcome
old_object_capture_result::outcome() const noexcept { return outcome_; }
const application_path_observation&
old_object_capture_result::captured() const noexcept { return captured_; }
bool old_object_capture_result::exact_recovery_possible() const noexcept
{ return exact_recovery_possible_; }
const std::vector<application_backend_evidence_identity>&
old_object_capture_result::evidence() const noexcept { return evidence_; }

backend_active_effect_request
backend_active_effect_request::make(
    pkgplan::package_path path,
    pkgplan::planned_active_outcome outcome,
    std::optional<pkgimage::entry_id> incoming_entry)
{
  bool requires_incoming = false;
  switch (outcome) {
    case pkgplan::planned_active_outcome::activate_incoming:
      requires_incoming = true;
      break;
    case pkgplan::planned_active_outcome::retain_observed:
    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remove_directory_if_empty:
    case pkgplan::planned_active_outcome::remain_absent:
      break;
    default:
      throw std::invalid_argument("invalid planned active outcome");
  }
  if (requires_incoming != incoming_entry.has_value())
    throw std::invalid_argument("active effect incoming entry mismatch");
  return backend_active_effect_request(
      std::move(path), outcome, incoming_entry);
}

backend_active_effect_request::backend_active_effect_request(
    pkgplan::package_path path,
    pkgplan::planned_active_outcome outcome,
    std::optional<pkgimage::entry_id> incoming_entry)
    : path_(std::move(path)), outcome_(outcome),
      incoming_entry_(incoming_entry)
{
}

const pkgplan::package_path&
backend_active_effect_request::path() const noexcept { return path_; }
pkgplan::planned_active_outcome
backend_active_effect_request::outcome() const noexcept { return outcome_; }
const std::optional<pkgimage::entry_id>&
backend_active_effect_request::incoming_entry() const noexcept
{ return incoming_entry_; }

backend_rejected_effect_request
backend_rejected_effect_request::stage_incoming(
    pkgplan::package_path path,
    pkgimage::entry_id incoming_entry)
{
  return backend_rejected_effect_request(
      std::move(path), pkgplan::planned_rejected_outcome::stage_incoming,
      incoming_entry);
}

backend_rejected_effect_request
backend_rejected_effect_request::stage_old(pkgplan::package_path path)
{
  return backend_rejected_effect_request(
      std::move(path), pkgplan::planned_rejected_outcome::stage_old,
      std::nullopt);
}

backend_rejected_effect_request::backend_rejected_effect_request(
    pkgplan::package_path path,
    pkgplan::planned_rejected_outcome outcome,
    std::optional<pkgimage::entry_id> incoming_entry)
    : path_(std::move(path)), outcome_(outcome),
      incoming_entry_(incoming_entry)
{
}

const pkgplan::package_path&
backend_rejected_effect_request::path() const noexcept { return path_; }
pkgplan::planned_rejected_outcome
backend_rejected_effect_request::outcome() const noexcept { return outcome_; }
const std::optional<pkgimage::entry_id>&
backend_rejected_effect_request::incoming_entry() const noexcept
{ return incoming_entry_; }


std::optional<application_journal_record_identity>
application_backend_transaction::resumed_journal() const noexcept
{
  return std::nullopt;
}

application_restart_checkpoint
application_backend_transaction::restart_checkpoint(
    const application_journal_record&)
{
  throw std::logic_error(
      "application backend transaction has no restart checkpoint");
}

std::unique_ptr<application_backend_transaction>
application_backend::resume_with_incoming_image(
    const application_target_context&,
    target_mutation_lease&,
    const application_journal_record&,
    const pkgimage::package_image&)
{
  throw std::logic_error("application backend does not support restart");
}

std::unique_ptr<application_backend_transaction>
application_backend::resume_without_incoming_image(
    const application_target_context&,
    target_mutation_lease&,
    const application_journal_record&)
{
  throw std::logic_error("application backend does not support restart");
}

} // namespace pkgapply
