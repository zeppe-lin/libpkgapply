// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_engine.h"

#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pkgapply::detail {
namespace {

[[nodiscard]] application_durability_profile
not_attempted_durability()
{
  return application_durability_profile({
      {application_durability_domain::journal,
       application_durability_status::not_attempted},
      {application_durability_domain::incoming_staging,
       application_durability_status::not_attempted},
      {application_durability_domain::recovery_staging,
       application_durability_status::not_attempted},
      {application_durability_domain::active_namespace,
       application_durability_status::not_attempted},
      {application_durability_domain::rejected_object_store,
       application_durability_status::not_attempted},
      {application_durability_domain::completed_evidence,
       application_durability_status::not_attempted},
  });
}

[[nodiscard]] std::vector<pkgplan::package_path>
precondition_paths(const pkgplan::operation_preconditions& preconditions)
{
  std::vector<pkgplan::package_path> paths;
  paths.reserve(preconditions.paths().size());
  for (const auto& path : preconditions.paths())
    paths.push_back(path.path());
  return paths;
}

template<class Request>
[[nodiscard]] application_receipt
precondition_refusal(const Request& request,
                     const application_attempt& attempt,
                     const lease_bound_state_projection& state,
                     const application_precondition_check& preconditions)
{
  return application_receipt::failed(
      request,
      attempt.identity(),
      state.identity(),
      application_attempt_outcome::precondition_refused,
      application_recovery_state::unchanged,
      not_attempted_durability(),
      {},
      std::nullopt,
      preconditions.observations().evidence());
}

template<class Request>
[[nodiscard]] application_engine_admission
finish_admission(const Request& request,
                 const lease_bound_state_projection& state,
                 target_mutation_lease& lease,
                 application_backend& backend,
                 std::unique_ptr<application_backend_transaction> transaction)
{
  if (!transaction)
    throw std::logic_error("application backend returned no transaction");

  validate_backend_transaction(
      request.target(), lease, backend, *transaction);

  // Backend construction is not allowed to consume or replace the caller's
  // outer mutation authority.  Revalidate the same acquisition immediately
  // before the only live observation in this phase.
  validate_target_mutation_lease(request.target(), state, lease);

  application_attempt attempt = application_attempt::make(
      request.identity(),
      request.target().identity(),
      backend.identity(),
      transaction->attempt_nonce());

  application_precondition_check preconditions =
      application_precondition_check::make(
          request.plan().preconditions(),
          transaction->observe(
              precondition_paths(request.plan().preconditions())));

  if (!preconditions.satisfied()) {
    return application_engine_admission::refused(
        precondition_refusal(request, attempt, state, preconditions));
  }

  return application_engine_admission::admitted(
      std::move(attempt),
      std::move(preconditions),
      std::move(transaction));
}

} // namespace

admitted_application::admitted_application(
    application_attempt attempt,
    application_precondition_check preconditions,
    std::unique_ptr<application_backend_transaction> transaction)
    : attempt_(std::move(attempt)),
      preconditions_(std::move(preconditions)),
      transaction_(std::move(transaction))
{
  if (!transaction_)
    throw std::invalid_argument("admitted application requires a transaction");
  if (!preconditions_.satisfied())
    throw std::invalid_argument(
        "admitted application contains failed preconditions");
}

const application_attempt&
admitted_application::attempt() const noexcept
{
  return attempt_;
}

const application_precondition_check&
admitted_application::preconditions() const noexcept
{
  return preconditions_;
}

application_backend_transaction&
admitted_application::transaction() noexcept
{
  return *transaction_;
}

const application_backend_transaction&
admitted_application::transaction() const noexcept
{
  return *transaction_;
}

application_engine_admission
application_engine_admission::refused(application_receipt receipt)
{
  if (receipt.outcome() != application_attempt_outcome::precondition_refused)
    throw std::invalid_argument(
        "engine admission refusal requires a precondition-refused receipt");
  return application_engine_admission(value_type(std::move(receipt)));
}

application_engine_admission
application_engine_admission::admitted(
    application_attempt attempt,
    application_precondition_check preconditions,
    std::unique_ptr<application_backend_transaction> transaction)
{
  return application_engine_admission(value_type(
      std::in_place_type<admitted_application>,
      std::move(attempt),
      std::move(preconditions),
      std::move(transaction)));
}

