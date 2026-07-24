// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/restart.h>

#include <libpkgapply/admission.h>

#include "application_engine.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

[[noreturn]] void
refuse(application_restart_error_code code, const char* message)
{
  throw application_restart_error(code, message);
}

template<class Request>
void
validate_journal_bindings(const Request& request,
                          pkgplan::operation_kind kind,
                          const application_backend& backend,
                          const application_journal_record& journal)
{
  const auto assessment = assess_application_restart(journal);
  if (!assessment.resumable())
    refuse(application_restart_error_code::journal_not_resumable,
           "application journal is not automatically resumable");

  const auto& header = journal.header();
  if (header.kind() != kind)
    refuse(application_restart_error_code::journal_operation_kind_mismatch,
           "application journal contains another operation kind");
  if (header.request() != request.identity())
    refuse(application_restart_error_code::journal_request_mismatch,
           "application journal belongs to another request");
  if (header.plan() != request.plan().identity())
    refuse(application_restart_error_code::journal_plan_mismatch,
           "application journal belongs to another operation plan");
  if (header.target() != request.target().identity())
    refuse(application_restart_error_code::journal_target_mismatch,
           "application journal belongs to another target context");
  if (header.control() != request.control().identity())
    refuse(application_restart_error_code::journal_control_mismatch,
           "application journal belongs to another execution control");
  if (header.backend() != backend.identity())
    refuse(application_restart_error_code::journal_backend_mismatch,
           "application journal belongs to another backend");
}

struct effect_progress final {
  bool intended = false;
  bool terminal = false;
  application_journal_event_kind terminal_kind =
      application_journal_event_kind::failed;
};

std::vector<effect_progress>
progress(const application_journal_record& journal)
{
  std::vector<effect_progress> result(journal.effects().size());
  for (const auto& event : journal.events()) {
    const auto effect = std::find_if(
        journal.effects().begin(), journal.effects().end(),
        [&event](const auto& candidate) {
          return candidate.identity() == event.effect();
        });
    if (effect == journal.effects().end())
      throw std::logic_error("validated journal cites an unknown effect");

    auto& item = result[effect->ordinal()];
    if (event.kind() == application_journal_event_kind::intent) {
      item.intended = true;
      continue;
    }
    item.terminal = true;
    item.terminal_kind = event.kind();
  }
  return result;
}

bool
active_effect_requires_recovery(const application_journal_record& journal)
{
  const auto states = progress(journal);
  for (const auto& effect : journal.effects()) {
    if (effect.kind() !=
        application_journal_effect_kind::publish_active_object)
    {
      continue;
    }

    const auto& state = states[effect.ordinal()];
    if (state.intended && !state.terminal)
      return true;
    if (!state.terminal)
      continue;
    if (state.terminal_kind == application_journal_event_kind::failed ||
        state.terminal_kind == application_journal_event_kind::indeterminate)
    {
      return true;
    }
  }
  return false;
}

application_restart_disposition
disposition(const application_journal_record& journal)
{
  if (journal.receipt()) {
    return journal.state() == application_journal_state::indeterminate ||
            journal.state() ==
                application_journal_state::external_resolution_pending
        ? application_restart_disposition::external_resolution_required
        : application_restart_disposition::terminal;
  }

  switch (journal.state()) {
    case application_journal_state::preparing:
    case application_journal_state::prepared:
    case application_journal_state::effects_visible:
    case application_journal_state::result_observed:
      return application_restart_disposition::resume_forward;

    case application_journal_state::mutating:
      return active_effect_requires_recovery(journal)
          ? application_restart_disposition::resume_recovery
          : application_restart_disposition::resume_forward;

    case application_journal_state::recovery_pending:
    case application_journal_state::recovering:
      return application_restart_disposition::resume_recovery;

    case application_journal_state::application_completed:
    case application_journal_state::recovered:
    case application_journal_state::finalized:
    case application_journal_state::abandoned:
      return application_restart_disposition::terminal;

    case application_journal_state::external_resolution_pending:
    case application_journal_state::indeterminate:
      return application_restart_disposition::external_resolution_required;
  }
  throw std::logic_error("validated journal has an invalid state");
}

} // namespace

application_restart_capture::application_restart_capture(
    old_object_capture_result result)
    : result_(std::move(result))
{
}

const pkgplan::package_path&
application_restart_capture::path() const noexcept
{
  return result_.captured().path();
}

const old_object_capture_result&
application_restart_capture::result() const noexcept
{
  return result_;
}

bool
operator<(const application_restart_capture& lhs,
          const application_restart_capture& rhs) noexcept
{
  return lhs.path() < rhs.path();
}

application_restart_rejected_effect::application_restart_rejected_effect(
    pkgplan::package_path path,
    rejected_object_publication_result result)
    : path_(std::move(path)), result_(std::move(result))
{
}

const pkgplan::package_path&
application_restart_rejected_effect::path() const noexcept
{
  return path_;
}

const rejected_object_publication_result&
application_restart_rejected_effect::result() const noexcept
{
  return result_;
}

