// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/restart.h>

#include <libpkgapply/admission.h>

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