application_engine_admission::application_engine_admission(value_type value)
    : value_(std::move(value))
{
}

bool
application_engine_admission::is_admitted() const noexcept
{
  return std::holds_alternative<admitted_application>(value_);
}

const application_receipt*
application_engine_admission::refusal() const noexcept
{
  return std::get_if<application_receipt>(&value_);
}

admitted_application*
application_engine_admission::admitted() noexcept
{
  return std::get_if<admitted_application>(&value_);
}

const admitted_application*
application_engine_admission::admitted() const noexcept
{
  return std::get_if<admitted_application>(&value_);
}

application_engine_admission
admit_application_engine(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const pkgimage::package_archive& archive)
{
  validate_application_admission(request, state, lease, backend, archive);
  return finish_admission(
      request,
      state,
      lease,
      backend,
      backend.begin_with_incoming_image(
          request.target(), lease, archive.image()));
}

application_engine_admission
admit_application_engine(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const pkgimage::package_archive& archive)
{
  validate_application_admission(request, state, lease, backend, archive);
  return finish_admission(
      request,
      state,
      lease,
      backend,
      backend.begin_with_incoming_image(
          request.target(), lease, archive.image()));
}

application_engine_admission
admit_application_engine(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend)
{
  validate_application_admission(request, state, lease, backend);
  return finish_admission(
      request,
      state,
      lease,
      backend,
      backend.begin_without_incoming_image(request.target(), lease));
}

} // namespace pkgapply::detail