bool
operator<(const application_restart_rejected_effect& lhs,
          const application_restart_rejected_effect& rhs) noexcept
{
  return lhs.path() < rhs.path();
}

application_restart_active_effect::application_restart_active_effect(
    pkgplan::package_path path,
    backend_operation_result result)
    : path_(std::move(path)), result_(std::move(result))
{
}

const pkgplan::package_path&
application_restart_active_effect::path() const noexcept
{
  return path_;
}

const backend_operation_result&
application_restart_active_effect::result() const noexcept
{
  return result_;
}

bool
operator<(const application_restart_active_effect& lhs,
          const application_restart_active_effect& rhs) noexcept
{
  return lhs.path() < rhs.path();
}

namespace {

template<class Value>
void
normalize_restart_path_values(std::vector<Value>& values,
                              const char* duplicate_message)
{
  std::sort(values.begin(), values.end());
  const auto duplicate = std::adjacent_find(
      values.begin(), values.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.path() == rhs.path();
      });
  if (duplicate != values.end())
    throw std::invalid_argument(duplicate_message);
}

template<class Value>
const Value*
find_restart_path_value(const std::vector<Value>& values,
                        const pkgplan::package_path& path) noexcept
{
  const auto item = std::lower_bound(
      values.begin(), values.end(), path,
      [](const auto& candidate, const auto& wanted) {
        return candidate.path() < wanted;
      });
  return item != values.end() && item->path() == path ? &*item : nullptr;
}

} // namespace

application_restart_recovery_effect::application_restart_recovery_effect(
    pkgplan::package_path path,
    backend_operation_result result)
    : path_(std::move(path)), result_(std::move(result))
{
}

const pkgplan::package_path&
application_restart_recovery_effect::path() const noexcept
{
  return path_;
}

const backend_operation_result&
application_restart_recovery_effect::result() const noexcept
{
  return result_;
}

bool
operator<(const application_restart_recovery_effect& lhs,
          const application_restart_recovery_effect& rhs) noexcept
{
  return lhs.path() < rhs.path();
}

application_restart_synchronization::application_restart_synchronization(
    application_durability_fact result)
    : result_(std::move(result))
{
}

application_durability_domain
application_restart_synchronization::domain() const noexcept
{
  return result_.domain();
}

const application_durability_fact&
application_restart_synchronization::result() const noexcept
{
  return result_;
}

bool
operator<(const application_restart_synchronization& lhs,
          const application_restart_synchronization& rhs) noexcept
{
  return lhs.domain() < rhs.domain();
}

application_restart_checkpoint
application_restart_checkpoint::make(
    application_journal_record_identity journal,
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
  normalize_restart_path_values(
      captures, "duplicate restart capture path");
  normalize_restart_path_values(
      rejected_effects, "duplicate restart rejected-effect path");
  normalize_restart_path_values(
      active_effects, "duplicate restart active-effect path");
  normalize_restart_path_values(
      recovery_effects, "duplicate restart recovery-effect path");
  std::sort(synchronizations.begin(), synchronizations.end());
  if (std::adjacent_find(
          synchronizations.begin(), synchronizations.end(),
          [](const auto& lhs, const auto& rhs) {
            return lhs.domain() == rhs.domain();
          }) != synchronizations.end())
  {
    throw std::invalid_argument("duplicate restart synchronization domain");
  }
  for (const auto& synchronization : synchronizations) {
    if (durability.status(synchronization.domain()) !=
        synchronization.result().status())
    {
      throw std::invalid_argument(
          "restart synchronization contradicts durability profile");
    }
  }

  std::sort(backend_evidence.begin(), backend_evidence.end());
  if (std::adjacent_find(
          backend_evidence.begin(), backend_evidence.end()) !=
      backend_evidence.end())
  {
    throw std::invalid_argument("duplicate restart backend evidence");
  }

  return application_restart_checkpoint(
      std::move(journal), std::move(admitted_observations),
      std::move(incoming_payload), std::move(captures),
      std::move(rejected_effects), std::move(active_effects),
      std::move(recovery_effects), std::move(synchronizations),
      std::move(durability),
      std::move(backend_evidence),
      std::move(completed_evidence));
}