namespace pkgapply::detail {
namespace {

application_journal_effect_kind
journal_kind(application_effect_step_kind kind)
{
  switch (kind) {
    case application_effect_step_kind::capture_old_object:
      return application_journal_effect_kind::capture_old_object;
    case application_effect_step_kind::stage_regular_payload:
      return application_journal_effect_kind::stage_incoming_payload;
    case application_effect_step_kind::publish_rejected_object:
      return application_journal_effect_kind::publish_rejected_object;
    case application_effect_step_kind::publish_active_object:
      return application_journal_effect_kind::publish_active_object;
    case application_effect_step_kind::observe_result:
      return application_journal_effect_kind::observe_result;
  }
  throw std::invalid_argument("invalid application schedule step kind");
}

std::vector<application_journal_effect>
journal_effects(const application_effect_schedule& schedule,
                bool has_incoming,
                bool has_recovery,
                bool has_active,
                bool has_rejected)
{
  std::vector<application_journal_effect> effects;
  effects.reserve(schedule.steps().size() + 7);
  for (const auto& step : schedule.steps()) {
    effects.push_back(application_journal_effect::make(
        effects.size(), journal_kind(step.kind()), step.path()));
  }

  const auto append = [&effects](application_journal_effect_kind kind) {
    effects.push_back(application_journal_effect::make(effects.size(), kind));
  };
  append(application_journal_effect_kind::synchronize_journal);
  if (has_incoming)
    append(application_journal_effect_kind::synchronize_incoming_staging);
  if (has_recovery)
    append(application_journal_effect_kind::synchronize_recovery_staging);
  if (has_active)
    append(application_journal_effect_kind::synchronize_active_namespace);
  if (has_rejected)
    append(application_journal_effect_kind::synchronize_rejected_store);
  append(application_journal_effect_kind::synchronize_completed_evidence);
  append(application_journal_effect_kind::seal_receipt);
  return effects;
}

template<class Request>
application_journal_header
journal_header(const Request& request,
               const admitted_application& admitted,
               const lease_bound_state_projection& state,
               const target_mutation_lease& lease)
{
  return application_journal_header::make(
      request.plan().kind(), request.identity(), request.plan().identity(),
      admitted.attempt(), request.target().identity(), request.control().identity(),
      state.identity(), lease.identity(), admitted.transaction().backend());
}

template<class Request>
journaled_application
publish_initial_journal(admitted_application admitted,
                        const Request& request,
                        const lease_bound_state_projection& state,
                        const target_mutation_lease& lease,
                        std::optional<incoming_payload_plan> payloads,
                        old_object_capture_plan captures,
                        application_effect_schedule schedule,
                        bool has_recovery,
                        bool has_active,
                        bool has_rejected)
{
  application_journal_record intended = application_journal_record::make(
      journal_header(request, admitted, state, lease),
      application_journal_state::preparing,
      journal_effects(schedule, payloads.has_value() &&
                      payloads->selection().size() != 0, has_recovery,
                      has_active, has_rejected),
      {});
  application_journal_record durable =
      admitted.transaction().publish_journal(intended);
  if (durable.identity() != intended.identity() ||
      durable.header().identity() != intended.header().identity())
    throw std::logic_error("backend changed the application journal snapshot");
  return journaled_application(
      std::move(admitted), std::move(payloads), std::move(captures),
      std::move(schedule), std::move(durable));
}

template<class Plan>
bool has_active_effect(const Plan& plan)
{
  for (const auto& path : plan.paths()) {
    if (path.active() != pkgplan::planned_active_outcome::retain_observed &&
        path.active() != pkgplan::planned_active_outcome::remain_absent)
      return true;
  }
  return false;
}

template<class Plan>
bool has_rejected_effect(const Plan& plan)
{
  for (const auto& path : plan.paths()) {
    if (path.rejected() != pkgplan::planned_rejected_outcome::none)
      return true;
  }
  return false;
}

} // namespace

journaled_application::journaled_application(
    admitted_application admitted,
    std::optional<incoming_payload_plan> payloads,
    old_object_capture_plan captures,
    application_effect_schedule schedule,
    application_journal_record journal)
    : admitted_(std::move(admitted)),
      payloads_(std::move(payloads)),
      captures_(std::move(captures)),
      schedule_(std::move(schedule)),
      journal_(std::move(journal))
{
  if (journal_.state() != application_journal_state::preparing)
    throw std::invalid_argument("initial engine journal is not preparing");
  if (journal_.header().attempt().identity() != admitted_.attempt().identity())
    throw std::invalid_argument("engine journal names another attempt");
}

admitted_application& journaled_application::admitted() noexcept { return admitted_; }
const admitted_application& journaled_application::admitted() const noexcept { return admitted_; }
const std::optional<incoming_payload_plan>&
journaled_application::payloads() const noexcept { return payloads_; }
const old_object_capture_plan&
journaled_application::captures() const noexcept { return captures_; }
const application_effect_schedule& journaled_application::schedule() const noexcept { return schedule_; }
const application_journal_record& journaled_application::journal() const noexcept { return journal_; }

journaled_application
journal_application_engine(
    admitted_application admitted,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image)
{
  auto payloads = prepare_incoming_payloads(request.plan(), image);
  auto captures = prepare_old_object_captures(request.plan(), request.control());
  auto schedule = prepare_application_schedule(
      request.plan(), image, payloads, captures);
  const bool has_recovery = !captures.requests().empty();
  return publish_initial_journal(
      std::move(admitted), request, state, lease, std::move(payloads),
      std::move(captures), std::move(schedule), has_recovery,
      has_active_effect(request.plan()), has_rejected_effect(request.plan()));
}

journaled_application
journal_application_engine(
    admitted_application admitted,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image)
{
  auto payloads = prepare_incoming_payloads(request.plan(), image);
  auto captures = prepare_old_object_captures(request.plan(), request.control());
  auto schedule = prepare_application_schedule(
      request.plan(), image, payloads, captures);
  const bool has_recovery = !captures.requests().empty();
  return publish_initial_journal(
      std::move(admitted), request, state, lease, std::move(payloads),
      std::move(captures), std::move(schedule), has_recovery,
      has_active_effect(request.plan()), has_rejected_effect(request.plan()));
}

journaled_application
journal_application_engine(
    admitted_application admitted,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  auto captures = prepare_old_object_captures(request.plan(), request.control());
  auto schedule = prepare_application_schedule(request.plan(), captures);
  const bool has_recovery = !captures.requests().empty();
  return publish_initial_journal(
      std::move(admitted), request, state, lease, std::nullopt,
      std::move(captures), std::move(schedule), has_recovery,
      has_active_effect(request.plan()),
      has_rejected_effect(request.plan()));
}

} // namespace pkgapply::detail