application_restart_checkpoint::application_restart_checkpoint(
    application_journal_record_identity journal,
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
    : journal_(std::move(journal)),
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

const application_journal_record_identity&
application_restart_checkpoint::journal() const noexcept
{
  return journal_;
}

const backend_observation_batch&
application_restart_checkpoint::admitted_observations() const noexcept
{
  return admitted_observations_;
}

const std::optional<backend_operation_result>&
application_restart_checkpoint::incoming_payload() const noexcept
{
  return incoming_payload_;
}

const std::vector<application_restart_capture>&
application_restart_checkpoint::captures() const noexcept
{
  return captures_;
}

const std::vector<application_restart_rejected_effect>&
application_restart_checkpoint::rejected_effects() const noexcept
{
  return rejected_effects_;
}

const std::vector<application_restart_active_effect>&
application_restart_checkpoint::active_effects() const noexcept
{
  return active_effects_;
}

const std::vector<application_restart_recovery_effect>&
application_restart_checkpoint::recovery_effects() const noexcept
{
  return recovery_effects_;
}

const std::vector<application_restart_synchronization>&
application_restart_checkpoint::synchronizations() const noexcept
{
  return synchronizations_;
}

const application_durability_profile&
application_restart_checkpoint::durability() const noexcept
{
  return durability_;
}

const std::vector<application_backend_evidence_identity>&
application_restart_checkpoint::backend_evidence() const noexcept
{
  return backend_evidence_;
}

const std::optional<completed_application_evidence>&
application_restart_checkpoint::completed_evidence() const noexcept
{
  return completed_evidence_;
}

const application_restart_capture*
application_restart_checkpoint::find_capture(
    const pkgplan::package_path& path) const noexcept
{
  return find_restart_path_value(captures_, path);
}

const application_restart_rejected_effect*
application_restart_checkpoint::find_rejected_effect(
    const pkgplan::package_path& path) const noexcept
{
  return find_restart_path_value(rejected_effects_, path);
}

const application_restart_active_effect*
application_restart_checkpoint::find_active_effect(
    const pkgplan::package_path& path) const noexcept
{
  return find_restart_path_value(active_effects_, path);
}

const application_restart_recovery_effect*
application_restart_checkpoint::find_recovery_effect(
    const pkgplan::package_path& path) const noexcept
{
  return find_restart_path_value(recovery_effects_, path);
}

const application_restart_synchronization*
application_restart_checkpoint::find_synchronization(
    application_durability_domain domain) const noexcept
{
  const auto item = std::lower_bound(
      synchronizations_.begin(), synchronizations_.end(), domain,
      [](const auto& candidate, const auto wanted) {
        return candidate.domain() < wanted;
      });
  return item != synchronizations_.end() && item->domain() == domain
      ? &*item
      : nullptr;
}

application_restart_assessment::application_restart_assessment(
    application_journal_record_identity journal,
    application_journal_state state,
    application_restart_disposition disposition)
    : journal_(std::move(journal)), state_(state), disposition_(disposition)
{
}

const application_journal_record_identity&
application_restart_assessment::journal() const noexcept
{
  return journal_;
}

application_journal_state
application_restart_assessment::state() const noexcept
{
  return state_;
}

application_restart_disposition
application_restart_assessment::disposition() const noexcept
{
  return disposition_;
}

bool
application_restart_assessment::resumable() const noexcept
{
  return disposition_ == application_restart_disposition::resume_forward ||
      disposition_ == application_restart_disposition::resume_recovery;
}

application_restart_assessment
assess_application_restart(const application_journal_record& journal)
{
  return application_restart_assessment(
      journal.identity(), journal.state(), disposition(journal));
}

application_restart_error::application_restart_error(
    application_restart_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

application_restart_error_code
application_restart_error::code() const noexcept
{
  return code_;
}

void
validate_application_restart(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive)
{
  validate_application_admission(request, state, lease, backend, archive);
  validate_journal_bindings(
      request, pkgplan::operation_kind::install, backend, journal);
}

void
validate_application_restart(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive)
{
  validate_application_admission(request, state, lease, backend, archive);
  validate_journal_bindings(
      request, pkgplan::operation_kind::upgrade, backend, journal);
}

void
validate_application_restart(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal)
{
  validate_application_admission(request, state, lease, backend);
  validate_journal_bindings(
      request, pkgplan::operation_kind::remove, backend, journal);
}

application_receipt
resume_application(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive)
{
  detail::reopened_application reopened =
      detail::reopen_application_engine(
          request, state, lease, backend, journal, archive);
  return detail::replay_application_engine(
      std::move(reopened), request, state, lease, archive);
}

application_receipt
resume_application(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive)
{
  detail::reopened_application reopened =
      detail::reopen_application_engine(
          request, state, lease, backend, journal, archive);
  return detail::replay_application_engine(
      std::move(reopened), request, state, lease, archive);
}

application_receipt
resume_application(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const application_journal_record& journal)
{
  detail::reopened_application reopened =
      detail::reopen_application_engine(
          request, state, lease, backend, journal);
  return detail::replay_application_engine(
      std::move(reopened), request, state, lease);
}

void
validate_restarted_backend_transaction(
    const application_target_context& target,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const application_backend_transaction& transaction)
{
  validate_backend_transaction(target, lease, backend, transaction);
  if (transaction.attempt_nonce() != journal.header().attempt().nonce()) {
    refuse(application_restart_error_code::transaction_attempt_nonce_mismatch,
           "restarted transaction reports another attempt nonce");
  }
  const auto resumed = transaction.resumed_journal();
  if (!resumed || *resumed != journal.identity()) {
    refuse(application_restart_error_code::transaction_journal_mismatch,
           "restarted transaction did not reopen the requested journal");
  }
}

} // namespace pkgapply
