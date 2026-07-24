// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_engine.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
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
[[nodiscard]] reopened_application
finish_restart(const Request& request,
               const lease_bound_state_projection& state,
               target_mutation_lease& lease,
               application_backend& backend,
               const application_journal_record& journal,
               std::unique_ptr<application_backend_transaction> transaction)
{
  if (!transaction)
    throw std::logic_error("application backend returned no reopened transaction");

  validate_restarted_backend_transaction(
      request.target(), lease, backend, journal, *transaction);
  validate_target_mutation_lease(request.target(), state, lease);

  application_attempt attempt = application_attempt::make(
      request.identity(),
      request.target().identity(),
      backend.identity(),
      transaction->attempt_nonce());
  if (attempt.identity() != journal.header().attempt().identity()) {
    throw std::logic_error(
        "restarted transaction did not reproduce the durable attempt");
  }

  application_restart_checkpoint checkpoint =
      transaction->restart_checkpoint(journal);
  if (checkpoint.journal() != journal.identity())
    throw std::logic_error("backend restart checkpoint names another journal");

  return reopened_application(
      std::move(attempt),
      assess_application_restart(journal),
      journal,
      std::move(checkpoint),
      std::move(transaction));
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

reopened_application::reopened_application(
    application_attempt attempt,
    application_restart_assessment assessment,
    application_journal_record journal,
    application_restart_checkpoint checkpoint,
    std::unique_ptr<application_backend_transaction> transaction)
    : attempt_(std::move(attempt)),
      assessment_(std::move(assessment)),
      journal_(std::move(journal)),
      checkpoint_(std::move(checkpoint)),
      transaction_(std::move(transaction))
{
  if (!transaction_)
    throw std::invalid_argument("reopened application requires a transaction");
  if (!assessment_.resumable())
    throw std::invalid_argument("reopened application journal is not resumable");
  if (assessment_.journal() != journal_.identity())
    throw std::invalid_argument("restart assessment names another journal");
  if (attempt_.identity() != journal_.header().attempt().identity())
    throw std::invalid_argument("reopened application names another attempt");
  if (checkpoint_.journal() != journal_.identity())
    throw std::invalid_argument("reopened checkpoint names another journal");
}

const application_attempt&
reopened_application::attempt() const noexcept
{
  return attempt_;
}

const application_restart_assessment&
reopened_application::assessment() const noexcept
{
  return assessment_;
}

const application_journal_record&
reopened_application::journal() const noexcept
{
  return journal_;
}

const application_restart_checkpoint&
reopened_application::checkpoint() const noexcept
{
  return checkpoint_;
}

application_backend_transaction&
reopened_application::transaction() noexcept
{
  return *transaction_;
}

const application_backend_transaction&
reopened_application::transaction() const noexcept
{
  return *transaction_;
}

reopened_application
reopen_application_engine(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive)
{
  validate_application_restart(
      request, state, lease, backend, journal, archive);
  return finish_restart(
      request,
      state,
      lease,
      backend,
      journal,
      backend.resume_with_incoming_image(
          request.target(), lease, journal, archive.image()));
}

reopened_application
reopen_application_engine(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive)
{
  validate_application_restart(
      request, state, lease, backend, journal, archive);
  return finish_restart(
      request,
      state,
      lease,
      backend,
      journal,
      backend.resume_with_incoming_image(
          request.target(), lease, journal, archive.image()));
}

reopened_application
reopen_application_engine(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const application_journal_record& journal)
{
  validate_application_restart(request, state, lease, backend, journal);
  return finish_restart(
      request,
      state,
      lease,
      backend,
      journal,
      backend.resume_without_incoming_image(
          request.target(), lease, journal));
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
                bool has_rejected,
                bool synchronize_application_domains)
{
  std::vector<application_journal_effect> effects;
  const std::size_t active_count = static_cast<std::size_t>(std::count_if(
      schedule.steps().begin(), schedule.steps().end(),
      [](const auto& step) {
        return step.kind() ==
            application_effect_step_kind::publish_active_object;
      }));
  effects.reserve(schedule.steps().size() + active_count + 8);
  for (const auto& step : schedule.steps()) {
    effects.push_back(application_journal_effect::make(
        effects.size(), journal_kind(step.kind()), step.path()));
  }

  // Recovery is a distinct, optional branch of the frozen effect graph.  Its
  // path order is the exact reverse of active execution so parent/child and
  // hard-link dependencies are undone without backend reinterpretation.
  for (auto step = schedule.steps().rbegin();
       step != schedule.steps().rend(); ++step) {
    if (step->kind() !=
        application_effect_step_kind::publish_active_object)
      continue;
    effects.push_back(application_journal_effect::make(
        effects.size(),
        application_journal_effect_kind::recover_active_object,
        step->path()));
  }

  const auto append = [&effects](application_journal_effect_kind kind) {
    effects.push_back(application_journal_effect::make(effects.size(), kind));
  };
  append(application_journal_effect_kind::synchronize_journal);
  if (has_incoming)
    append(application_journal_effect_kind::synchronize_incoming_staging);
  if (has_recovery)
    append(application_journal_effect_kind::synchronize_recovery_staging);
  if (has_active && synchronize_application_domains) {
    append(application_journal_effect_kind::synchronize_active_namespace);
    append(application_journal_effect_kind::synchronize_recovered_namespace);
  }
  if (has_rejected && synchronize_application_domains)
    append(application_journal_effect_kind::synchronize_rejected_store);
  append(application_journal_effect_kind::publish_completed_evidence);
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
      journal_effects(
          schedule, payloads.has_value() &&
              payloads->selection().size() != 0,
          has_recovery, has_active, has_rejected,
          request.control().durability() ==
              application_durability_requirement::all_application_domains),
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

void
journaled_application::advance_journal(application_journal_record journal)
{
  if (journal.header().identity() != journal_.header().identity() ||
      journal.effects().size() != journal_.effects().size())
  {
    throw std::invalid_argument(
        "advanced application journal changed its durable authority");
  }
  for (std::size_t index = 0; index < journal.effects().size(); ++index) {
    if (journal.effects()[index].identity() !=
        journal_.effects()[index].identity())
    {
      throw std::invalid_argument(
          "advanced application journal changed its effect graph");
    }
  }
  journal_ = std::move(journal);
}

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

namespace pkgapply::detail {
namespace {

void
append_unique_evidence(
    std::vector<application_backend_evidence_identity>& target,
    const std::vector<application_backend_evidence_identity>& source)
{
  target.insert(target.end(), source.begin(), source.end());
  std::sort(target.begin(), target.end());
  target.erase(std::unique(target.begin(), target.end()), target.end());
}

bool
same_observation(const application_path_observation& lhs,
                 const application_path_observation& rhs) noexcept
{
  return lhs.path() == rhs.path() &&
         lhs.state() == rhs.state() &&
         lhs.object() == rhs.object();
}

bool
same_effect_graph(const application_journal_record& lhs,
                  const application_journal_record& rhs) noexcept
{
  if (lhs.effects().size() != rhs.effects().size())
    return false;
  for (std::size_t index = 0; index < lhs.effects().size(); ++index) {
    if (lhs.effects()[index].identity() != rhs.effects()[index].identity())
      return false;
  }
  return true;
}

const application_journal_effect&
find_effect(const application_journal_record& journal,
            application_journal_effect_kind kind,
            const pkgplan::package_path* path)
{
  const application_journal_effect* found = nullptr;
  for (const auto& effect : journal.effects()) {
    const bool path_matches = path == nullptr
        ? !effect.path().has_value()
        : effect.path().has_value() && *effect.path() == *path;
    if (effect.kind() != kind || !path_matches)
      continue;
    if (found != nullptr)
      throw std::logic_error("application journal effect is not unique");
    found = &effect;
  }
  if (found == nullptr)
    throw std::logic_error("application journal lacks required effect");
  return *found;
}

const application_journal_effect&
find_effect(const application_journal_record& journal,
            application_journal_effect_kind kind)
{
  return find_effect(journal, kind, nullptr);
}

const application_journal_effect&
find_effect(const application_journal_record& journal,
            application_journal_effect_kind kind,
            const pkgplan::package_path& path)
{
  return find_effect(journal, kind, &path);
}

application_journal_event_kind
terminal_event(backend_operation_outcome outcome)
{
  switch (outcome) {
    case backend_operation_outcome::completed:
      return application_journal_event_kind::completed;
    case backend_operation_outcome::failed:
      return application_journal_event_kind::failed;
    case backend_operation_outcome::indeterminate:
      return application_journal_event_kind::indeterminate;
    case backend_operation_outcome::conditional_retained:
      throw std::logic_error(
          "backend returned an unsupported conditional outcome");
  }
  throw std::logic_error("invalid backend operation outcome");
}

application_journal_event_kind
terminal_event(application_durability_status status)
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
      throw std::logic_error(
          "backend synchronization returned not-attempted status");
  }
  throw std::logic_error("invalid preparation durability status");
}

void
publish_snapshot(
    journaled_application& application,
    application_journal_state state,
    std::vector<application_journal_event> events,
    std::optional<application_receipt_identity> receipt = std::nullopt,
    std::optional<completed_application_evidence_identity>
        completed_evidence = std::nullopt)
{
  application_journal_record intended = application_journal_record::make(
      application.journal().header(), state, application.journal().effects(),
      std::move(events), std::move(receipt), std::move(completed_evidence));
  application_journal_record durable =
      application.admitted().transaction().publish_journal(intended);
  if (durable.identity() != intended.identity() ||
      durable.header().identity() != intended.header().identity() ||
      !same_effect_graph(durable, intended))
  {
    throw std::logic_error("backend changed the application journal snapshot");
  }
  application.advance_journal(std::move(durable));
}

void
publish_event(
    journaled_application& application,
    application_journal_state state,
    application_journal_effect_identity effect,
    application_journal_event_kind kind,
    std::vector<application_backend_evidence_identity> evidence = {})
{
  std::vector<application_journal_event> events =
      application.journal().events();
  events.emplace_back(
      static_cast<std::uint64_t>(events.size()), kind, std::move(effect),
      std::move(evidence));
  publish_snapshot(application, state, std::move(events));
}

application_durability_profile
preparation_durability(
    application_durability_status journal,
    application_durability_status incoming,
    application_durability_status recovery)
{
  return application_durability_profile({
      {application_durability_domain::journal, journal},
      {application_durability_domain::incoming_staging, incoming},
      {application_durability_domain::recovery_staging, recovery},
      {application_durability_domain::active_namespace,
       application_durability_status::not_attempted},
      {application_durability_domain::rejected_object_store,
       application_durability_status::not_attempted},
      {application_durability_domain::completed_evidence,
       application_durability_status::not_attempted},
  });
}

template<class Request>
void
validate_preparation_binding(
    const journaled_application& application,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_target_mutation_lease(request.target(), state, lease);
  const auto& header = application.journal().header();
  if (application.journal().state() != application_journal_state::preparing ||
      header.request() != request.identity() ||
      header.plan() != request.plan().identity() ||
      header.target() != request.target().identity() ||
      header.control() != request.control().identity() ||
      header.state_projection() != state.identity() ||
      header.lease() != lease.identity() ||
      header.attempt().identity() !=
          application.admitted().attempt().identity())
  {
    throw std::invalid_argument(
        "application preparation inputs differ from durable admission");
  }
}

template<class Request>
void
validate_preparation_archive(
    const journaled_application& application,
    const Request& request,
    const pkgimage::package_archive& archive)
{
  if (!application.payloads())
    throw std::logic_error("incoming application lacks payload authority");
  const auto& incoming = request.plan().preconditions().incoming_archive();
  if (!incoming)
    throw std::logic_error("incoming application lacks archive precondition");
  const auto& image = archive.image();
  const auto& receipt = archive.inspection_receipt();
  if (image.identity() != application.payloads()->image() ||
      image.identity() != incoming->image() ||
      receipt.image_identity() != image.identity() ||
      receipt.entry_count() != image.size() ||
      receipt.archive_digest() != incoming->archive() ||
      receipt.identity() != incoming->inspection_receipt())
  {
    throw std::invalid_argument(
        "application preparation archive differs from admitted authority");
  }
  application.payloads()->selection().validate(image);
}

template<class Request>
application_engine_preparation
fail_preparation(
    journaled_application application,
    const Request& request,
    const lease_bound_state_projection& state,
    application_durability_status journal,
    application_durability_status incoming,
    application_durability_status recovery,
    std::vector<application_backend_evidence_identity> evidence)
{
  application_receipt receipt = application_receipt::failed(
      request,
      application.admitted().attempt().identity(),
      state.identity(),
      application_attempt_outcome::failed_before_target_mutation,
      application_recovery_state::unchanged,
      preparation_durability(journal, incoming, recovery),
      {},
      application.journal().header().identity(),
      std::move(evidence));

  publish_snapshot(
      application,
      application_journal_state::abandoned,
      application.journal().events(),
      receipt.identity());
  return application_engine_preparation::failed(std::move(receipt));
}

struct capture_preparation_result final {
  std::vector<old_object_capture_result> captures;
  application_durability_status durability =
      application_durability_status::not_attempted;
  bool completed = true;
};

capture_preparation_result
capture_old_objects(
    journaled_application& application,
    application_recovery_requirement recovery_requirement,
    std::vector<application_backend_evidence_identity>& evidence)
{
  capture_preparation_result result;
  result.captures.reserve(application.captures().requests().size());

  for (const auto& request : application.captures().requests()) {
    const application_journal_effect_identity effect = find_effect(
        application.journal(),
        application_journal_effect_kind::capture_old_object,
        request.path()).identity();
    publish_event(
        application, application_journal_state::preparing, effect,
        application_journal_event_kind::intent);

    old_object_capture_result captured =
        application.admitted().transaction().capture_old(request);
    append_unique_evidence(evidence, captured.evidence());

    const application_path_observation* observed =
        application.admitted().preconditions().observations().find(
            request.path());
    if (observed == nullptr)
      throw std::logic_error("old-object capture lacks admitted observation");
    if (captured.captured().path() != request.path())
      throw std::logic_error("backend captured another logical path");
    if (captured.outcome() == backend_operation_outcome::completed &&
        !same_observation(captured.captured(), *observed))
    {
      throw std::logic_error(
          "backend capture differs from the admitted live object");
    }

    const backend_operation_outcome backend_outcome = captured.outcome();
    if (backend_outcome == backend_operation_outcome::completed)
      result.durability = application_durability_status::visible;

    backend_operation_outcome semantic_outcome = backend_outcome;
    if (semantic_outcome == backend_operation_outcome::completed &&
        request.for_recovery() &&
        recovery_requirement ==
            application_recovery_requirement::exact_prior_state &&
        !captured.exact_recovery_possible())
    {
      semantic_outcome = backend_operation_outcome::failed;
    }

    publish_event(
        application, application_journal_state::preparing, effect,
        terminal_event(semantic_outcome), captured.evidence());
    result.captures.push_back(std::move(captured));

    if (semantic_outcome != backend_operation_outcome::completed) {
      if (backend_outcome != backend_operation_outcome::completed)
        result.durability = application_durability_status::indeterminate;
      result.completed = false;
      return result;
    }
  }
  return result;
}

struct payload_preparation_result final {
  application_durability_status durability =
      application_durability_status::not_attempted;
  bool completed = true;
};

payload_preparation_result
stage_incoming_payloads(
    journaled_application& application,
    const pkgimage::package_archive& archive,
    std::vector<application_backend_evidence_identity>& evidence)
{
  payload_preparation_result result;
  if (!application.payloads() ||
      application.payloads()->selection().size() == 0)
  {
    return result;
  }

  std::vector<pkgplan::package_path> effect_paths;
  for (const auto& step : application.schedule().steps()) {
    if (step.kind() != application_effect_step_kind::stage_regular_payload)
      continue;
    const application_journal_effect_identity effect = find_effect(
        application.journal(),
        application_journal_effect_kind::stage_incoming_payload,
        step.path()).identity();
    publish_event(
        application, application_journal_state::preparing, effect,
        application_journal_event_kind::intent);
    effect_paths.push_back(step.path());
  }

  if (effect_paths.size() != application.payloads()->selection().size())
    throw std::logic_error("payload journal effect closure mismatch");

  std::unique_ptr<incoming_payload_stage> stage =
      application.admitted().transaction().begin_payload_stage(
          archive.image(), application.payloads()->selection());
  if (!stage)
    throw std::logic_error("application backend returned no payload stage");

  archive.replay(application.payloads()->selection(), *stage);
  backend_operation_result sealed = stage->seal();
  append_unique_evidence(evidence, sealed.evidence());
  if ((sealed.outcome() == backend_operation_outcome::completed) !=
      stage->sealed())
  {
    throw std::logic_error("payload stage seal state contradicts outcome");
  }

  const auto terminal = terminal_event(sealed.outcome());
  for (const auto& path : effect_paths) {
    const application_journal_effect_identity effect = find_effect(
        application.journal(),
        application_journal_effect_kind::stage_incoming_payload, path).identity();
    publish_event(
        application, application_journal_state::preparing, effect,
        terminal, sealed.evidence());
  }

  if (sealed.outcome() != backend_operation_outcome::completed) {
    stage->abandon();
    result.durability = application_durability_status::indeterminate;
    result.completed = false;
    return result;
  }

  result.durability = application_durability_status::visible;
  return result;
}

bool
synchronize_preparation_domain(
    journaled_application& application,
    application_durability_domain domain,
    application_journal_effect_kind kind,
    application_durability_status& status)
{
  const application_journal_effect_identity effect =
      find_effect(application.journal(), kind).identity();
  publish_event(
      application, application_journal_state::preparing, effect,
      application_journal_event_kind::intent);
  application_durability_fact fact =
      application.admitted().transaction().synchronize(domain);
  if (fact.domain() != domain)
    throw std::logic_error("backend synchronized another durability domain");
  status = fact.status();
  publish_event(
      application, application_journal_state::preparing, effect,
      terminal_event(status));
  return status == application_durability_status::confirmed;
}

template<class Request>
application_engine_preparation
finish_preparation(
    journaled_application application,
    const Request& request,
    const lease_bound_state_projection& state,
    std::vector<old_object_capture_result> captures,
    application_durability_status incoming,
    application_durability_status recovery,
    std::vector<application_backend_evidence_identity> evidence)
{
  application_durability_status journal =
      application_durability_status::not_attempted;
  if (!synchronize_preparation_domain(
          application,
          application_durability_domain::journal,
          application_journal_effect_kind::synchronize_journal,
          journal))
  {
    return fail_preparation(
        std::move(application), request, state, journal, incoming, recovery,
        std::move(evidence));
  }

  publish_snapshot(
      application,
      application_journal_state::prepared,
      application.journal().events());
  return application_engine_preparation::prepared(
      std::move(application), std::move(captures),
      preparation_durability(journal, incoming, recovery),
      std::move(evidence));
}

template<class Request>
application_engine_preparation
prepare_with_archive(
    journaled_application application,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive)
{
  validate_preparation_binding(application, request, state, lease);
  validate_preparation_archive(application, request, archive);

  std::vector<application_backend_evidence_identity> evidence =
      application.admitted().preconditions().observations().evidence();
  auto captures = capture_old_objects(
      application, request.control().recovery(), evidence);
  if (!captures.completed) {
    return fail_preparation(
        std::move(application), request, state,
        application_durability_status::confirmed,
        application_durability_status::not_attempted,
        captures.durability, std::move(evidence));
  }

  auto payload = stage_incoming_payloads(application, archive, evidence);
  if (!payload.completed) {
    return fail_preparation(
        std::move(application), request, state,
        application_durability_status::confirmed, payload.durability,
        captures.durability, std::move(evidence));
  }

  if (payload.durability != application_durability_status::not_attempted &&
      !synchronize_preparation_domain(
          application,
          application_durability_domain::incoming_staging,
          application_journal_effect_kind::synchronize_incoming_staging,
          payload.durability))
  {
    return fail_preparation(
        std::move(application), request, state,
        application_durability_status::confirmed, payload.durability,
        captures.durability, std::move(evidence));
  }

  if (captures.durability != application_durability_status::not_attempted &&
      !synchronize_preparation_domain(
          application,
          application_durability_domain::recovery_staging,
          application_journal_effect_kind::synchronize_recovery_staging,
          captures.durability))
  {
    return fail_preparation(
        std::move(application), request, state,
        application_durability_status::confirmed, payload.durability,
        captures.durability, std::move(evidence));
  }

  return finish_preparation(
      std::move(application), request, state, std::move(captures.captures),
      payload.durability, captures.durability, std::move(evidence));
}

} // namespace

prepared_application::prepared_application(
    journaled_application journaled,
    std::vector<old_object_capture_result> captures,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : journaled_(std::move(journaled)), captures_(std::move(captures)),
      durability_(std::move(durability)),
      backend_evidence_(std::move(backend_evidence))
{
  if (journaled_.journal().state() != application_journal_state::prepared)
    throw std::invalid_argument("prepared application journal is not prepared");
  if (captures_.size() != journaled_.captures().requests().size())
    throw std::invalid_argument("prepared application capture closure mismatch");
  if (std::any_of(
          captures_.begin(), captures_.end(),
          [](const auto& capture) {
            return capture.outcome() != backend_operation_outcome::completed;
          }))
  {
    throw std::invalid_argument(
        "prepared application contains an incomplete old-object capture");
  }

  const bool has_payloads = journaled_.payloads().has_value() &&
      journaled_.payloads()->selection().size() != 0;
  const bool has_captures = !captures_.empty();
  if (durability_.status(application_durability_domain::journal) !=
          application_durability_status::confirmed ||
      durability_.status(application_durability_domain::incoming_staging) !=
          (has_payloads ? application_durability_status::confirmed
                        : application_durability_status::not_attempted) ||
      durability_.status(application_durability_domain::recovery_staging) !=
          (has_captures ? application_durability_status::confirmed
                        : application_durability_status::not_attempted) ||
      durability_.status(application_durability_domain::active_namespace) !=
          application_durability_status::not_attempted ||
      durability_.status(application_durability_domain::rejected_object_store) !=
          application_durability_status::not_attempted ||
      durability_.status(application_durability_domain::completed_evidence) !=
          application_durability_status::not_attempted)
  {
    throw std::invalid_argument(
        "prepared application has an invalid durability boundary");
  }

  std::sort(backend_evidence_.begin(), backend_evidence_.end());
  if (std::adjacent_find(
          backend_evidence_.begin(), backend_evidence_.end()) !=
      backend_evidence_.end())
  {
    throw std::invalid_argument("duplicate prepared backend evidence");
  }
}

journaled_application& prepared_application::journaled() noexcept
{ return journaled_; }
const journaled_application& prepared_application::journaled() const noexcept
{ return journaled_; }
const std::vector<old_object_capture_result>&
prepared_application::captures() const noexcept { return captures_; }
const application_durability_profile&
prepared_application::durability() const noexcept { return durability_; }
const std::vector<application_backend_evidence_identity>&
prepared_application::backend_evidence() const noexcept
{ return backend_evidence_; }

application_engine_preparation
application_engine_preparation::failed(application_receipt receipt)
{
  if (receipt.outcome() !=
      application_attempt_outcome::failed_before_target_mutation)
  {
    throw std::invalid_argument(
        "engine preparation failure is not pre-mutation");
  }
  return application_engine_preparation(value_type(std::move(receipt)));
}

application_engine_preparation
application_engine_preparation::prepared(
    journaled_application journaled,
    std::vector<old_object_capture_result> captures,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  return application_engine_preparation(value_type(
      std::in_place_type<prepared_application>, std::move(journaled),
      std::move(captures), std::move(durability),
      std::move(backend_evidence)));
}

application_engine_preparation::application_engine_preparation(value_type value)
    : value_(std::move(value))
{
}

bool application_engine_preparation::is_prepared() const noexcept
{ return std::holds_alternative<prepared_application>(value_); }
const application_receipt*
application_engine_preparation::failure() const noexcept
{ return std::get_if<application_receipt>(&value_); }
prepared_application* application_engine_preparation::prepared() noexcept
{ return std::get_if<prepared_application>(&value_); }
const prepared_application*
application_engine_preparation::prepared() const noexcept
{ return std::get_if<prepared_application>(&value_); }

application_engine_preparation
prepare_application_engine(
    journaled_application application,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive)
{
  return prepare_with_archive(
      std::move(application), request, state, lease, archive);
}

application_engine_preparation
prepare_application_engine(
    journaled_application application,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive)
{
  return prepare_with_archive(
      std::move(application), request, state, lease, archive);
}

application_engine_preparation
prepare_application_engine(
    journaled_application application,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_preparation_binding(application, request, state, lease);
  if (application.payloads())
    throw std::logic_error("removal preparation has incoming payloads");

  std::vector<application_backend_evidence_identity> evidence =
      application.admitted().preconditions().observations().evidence();
  auto captures = capture_old_objects(
      application, request.control().recovery(), evidence);
  if (!captures.completed) {
    return fail_preparation(
        std::move(application), request, state,
        application_durability_status::confirmed,
        application_durability_status::not_attempted,
        captures.durability, std::move(evidence));
  }

  if (captures.durability != application_durability_status::not_attempted &&
      !synchronize_preparation_domain(
          application,
          application_durability_domain::recovery_staging,
          application_journal_effect_kind::synchronize_recovery_staging,
          captures.durability))
  {
    return fail_preparation(
        std::move(application), request, state,
        application_durability_status::confirmed,
        application_durability_status::not_attempted,
        captures.durability, std::move(evidence));
  }

  return finish_preparation(
      std::move(application), request, state, std::move(captures.captures),
      application_durability_status::not_attempted, captures.durability,
      std::move(evidence));
}

} // namespace pkgapply::detail

namespace pkgapply::detail {
namespace {

application_effect_status
rejected_effect_status(backend_operation_outcome outcome)
{
  switch (outcome) {
    case backend_operation_outcome::completed:
      return application_effect_status::completed;
    case backend_operation_outcome::failed:
      return application_effect_status::failed;
    case backend_operation_outcome::indeterminate:
      return application_effect_status::indeterminate;
    case backend_operation_outcome::conditional_retained:
      throw std::logic_error(
          "rejected-object publication returned a conditional outcome");
  }
  throw std::logic_error("invalid rejected-object publication outcome");
}

application_path_role
rejected_path_role(pkgplan::installation_path_role role)
{
  switch (role) {
    case pkgplan::installation_path_role::incoming_entry:
      return application_path_role::incoming_entry;
    case pkgplan::installation_path_role::structural_parent:
      return application_path_role::structural_parent;
  }
  throw std::logic_error("invalid installation path role");
}

application_path_role
rejected_path_role(pkgplan::upgrade_path_role role)
{
  switch (role) {
    case pkgplan::upgrade_path_role::incoming_entry:
      return application_path_role::incoming_entry;
    case pkgplan::upgrade_path_role::obsolete_old_path:
      return application_path_role::obsolete_old_path;
    case pkgplan::upgrade_path_role::structural_parent:
      return application_path_role::structural_parent;
  }
  throw std::logic_error("invalid upgrade path role");
}

application_path_role
rejected_path_role(const pkgplan::removal_path_decision&)
{
  return application_path_role::installed_owned_path;
}

std::optional<pkgimage::entry_id>
rejected_incoming_entry(const pkgplan::removal_path_decision&)
{
  return std::nullopt;
}

template<class Decision>
std::optional<pkgimage::entry_id>
rejected_incoming_entry(const Decision& decision)
{
  return decision.incoming_entry();
}

template<class Plan>
const auto&
rejected_decision(const Plan& plan, const pkgplan::package_path& path)
{
  const auto item = std::lower_bound(
      plan.paths().begin(), plan.paths().end(), path,
      [](const auto& decision, const auto& wanted) {
        return decision.path() < wanted;
      });
  if (item == plan.paths().end() || item->path() != path)
    throw std::logic_error("rejected-object schedule path lacks a plan decision");
  return *item;
}

template<class Decision>
backend_rejected_effect_request
rejected_effect_request(const Decision& decision,
                        const application_effect_step& step)
{
  if (decision.path() != step.path())
    throw std::logic_error("rejected-object schedule decision path mismatch");

  const std::optional<pkgimage::entry_id> incoming =
      rejected_incoming_entry(decision);
  switch (decision.rejected()) {
    case pkgplan::planned_rejected_outcome::stage_incoming:
      if (!incoming || step.incoming_entry() != incoming)
        throw std::logic_error(
            "incoming rejected-object schedule binding mismatch");
      return backend_rejected_effect_request::stage_incoming(
          decision.path(), *incoming);

    case pkgplan::planned_rejected_outcome::stage_old:
      if (step.incoming_entry())
        throw std::logic_error(
            "old rejected-object schedule gained incoming authority");
      return backend_rejected_effect_request::stage_old(decision.path());

    case pkgplan::planned_rejected_outcome::none:
      throw std::logic_error(
          "rejected-object schedule cites a non-rejected plan path");
  }
  throw std::logic_error("invalid planned rejected-object outcome");
}

template<class Decision>
application_path_consequence
rejected_path_consequence(
    const Decision& decision,
    application_path_role role,
    const application_path_observation& before,
    const rejected_object_publication_result& result)
{
  return application_path_consequence(
      decision.path(), role, decision.active(), decision.rejected(),
      rejected_incoming_entry(decision), decision.ownership(),
      application_effect_status::not_attempted,
      rejected_effect_status(result.outcome()), before, before,
      result.record(), ownership_publication_status::ineligible);
}

template<class Plan>
std::vector<application_path_consequence>
rejected_path_consequences(
    const prepared_application& prepared,
    const Plan& plan,
    const std::vector<rejected_effect_application_result>& effects)
{
  std::vector<application_path_consequence> paths;
  std::vector<rejected_object_record_identity> records;
  paths.reserve(effects.size());
  records.reserve(effects.size());
  for (const auto& effect : effects) {
    const auto& decision = rejected_decision(plan, effect.request().path());
    const application_path_observation* before =
        prepared.journaled().admitted().preconditions().observations().find(
            effect.request().path());
    if (before == nullptr)
      throw std::logic_error(
          "rejected-object result lacks admitted path observation");

    if (effect.result().record())
      records.push_back(*effect.result().record());

    if constexpr (std::is_same_v<
                      std::decay_t<decltype(decision)>,
                      pkgplan::removal_path_decision>)
    {
      paths.push_back(rejected_path_consequence(
          decision, rejected_path_role(decision), *before, effect.result()));
    }
    else
    {
      paths.push_back(rejected_path_consequence(
          decision, rejected_path_role(decision.role()), *before,
          effect.result()));
    }
  }

  std::sort(records.begin(), records.end());
  if (std::adjacent_find(records.begin(), records.end()) != records.end())
    throw std::logic_error(
        "backend reused one rejected record for multiple path effects");
  return paths;
}

application_durability_profile
with_rejected_durability(
    const application_durability_profile& prepared,
    application_durability_status rejected)
{
  std::vector<application_durability_fact> facts;
  facts.reserve(prepared.facts().size());
  for (const auto& fact : prepared.facts()) {
    facts.emplace_back(
        fact.domain(),
        fact.domain() == application_durability_domain::rejected_object_store
            ? rejected
            : fact.status());
  }
  return application_durability_profile(std::move(facts));
}

bool
requires_rejected_store_synchronization(
    const application_execution_control& control)
{
  switch (control.durability()) {
    case application_durability_requirement::visibility_only:
    case application_durability_requirement::journal_and_recovery:
      return false;
    case application_durability_requirement::all_application_domains:
      return true;
  }
  throw std::logic_error("invalid application durability requirement");
}

void
validate_rejected_source(
    const prepared_application& prepared,
    const backend_rejected_effect_request& request)
{
  switch (request.outcome()) {
    case pkgplan::planned_rejected_outcome::stage_incoming: {
      if (!request.incoming_entry() || !prepared.journaled().payloads())
        throw std::logic_error(
            "incoming rejected object lacks prepared image authority");
      const auto& requirements =
          prepared.journaled().payloads()->requirements();
      const auto item = std::find_if(
          requirements.begin(), requirements.end(),
          [&request](const auto& requirement) {
            return requirement.path() == request.path();
          });
      if (item == requirements.end() ||
          !item->required_for_rejected() ||
          item->image_entry() != *request.incoming_entry())
      {
        throw std::logic_error(
            "incoming rejected object differs from prepared payload authority");
      }
      return;
    }

    case pkgplan::planned_rejected_outcome::stage_old: {
      const old_object_capture_request* capture =
          prepared.journaled().captures().find(request.path());
      if (capture == nullptr || !capture->for_rejected_object())
        throw std::logic_error(
            "old rejected object lacks rejected capture authority");
      const auto captured = std::find_if(
          prepared.captures().begin(), prepared.captures().end(),
          [&request](const auto& result) {
            return result.captured().path() == request.path();
          });
      if (captured == prepared.captures().end() ||
          captured->outcome() != backend_operation_outcome::completed)
      {
        throw std::logic_error(
            "old rejected object lacks completed capture evidence");
      }
      return;
    }

    case pkgplan::planned_rejected_outcome::none:
      break;
  }
  throw std::logic_error("invalid rejected-object source outcome");
}

template<class Request>
void
validate_rejected_publication_binding(
    const prepared_application& prepared,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_target_mutation_lease(request.target(), state, lease);
  const auto& journal = prepared.journaled().journal();
  const auto& header = journal.header();
  if (journal.state() != application_journal_state::prepared ||
      header.request() != request.identity() ||
      header.plan() != request.plan().identity() ||
      header.target() != request.target().identity() ||
      header.control() != request.control().identity() ||
      header.state_projection() != state.identity() ||
      header.lease() != lease.identity() ||
      header.attempt().identity() !=
          prepared.journaled().admitted().attempt().identity())
  {
    throw std::invalid_argument(
        "rejected-object publication inputs differ from preparation");
  }
}

template<class Request>
application_engine_rejected_publication
fail_rejected_publication(
    prepared_application prepared,
    const Request& request,
    const lease_bound_state_projection& state,
    std::vector<rejected_effect_application_result> effects,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_status rejected_durability,
    std::vector<application_backend_evidence_identity> evidence)
{
  std::vector<application_path_consequence> paths =
      rejected_path_consequences(prepared, request.plan(), effects);
  application_durability_profile durability = with_rejected_durability(
      prepared.durability(), rejected_durability);
  application_receipt receipt = application_receipt::failed(
      request,
      prepared.journaled().admitted().attempt().identity(),
      state.identity(), outcome, recovery, std::move(durability),
      std::move(paths), prepared.journaled().journal().header().identity(),
      std::move(evidence));

  const bool rejected_effect_visible = std::any_of(
      effects.begin(), effects.end(),
      [](const auto& effect) {
        return effect.result().outcome() ==
            backend_operation_outcome::completed;
      });
  const application_journal_state journal_state =
      outcome == application_attempt_outcome::indeterminate
          ? application_journal_state::indeterminate
          : outcome ==
                    application_attempt_outcome::
                        effects_visible_durability_unconfirmed ||
                rejected_effect_visible
              ? application_journal_state::effects_visible
              : application_journal_state::abandoned;
  publish_snapshot(
      prepared.journaled(), journal_state,
      prepared.journaled().journal().events(), receipt.identity());
  return application_engine_rejected_publication::failed(std::move(receipt));
}

template<class Request>
application_engine_rejected_publication
publish_rejected(
    prepared_application prepared,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_rejected_publication_binding(prepared, request, state, lease);

  // The first mutating journal state is published before the first externally
  // visible rejected-store operation.  Active namespace effects remain absent.
  publish_snapshot(
      prepared.journaled(), application_journal_state::mutating,
      prepared.journaled().journal().events());

  std::vector<rejected_effect_application_result> effects;
  std::vector<application_backend_evidence_identity> evidence =
      prepared.backend_evidence();
  application_durability_status rejected_durability =
      application_durability_status::not_attempted;

  for (const auto& step : prepared.journaled().schedule().steps()) {
    if (step.kind() !=
        application_effect_step_kind::publish_rejected_object)
    {
      continue;
    }

    const auto& decision = rejected_decision(request.plan(), step.path());
    backend_rejected_effect_request command =
        rejected_effect_request(decision, step);
    validate_rejected_source(prepared, command);
    const application_journal_effect_identity effect = find_effect(
        prepared.journaled().journal(),
        application_journal_effect_kind::publish_rejected_object,
        step.path()).identity();
    publish_event(
        prepared.journaled(), application_journal_state::mutating, effect,
        application_journal_event_kind::intent);

    rejected_object_publication_result result =
        prepared.journaled().admitted().transaction().execute_rejected(command);
    append_unique_evidence(evidence, result.evidence());
    publish_event(
        prepared.journaled(), application_journal_state::mutating, effect,
        terminal_event(result.outcome()), result.evidence());

    const backend_operation_outcome result_outcome = result.outcome();
    effects.emplace_back(std::move(command), std::move(result));
    if (result_outcome == backend_operation_outcome::completed) {
      rejected_durability = application_durability_status::visible;
      continue;
    }

    if (result_outcome == backend_operation_outcome::indeterminate) {
      return fail_rejected_publication(
          std::move(prepared), request, state, std::move(effects),
          application_attempt_outcome::indeterminate,
          application_recovery_state::requires_authoritative_observation,
          application_durability_status::indeterminate,
          std::move(evidence));
    }

    return fail_rejected_publication(
        std::move(prepared), request, state, std::move(effects),
        application_attempt_outcome::failed_before_target_mutation,
        application_recovery_state::unchanged, rejected_durability,
        std::move(evidence));
  }

  if (!effects.empty() &&
      requires_rejected_store_synchronization(request.control()))
  {
    const application_journal_effect_identity synchronize_effect = find_effect(
        prepared.journaled().journal(),
        application_journal_effect_kind::synchronize_rejected_store).identity();
    publish_event(
        prepared.journaled(), application_journal_state::mutating,
        synchronize_effect, application_journal_event_kind::intent);
    const application_durability_fact synchronized =
        prepared.journaled().admitted().transaction().synchronize(
            application_durability_domain::rejected_object_store);
    if (synchronized.domain() !=
        application_durability_domain::rejected_object_store)
    {
      throw std::logic_error(
          "backend synchronized another rejected-object durability domain");
    }
    rejected_durability = synchronized.status();
    if (rejected_durability ==
        application_durability_status::not_attempted)
    {
      throw std::logic_error(
          "backend reported an unattempted rejected-store synchronization");
    }
    const application_journal_event_kind synchronization_event =
        rejected_durability == application_durability_status::confirmed
            ? application_journal_event_kind::completed
            : rejected_durability ==
                      application_durability_status::indeterminate
                  ? application_journal_event_kind::indeterminate
                  : application_journal_event_kind::failed;
    publish_event(
        prepared.journaled(), application_journal_state::mutating,
        synchronize_effect, synchronization_event);

    if (rejected_durability != application_durability_status::confirmed)
    {
      if (rejected_durability ==
          application_durability_status::indeterminate)
      {
        return fail_rejected_publication(
            std::move(prepared), request, state, std::move(effects),
            application_attempt_outcome::indeterminate,
            application_recovery_state::requires_authoritative_observation,
            rejected_durability, std::move(evidence));
      }
      return fail_rejected_publication(
          std::move(prepared), request, state, std::move(effects),
          application_attempt_outcome::
              effects_visible_durability_unconfirmed,
          application_recovery_state::recovery_assets_retained,
          rejected_durability, std::move(evidence));
    }
  }

  application_durability_profile durability = with_rejected_durability(
      prepared.durability(), rejected_durability);
  return application_engine_rejected_publication::published(
      std::move(prepared), std::move(effects), std::move(durability),
      std::move(evidence));
}

} // namespace

rejected_effect_application_result::rejected_effect_application_result(
    backend_rejected_effect_request request,
    rejected_object_publication_result result)
    : request_(std::move(request)), result_(std::move(result))
{
}

const backend_rejected_effect_request&
rejected_effect_application_result::request() const noexcept
{ return request_; }
const rejected_object_publication_result&
rejected_effect_application_result::result() const noexcept
{ return result_; }

rejected_published_application::rejected_published_application(
    prepared_application prepared,
    std::vector<rejected_effect_application_result> rejected_effects,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : prepared_(std::move(prepared)),
      rejected_effects_(std::move(rejected_effects)),
      durability_(std::move(durability)),
      backend_evidence_(std::move(backend_evidence))
{
  if (prepared_.journaled().journal().state() !=
      application_journal_state::mutating)
  {
    throw std::invalid_argument(
        "rejected-published application journal is not mutating");
  }

  std::vector<const application_effect_step*> expected_effects;
  for (const auto& step : prepared_.journaled().schedule().steps()) {
    if (step.kind() ==
        application_effect_step_kind::publish_rejected_object)
    {
      expected_effects.push_back(&step);
    }
  }
  if (rejected_effects_.size() != expected_effects.size())
    throw std::invalid_argument(
        "rejected-published application effect closure mismatch");

  std::vector<rejected_object_record_identity> records;
  records.reserve(rejected_effects_.size());
  for (std::size_t index = 0; index < rejected_effects_.size(); ++index) {
    const auto& expected = *expected_effects[index];
    const auto& effect = rejected_effects_[index];
    if (effect.request().path() != expected.path() ||
        effect.request().incoming_entry() != expected.incoming_entry())
    {
      throw std::invalid_argument(
          "rejected-published application effect order or source mismatch");
    }
    if (effect.result().outcome() !=
            backend_operation_outcome::completed ||
        !effect.result().record().has_value())
    {
      throw std::invalid_argument(
          "rejected-published application contains an incomplete effect");
    }
    records.push_back(*effect.result().record());
  }
  std::sort(records.begin(), records.end());
  if (std::adjacent_find(records.begin(), records.end()) != records.end())
    throw std::invalid_argument(
        "rejected-published application reused a rejected record");

  const application_durability_status expected_rejected =
      rejected_effects_.empty()
          ? application_durability_status::not_attempted
          : durability_.status(
                application_durability_domain::rejected_object_store);
  if ((!rejected_effects_.empty() &&
       expected_rejected != application_durability_status::visible &&
       expected_rejected != application_durability_status::confirmed) ||
      (rejected_effects_.empty() &&
       expected_rejected != application_durability_status::not_attempted) ||
      durability_.status(application_durability_domain::journal) !=
          prepared_.durability().status(
              application_durability_domain::journal) ||
      durability_.status(application_durability_domain::incoming_staging) !=
          prepared_.durability().status(
              application_durability_domain::incoming_staging) ||
      durability_.status(application_durability_domain::recovery_staging) !=
          prepared_.durability().status(
              application_durability_domain::recovery_staging) ||
      durability_.status(application_durability_domain::active_namespace) !=
          application_durability_status::not_attempted ||
      durability_.status(application_durability_domain::completed_evidence) !=
          application_durability_status::not_attempted)
  {
    throw std::invalid_argument(
        "rejected-published application has an invalid durability boundary");
  }

  std::sort(backend_evidence_.begin(), backend_evidence_.end());
  if (std::adjacent_find(
          backend_evidence_.begin(), backend_evidence_.end()) !=
      backend_evidence_.end())
  {
    throw std::invalid_argument(
        "duplicate rejected-publication backend evidence");
  }
}

prepared_application&
rejected_published_application::prepared() noexcept
{ return prepared_; }
const prepared_application&
rejected_published_application::prepared() const noexcept
{ return prepared_; }
const std::vector<rejected_effect_application_result>&
rejected_published_application::rejected_effects() const noexcept
{ return rejected_effects_; }
const application_durability_profile&
rejected_published_application::durability() const noexcept
{ return durability_; }
const std::vector<application_backend_evidence_identity>&
rejected_published_application::backend_evidence() const noexcept
{ return backend_evidence_; }

application_engine_rejected_publication
application_engine_rejected_publication::failed(application_receipt receipt)
{
  if (receipt.outcome() !=
          application_attempt_outcome::failed_before_target_mutation &&
      receipt.outcome() !=
          application_attempt_outcome::
              effects_visible_durability_unconfirmed &&
      receipt.outcome() != application_attempt_outcome::indeterminate)
  {
    throw std::invalid_argument(
        "rejected-publication failure has an invalid outcome");
  }
  return application_engine_rejected_publication(
      value_type(std::move(receipt)));
}

application_engine_rejected_publication
application_engine_rejected_publication::published(
    prepared_application prepared,
    std::vector<rejected_effect_application_result> rejected_effects,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  return application_engine_rejected_publication(value_type(
      std::in_place_type<rejected_published_application>,
      std::move(prepared), std::move(rejected_effects),
      std::move(durability), std::move(backend_evidence)));
}

application_engine_rejected_publication::
application_engine_rejected_publication(value_type value)
    : value_(std::move(value))
{
}

bool
application_engine_rejected_publication::is_published() const noexcept
{ return std::holds_alternative<rejected_published_application>(value_); }
const application_receipt*
application_engine_rejected_publication::failure() const noexcept
{ return std::get_if<application_receipt>(&value_); }
rejected_published_application*
application_engine_rejected_publication::published() noexcept
{ return std::get_if<rejected_published_application>(&value_); }
const rejected_published_application*
application_engine_rejected_publication::published() const noexcept
{ return std::get_if<rejected_published_application>(&value_); }

application_engine_rejected_publication
publish_rejected_application_engine(
    prepared_application prepared,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return publish_rejected(
      std::move(prepared), request, state, lease);
}

application_engine_rejected_publication
publish_rejected_application_engine(
    prepared_application prepared,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return publish_rejected(
      std::move(prepared), request, state, lease);
}

application_engine_rejected_publication
publish_rejected_application_engine(
    prepared_application prepared,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return publish_rejected(
      std::move(prepared), request, state, lease);
}

} // namespace pkgapply::detail

namespace pkgapply::detail {
namespace {

std::vector<const application_effect_step*>
active_schedule_steps(const rejected_published_application& rejected)
{
  std::vector<const application_effect_step*> steps;
  for (const auto& step :
       rejected.prepared().journaled().schedule().steps()) {
    if (step.kind() == application_effect_step_kind::publish_active_object)
      steps.push_back(&step);
  }
  return steps;
}

void
validate_active_effect_prefix(
    const rejected_published_application& rejected,
    const std::vector<active_effect_application_result>& effects,
    bool require_complete)
{
  const auto expected = active_schedule_steps(rejected);
  if ((require_complete && effects.size() != expected.size()) ||
      (!require_complete && effects.size() > expected.size()))
  {
    throw std::invalid_argument("active application effect closure mismatch");
  }

  for (std::size_t index = 0; index < effects.size(); ++index) {
    if (effects[index].request().path() != expected[index]->path() ||
        effects[index].request().incoming_entry() !=
            expected[index]->incoming_entry())
    {
      throw std::invalid_argument(
          "active application effect order or source mismatch");
    }
  }
}

void
validate_active_durability_base(
    const rejected_published_application& rejected,
    const application_durability_profile& durability)
{
  for (const auto& fact : rejected.durability().facts()) {
    if (fact.domain() == application_durability_domain::active_namespace)
      continue;
    if (durability.status(fact.domain()) != fact.status())
      throw std::invalid_argument(
          "active application changed an earlier durability domain");
  }
  if (durability.status(
          application_durability_domain::completed_evidence) !=
      application_durability_status::not_attempted)
  {
    throw std::invalid_argument(
        "active application invented completed-evidence durability");
  }
}

void
normalize_unique_evidence(
    std::vector<application_backend_evidence_identity>& evidence,
    const char* duplicate_message)
{
  std::sort(evidence.begin(), evidence.end());
  if (std::adjacent_find(evidence.begin(), evidence.end()) != evidence.end())
    throw std::invalid_argument(duplicate_message);
}

bool
all_active_effects_semantically_complete(
    const std::vector<active_effect_application_result>& effects)
{
  return std::all_of(
      effects.begin(), effects.end(), [](const auto& effect) {
        return effect.result().outcome() ==
                   backend_operation_outcome::completed ||
               effect.result().outcome() ==
                   backend_operation_outcome::conditional_retained;
      });
}

} // namespace

active_effect_application_result::active_effect_application_result(
    backend_active_effect_request request,
    backend_operation_result result)
    : request_(std::move(request)), result_(std::move(result))
{
  if (result_.outcome() ==
          backend_operation_outcome::conditional_retained &&
      request_.outcome() !=
          pkgplan::planned_active_outcome::remove_directory_if_empty)
  {
    throw std::invalid_argument(
        "conditional active retention requires directory cleanup");
  }
}

const backend_active_effect_request&
active_effect_application_result::request() const noexcept
{
  return request_;
}

const backend_operation_result&
active_effect_application_result::result() const noexcept
{
  return result_;
}

bool
active_effect_application_result::changed_target() const noexcept
{
  if (result_.outcome() != backend_operation_outcome::completed)
    return false;
  switch (request_.outcome()) {
    case pkgplan::planned_active_outcome::activate_incoming:
    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      return true;
    case pkgplan::planned_active_outcome::retain_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      return false;
  }
  return false;
}

active_mutated_application::active_mutated_application(
    rejected_published_application rejected,
    std::vector<active_effect_application_result> active_effects,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : rejected_(std::move(rejected)),
      active_effects_(std::move(active_effects)),
      durability_(std::move(durability)),
      backend_evidence_(std::move(backend_evidence))
{
  if (rejected_.prepared().journaled().journal().state() !=
      application_journal_state::effects_visible)
  {
    throw std::invalid_argument(
        "active-mutated application journal is not effects-visible");
  }
  validate_active_effect_prefix(rejected_, active_effects_, true);
  if (!all_active_effects_semantically_complete(active_effects_))
    throw std::invalid_argument(
        "active-mutated application contains an incomplete effect");

  validate_active_durability_base(rejected_, durability_);
  const bool changed = std::any_of(
      active_effects_.begin(), active_effects_.end(),
      [](const auto& effect) { return effect.changed_target(); });
  const application_durability_status active = durability_.status(
      application_durability_domain::active_namespace);
  if ((!changed && active != application_durability_status::not_attempted) ||
      (changed && active != application_durability_status::visible &&
       active != application_durability_status::confirmed))
  {
    throw std::invalid_argument(
        "active-mutated application has an invalid durability boundary");
  }

  normalize_unique_evidence(
      backend_evidence_, "duplicate active-application backend evidence");
}

rejected_published_application&
active_mutated_application::rejected() noexcept
{
  return rejected_;
}

const rejected_published_application&
active_mutated_application::rejected() const noexcept
{
  return rejected_;
}

std::vector<active_effect_application_result>&
active_mutated_application::active_effects() noexcept
{
  return active_effects_;
}

const std::vector<active_effect_application_result>&
active_mutated_application::active_effects() const noexcept
{
  return active_effects_;
}

const application_durability_profile&
active_mutated_application::durability() const noexcept
{
  return durability_;
}

std::vector<application_backend_evidence_identity>&
active_mutated_application::backend_evidence() noexcept
{
  return backend_evidence_;
}

const std::vector<application_backend_evidence_identity>&
active_mutated_application::backend_evidence() const noexcept
{
  return backend_evidence_;
}

active_interrupted_application::active_interrupted_application(
    rejected_published_application rejected,
    std::vector<active_effect_application_result> active_effects,
    active_execution_interruption interruption,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : rejected_(std::move(rejected)),
      active_effects_(std::move(active_effects)),
      interruption_(interruption),
      durability_(std::move(durability)),
      backend_evidence_(std::move(backend_evidence))
{
  const application_journal_state state =
      rejected_.prepared().journaled().journal().state();
  const bool indeterminate =
      interruption_ == active_execution_interruption::effect_indeterminate ||
      interruption_ ==
          active_execution_interruption::durability_indeterminate ||
      interruption_ ==
          active_execution_interruption::result_observation_indeterminate;
  if ((indeterminate && state != application_journal_state::indeterminate) ||
      (!indeterminate &&
       state != application_journal_state::recovery_pending))
  {
    throw std::invalid_argument(
        "interrupted active application has the wrong journal state");
  }

  const bool durability_interruption =
      interruption_ ==
          active_execution_interruption::durability_unconfirmed ||
      interruption_ ==
          active_execution_interruption::durability_indeterminate;
  const bool observation_interruption =
      interruption_ ==
          active_execution_interruption::result_observation_mismatch ||
      interruption_ ==
          active_execution_interruption::result_observation_indeterminate;
  validate_active_effect_prefix(
      rejected_, active_effects_,
      durability_interruption || observation_interruption);
  if (active_effects_.empty() && !durability_interruption &&
      !observation_interruption)
  {
    throw std::invalid_argument(
        "active effect interruption contains no attempted effect");
  }

  if (durability_interruption || observation_interruption) {
    if (!all_active_effects_semantically_complete(active_effects_))
      throw std::invalid_argument(
          "active durability interruption contains an incomplete effect");
  }
  else {
    const backend_operation_outcome terminal =
        active_effects_.back().result().outcome();
    const backend_operation_outcome expected =
        interruption_ == active_execution_interruption::effect_failed
            ? backend_operation_outcome::failed
            : backend_operation_outcome::indeterminate;
    if (terminal != expected)
      throw std::invalid_argument(
          "active effect interruption contradicts its terminal outcome");
  }

  validate_active_durability_base(rejected_, durability_);
  const application_durability_status active = durability_.status(
      application_durability_domain::active_namespace);
  switch (interruption_) {
    case active_execution_interruption::effect_failed:
      if (active != application_durability_status::not_attempted &&
          active != application_durability_status::visible)
      {
        throw std::invalid_argument(
            "failed active effect has an invalid durability state");
      }
      break;
    case active_execution_interruption::effect_indeterminate:
    case active_execution_interruption::durability_indeterminate:
      if (active != application_durability_status::indeterminate)
        throw std::invalid_argument(
            "indeterminate active execution lacks indeterminate durability");
      break;
    case active_execution_interruption::durability_unconfirmed:
      if (active != application_durability_status::unconfirmed)
        throw std::invalid_argument(
            "active durability failure lacks unconfirmed durability");
      break;
    case active_execution_interruption::result_observation_mismatch:
    case active_execution_interruption::result_observation_indeterminate:
      if (active != application_durability_status::not_attempted &&
          active != application_durability_status::visible &&
          active != application_durability_status::confirmed)
      {
        throw std::invalid_argument(
            "result observation interruption has invalid target durability");
      }
      break;
  }

  normalize_unique_evidence(
      backend_evidence_, "duplicate interrupted-active backend evidence");
}

rejected_published_application&
active_interrupted_application::rejected() noexcept
{
  return rejected_;
}

const rejected_published_application&
active_interrupted_application::rejected() const noexcept
{
  return rejected_;
}

const std::vector<active_effect_application_result>&
active_interrupted_application::active_effects() const noexcept
{
  return active_effects_;
}

active_execution_interruption
active_interrupted_application::interruption() const noexcept
{
  return interruption_;
}

const application_durability_profile&
active_interrupted_application::durability() const noexcept
{
  return durability_;
}

const std::vector<application_backend_evidence_identity>&
active_interrupted_application::backend_evidence() const noexcept
{
  return backend_evidence_;
}

application_engine_active_execution
application_engine_active_execution::complete(
    rejected_published_application rejected,
    std::vector<active_effect_application_result> active_effects,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  return application_engine_active_execution(value_type(
      std::in_place_type<active_mutated_application>,
      std::move(rejected), std::move(active_effects),
      std::move(durability), std::move(backend_evidence)));
}

application_engine_active_execution
application_engine_active_execution::interrupted(
    rejected_published_application rejected,
    std::vector<active_effect_application_result> active_effects,
    active_execution_interruption interruption,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  return application_engine_active_execution(value_type(
      std::in_place_type<active_interrupted_application>,
      std::move(rejected), std::move(active_effects), interruption,
      std::move(durability), std::move(backend_evidence)));
}

application_engine_active_execution::application_engine_active_execution(
    value_type value)
    : value_(std::move(value))
{
}

bool
application_engine_active_execution::is_complete() const noexcept
{
  return std::holds_alternative<active_mutated_application>(value_);
}

active_mutated_application*
application_engine_active_execution::complete() noexcept
{
  return std::get_if<active_mutated_application>(&value_);
}

const active_mutated_application*
application_engine_active_execution::complete() const noexcept
{
  return std::get_if<active_mutated_application>(&value_);
}

active_interrupted_application*
application_engine_active_execution::interruption() noexcept
{
  return std::get_if<active_interrupted_application>(&value_);
}

const active_interrupted_application*
application_engine_active_execution::interruption() const noexcept
{
  return std::get_if<active_interrupted_application>(&value_);
}

} // namespace pkgapply::detail

namespace pkgapply::detail {
namespace {

template<class Plan>
const auto&
active_decision(const Plan& plan, const pkgplan::package_path& path)
{
  const auto item = std::lower_bound(
      plan.paths().begin(), plan.paths().end(), path,
      [](const auto& decision, const auto& wanted) {
        return decision.path() < wanted;
      });
  if (item == plan.paths().end() || item->path() != path)
    throw std::logic_error("active schedule path lacks a plan decision");
  return *item;
}

std::optional<pkgimage::entry_id>
active_incoming_entry(const pkgplan::removal_path_decision&)
{
  return std::nullopt;
}

template<class Decision>
std::optional<pkgimage::entry_id>
active_incoming_entry(const Decision& decision)
{
  return decision.incoming_entry();
}

template<class Decision>
backend_active_effect_request
active_effect_request(const Decision& decision,
                      const application_effect_step& step)
{
  if (decision.path() != step.path())
    throw std::logic_error("active schedule decision path mismatch");

  const std::optional<pkgimage::entry_id> incoming =
      active_incoming_entry(decision);
  switch (decision.active()) {
    case pkgplan::planned_active_outcome::activate_incoming:
      if (!incoming || step.incoming_entry() != incoming)
        throw std::logic_error("incoming active schedule binding mismatch");
      return backend_active_effect_request::make(
          decision.path(), decision.active(), *incoming);

    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      if (step.incoming_entry())
        throw std::logic_error(
            "destructive active effect gained incoming authority");
      return backend_active_effect_request::make(
          decision.path(), decision.active());

    case pkgplan::planned_active_outcome::retain_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      throw std::logic_error("active schedule cites a no-op plan path");
  }
  throw std::logic_error("invalid planned active outcome");
}

void
validate_active_source(const rejected_published_application& rejected,
                       const backend_active_effect_request& request)
{
  if (request.outcome() !=
      pkgplan::planned_active_outcome::activate_incoming)
  {
    if (request.incoming_entry())
      throw std::logic_error(
          "non-incoming active effect retained incoming authority");
    return;
  }

  if (!request.incoming_entry() ||
      !rejected.prepared().journaled().payloads())
  {
    throw std::logic_error(
        "incoming active object lacks prepared payload authority");
  }

  const auto& requirements =
      rejected.prepared().journaled().payloads()->requirements();
  const auto item = std::find_if(
      requirements.begin(), requirements.end(),
      [&request](const auto& requirement) {
        return requirement.path() == request.path();
      });
  if (item == requirements.end() || !item->required_for_active() ||
      item->image_entry() != *request.incoming_entry())
  {
    throw std::logic_error(
        "incoming active object differs from prepared payload authority");
  }
}

template<class Request>
void
validate_active_execution_binding(
    const rejected_published_application& rejected,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_target_mutation_lease(request.target(), state, lease);
  const auto& journal = rejected.prepared().journaled().journal();
  const auto& header = journal.header();
  if (journal.state() != application_journal_state::mutating ||
      header.request() != request.identity() ||
      header.plan() != request.plan().identity() ||
      header.target() != request.target().identity() ||
      header.control() != request.control().identity() ||
      header.state_projection() != state.identity() ||
      header.lease() != lease.identity() ||
      header.attempt().identity() !=
          rejected.prepared().journaled().admitted().attempt().identity())
  {
    throw std::invalid_argument(
        "active execution inputs differ from rejected publication");
  }
}

application_durability_profile
with_active_durability(const application_durability_profile& previous,
                       application_durability_status active)
{
  std::vector<application_durability_fact> facts;
  facts.reserve(previous.facts().size());
  for (const auto& fact : previous.facts()) {
    facts.emplace_back(
        fact.domain(),
        fact.domain() == application_durability_domain::active_namespace
            ? active
            : fact.status());
  }
  return application_durability_profile(std::move(facts));
}

bool
requires_active_namespace_synchronization(
    const application_execution_control& control)
{
  switch (control.durability()) {
    case application_durability_requirement::visibility_only:
    case application_durability_requirement::journal_and_recovery:
      return false;
    case application_durability_requirement::all_application_domains:
      return true;
  }
  throw std::logic_error("invalid application durability requirement");
}

application_journal_event_kind
active_terminal_event(backend_operation_outcome outcome)
{
  if (outcome == backend_operation_outcome::conditional_retained)
    return application_journal_event_kind::completed;
  return terminal_event(outcome);
}

application_engine_active_execution
interrupt_active_execution(
    rejected_published_application rejected,
    std::vector<active_effect_application_result> effects,
    active_execution_interruption interruption,
    application_durability_status active_durability,
    std::vector<application_backend_evidence_identity> evidence)
{
  const bool indeterminate =
      interruption == active_execution_interruption::effect_indeterminate ||
      interruption ==
          active_execution_interruption::durability_indeterminate;
  publish_snapshot(
      rejected.prepared().journaled(),
      indeterminate ? application_journal_state::indeterminate
                    : application_journal_state::recovery_pending,
      rejected.prepared().journaled().journal().events());

  application_durability_profile durability = with_active_durability(
      rejected.durability(), active_durability);
  return application_engine_active_execution::interrupted(
      std::move(rejected), std::move(effects), interruption,
      std::move(durability), std::move(evidence));
}

template<class Request>
application_engine_active_execution
execute_active(
    rejected_published_application rejected,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_active_execution_binding(rejected, request, state, lease);

  std::vector<active_effect_application_result> effects;
  std::vector<application_backend_evidence_identity> evidence =
      rejected.backend_evidence();
  application_durability_status active_durability =
      application_durability_status::not_attempted;

  for (const auto& step :
       rejected.prepared().journaled().schedule().steps()) {
    if (step.kind() != application_effect_step_kind::publish_active_object)
      continue;

    const auto& decision = active_decision(request.plan(), step.path());
    backend_active_effect_request command =
        active_effect_request(decision, step);
    validate_active_source(rejected, command);

    const application_journal_effect_identity effect = find_effect(
        rejected.prepared().journaled().journal(),
        application_journal_effect_kind::publish_active_object,
        step.path()).identity();
    publish_event(
        rejected.prepared().journaled(),
        application_journal_state::mutating, effect,
        application_journal_event_kind::intent);

    backend_operation_result result =
        rejected.prepared().journaled().admitted().transaction().
            execute_active(command);
    append_unique_evidence(evidence, result.evidence());

    const backend_operation_outcome outcome = result.outcome();
    active_effect_application_result applied(
        std::move(command), std::move(result));
    publish_event(
        rejected.prepared().journaled(),
        application_journal_state::mutating, effect,
        active_terminal_event(outcome), applied.result().evidence());
    effects.push_back(std::move(applied));

    if (effects.back().changed_target())
      active_durability = application_durability_status::visible;

    if (outcome == backend_operation_outcome::completed ||
        outcome == backend_operation_outcome::conditional_retained)
    {
      continue;
    }

    if (outcome == backend_operation_outcome::indeterminate) {
      return interrupt_active_execution(
          std::move(rejected), std::move(effects),
          active_execution_interruption::effect_indeterminate,
          application_durability_status::indeterminate,
          std::move(evidence));
    }

    return interrupt_active_execution(
        std::move(rejected), std::move(effects),
        active_execution_interruption::effect_failed,
        active_durability, std::move(evidence));
  }

  if (active_durability == application_durability_status::visible &&
      requires_active_namespace_synchronization(request.control()))
  {
    const application_journal_effect_identity synchronize_effect = find_effect(
        rejected.prepared().journaled().journal(),
        application_journal_effect_kind::synchronize_active_namespace).
            identity();
    publish_event(
        rejected.prepared().journaled(),
        application_journal_state::mutating, synchronize_effect,
        application_journal_event_kind::intent);

    const application_durability_fact synchronized =
        rejected.prepared().journaled().admitted().transaction().synchronize(
            application_durability_domain::active_namespace);
    if (synchronized.domain() !=
        application_durability_domain::active_namespace)
    {
      throw std::logic_error(
          "backend synchronized another active-namespace durability domain");
    }
    if (synchronized.status() ==
        application_durability_status::not_attempted)
    {
      throw std::logic_error(
          "backend reported an unattempted active-namespace synchronization");
    }

    publish_event(
        rejected.prepared().journaled(),
        application_journal_state::mutating, synchronize_effect,
        terminal_event(synchronized.status()));

    if (synchronized.status() !=
        application_durability_status::confirmed)
    {
      const bool indeterminate = synchronized.status() ==
          application_durability_status::indeterminate;
      return interrupt_active_execution(
          std::move(rejected), std::move(effects),
          indeterminate
              ? active_execution_interruption::durability_indeterminate
              : active_execution_interruption::durability_unconfirmed,
          indeterminate
              ? application_durability_status::indeterminate
              : application_durability_status::unconfirmed,
          std::move(evidence));
    }
    active_durability = application_durability_status::confirmed;
  }

  publish_snapshot(
      rejected.prepared().journaled(),
      application_journal_state::effects_visible,
      rejected.prepared().journaled().journal().events());
  application_durability_profile durability = with_active_durability(
      rejected.durability(), active_durability);
  return application_engine_active_execution::complete(
      std::move(rejected), std::move(effects), std::move(durability),
      std::move(evidence));
}

} // namespace

application_engine_active_execution
execute_active_application_engine(
    rejected_published_application rejected,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return execute_active(std::move(rejected), request, state, lease);
}

application_engine_active_execution
execute_active_application_engine(
    rejected_published_application rejected,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return execute_active(std::move(rejected), request, state, lease);
}

application_engine_active_execution
execute_active_application_engine(
    rejected_published_application rejected,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return execute_active(std::move(rejected), request, state, lease);
}

} // namespace pkgapply::detail

namespace pkgapply::detail {
namespace {

enum class result_observation_match : std::uint8_t {
  matched = 1,
  mismatch = 2,
  indeterminate = 3,
};

template<class Value>
result_observation_match
match_expected_fact(const qualified_fact<Value>& observed,
                    const Value& expected) noexcept
{
  switch (observed.state()) {
    case fact_state::known:
      return observed.value() && *observed.value() == expected
          ? result_observation_match::matched
          : result_observation_match::mismatch;
    case fact_state::unknown:
      return result_observation_match::indeterminate;
    case fact_state::not_applicable:
      return result_observation_match::mismatch;
  }
  return result_observation_match::indeterminate;
}

result_observation_match
merge_match(result_observation_match lhs,
            result_observation_match rhs) noexcept
{
  if (lhs == result_observation_match::mismatch ||
      rhs == result_observation_match::mismatch)
  {
    return result_observation_match::mismatch;
  }
  if (lhs == result_observation_match::indeterminate ||
      rhs == result_observation_match::indeterminate)
  {
    return result_observation_match::indeterminate;
  }
  return result_observation_match::matched;
}

template<class Digest>
result_observation_match
match_expected_digest(
    const qualified_fact<completed_regular_content_identity>& observed,
    const Digest& expected) noexcept
{
  switch (observed.state()) {
    case fact_state::known:
      if (!observed.value())
        return result_observation_match::mismatch;
      return observed.value()->bytes().size() == expected.bytes().size() &&
              std::equal(observed.value()->bytes().begin(),
                         observed.value()->bytes().end(),
                         expected.bytes().begin())
          ? result_observation_match::matched
          : result_observation_match::mismatch;
    case fact_state::unknown:
      return result_observation_match::indeterminate;
    case fact_state::not_applicable:
      return result_observation_match::mismatch;
  }
  return result_observation_match::indeterminate;
}

template<class Value>
result_observation_match
match_retained_fact(const qualified_fact<Value>& before,
                    const qualified_fact<Value>& after) noexcept
{
  switch (before.state()) {
    case fact_state::known:
      if (!before.value())
        return result_observation_match::mismatch;
      return match_expected_fact(after, *before.value());
    case fact_state::unknown:
      // The admitted observation did not constrain this applicable field.
      // Richer result observations are therefore accepted, while an unknown
      // result remains exactly as informative as the admitted fact.
      return after.state() == fact_state::not_applicable
          ? result_observation_match::mismatch
          : result_observation_match::matched;
    case fact_state::not_applicable:
      return after.state() == fact_state::not_applicable
          ? result_observation_match::matched
          : result_observation_match::mismatch;
  }
  return result_observation_match::indeterminate;
}

completed_object_kind
completed_kind(pkgimage::entry_type type)
{
  switch (type) {
    case pkgimage::entry_type::regular:
    case pkgimage::entry_type::hardlink:
      return completed_object_kind::regular;
    case pkgimage::entry_type::directory:
      return completed_object_kind::directory;
    case pkgimage::entry_type::symlink:
      return completed_object_kind::symlink;
    case pkgimage::entry_type::fifo:
      return completed_object_kind::fifo;
    case pkgimage::entry_type::character_device:
      return completed_object_kind::character_device;
    case pkgimage::entry_type::block_device:
      return completed_object_kind::block_device;
  }
  throw std::logic_error("invalid incoming package entry type");
}

result_observation_match
match_incoming_entry(const application_path_observation& observed,
                     const pkgimage::package_entry& entry)
{
  if (observed.state() == fact_state::unknown)
    return result_observation_match::indeterminate;
  if (observed.state() != fact_state::known || !observed.object())
    return result_observation_match::mismatch;

  const completed_object_fact& object = *observed.object();
  if (object.provenance() != object_fact_provenance::application_observation)
    throw std::logic_error(
        "backend result object lacks application-observation provenance");
  if (object.path().string() != entry.path.string() ||
      object.kind() != completed_kind(entry.type))
  {
    return result_observation_match::mismatch;
  }

  result_observation_match match = result_observation_match::matched;
  match = merge_match(match, match_expected_fact(object.mode(), entry.mode));
  match = merge_match(match, match_expected_fact(object.uid(), entry.uid));
  match = merge_match(match, match_expected_fact(object.gid(), entry.gid));
  match = merge_match(
      match,
      match_expected_fact(
          object.mtime(),
          completed_object_timestamp{entry.mtime, entry.mtime_nanoseconds}));

  switch (entry.type) {
    case pkgimage::entry_type::regular:
      if (!entry.regular_content)
        throw std::logic_error("regular image entry lacks content identity");
      match = merge_match(
          match, match_expected_fact(object.size(), entry.size));
      return merge_match(
          match,
          match_expected_digest(object.regular_content(),
                                *entry.regular_content));

    case pkgimage::entry_type::directory:
    case pkgimage::entry_type::fifo:
      return match;

    case pkgimage::entry_type::symlink:
      if (!entry.symlink_target)
        throw std::logic_error("symlink image entry lacks target");
      return merge_match(
          match,
          match_expected_fact(object.symlink_target(),
                              *entry.symlink_target));

    case pkgimage::entry_type::hardlink:
      if (!entry.hardlink_target)
        throw std::logic_error("hard-link image entry lacks anchor");
      if (object.hardlink().state() == fact_state::unknown)
        return merge_match(match, result_observation_match::indeterminate);
      if (object.hardlink().state() != fact_state::known ||
          !object.hardlink().value())
      {
        return result_observation_match::mismatch;
      }
      return merge_match(
          match,
          object.hardlink().value()->anchor().string() ==
                  entry.hardlink_target->string()
              ? result_observation_match::matched
              : result_observation_match::mismatch);

    case pkgimage::entry_type::character_device:
    case pkgimage::entry_type::block_device:
      if (!entry.device)
        throw std::logic_error("device image entry lacks device number");
      return merge_match(
          match,
          match_expected_fact(
              object.device(),
              completed_device_number{
                  entry.device->major, entry.device->minor}));
  }
  return result_observation_match::indeterminate;
}

result_observation_match
match_retained_observation(const application_path_observation& before,
                           const application_path_observation& after)
{
  if (before.path() != after.path())
    throw std::logic_error("result observation changed logical path");
  if (after.state() == fact_state::unknown)
    return result_observation_match::indeterminate;
  if (before.state() == fact_state::unknown)
    return result_observation_match::indeterminate;
  if (before.state() == fact_state::not_applicable) {
    return after.state() == fact_state::not_applicable
        ? result_observation_match::matched
        : result_observation_match::mismatch;
  }
  if (after.state() != fact_state::known || !before.object() ||
      !after.object())
  {
    return result_observation_match::mismatch;
  }

  const completed_object_fact& expected = *before.object();
  const completed_object_fact& observed = *after.object();
  if (observed.provenance() !=
      object_fact_provenance::application_observation)
  {
    throw std::logic_error(
        "backend result object lacks application-observation provenance");
  }
  if (expected.path() != observed.path() ||
      expected.kind() != observed.kind())
  {
    return result_observation_match::mismatch;
  }

  result_observation_match match = result_observation_match::matched;
  match = merge_match(
      match, match_retained_fact(expected.mode(), observed.mode()));
  match = merge_match(
      match, match_retained_fact(expected.uid(), observed.uid()));
  match = merge_match(
      match, match_retained_fact(expected.gid(), observed.gid()));
  match = merge_match(
      match, match_retained_fact(expected.size(), observed.size()));
  match = merge_match(
      match, match_retained_fact(expected.mtime(), observed.mtime()));
  match = merge_match(
      match,
      match_retained_fact(
          expected.regular_content(), observed.regular_content()));
  match = merge_match(
      match,
      match_retained_fact(
          expected.symlink_target(), observed.symlink_target()));
  match = merge_match(
      match, match_retained_fact(expected.device(), observed.device()));
  return merge_match(
      match, match_retained_fact(expected.hardlink(), observed.hardlink()));
}

const active_effect_application_result*
find_active_effect(const active_mutated_application& active,
                   const pkgplan::package_path& path) noexcept
{
  const auto item = std::find_if(
      active.active_effects().begin(), active.active_effects().end(),
      [&path](const auto& effect) {
        return effect.request().path() == path;
      });
  return item == active.active_effects().end() ? nullptr : &*item;
}

const rejected_effect_application_result*
find_rejected_effect(const active_mutated_application& active,
                     const pkgplan::package_path& path) noexcept
{
  const auto& effects = active.rejected().rejected_effects();
  const auto item = std::find_if(
      effects.begin(), effects.end(),
      [&path](const auto& effect) {
        return effect.request().path() == path;
      });
  return item == effects.end() ? nullptr : &*item;
}

application_effect_status
completion_effect_status(backend_operation_outcome outcome)
{
  switch (outcome) {
    case backend_operation_outcome::completed:
      return application_effect_status::completed;
    case backend_operation_outcome::conditional_retained:
      return application_effect_status::conditional_retained;
    case backend_operation_outcome::failed:
      return application_effect_status::failed;
    case backend_operation_outcome::indeterminate:
      return application_effect_status::indeterminate;
  }
  throw std::logic_error("invalid completion effect outcome");
}

template<class Decision>
application_path_role
completion_path_role(const Decision& decision)
{
  if constexpr (std::is_same_v<Decision, pkgplan::removal_path_decision>)
    return application_path_role::installed_owned_path;
  else
    return rejected_path_role(decision.role());
}

template<class Decision>
result_observation_match
match_result_observation(
    const active_mutated_application& active,
    const Decision& decision,
    const application_path_observation& before,
    const application_path_observation& after,
    const pkgimage::package_image* image)
{
  if (after.state() == fact_state::unknown)
    return result_observation_match::indeterminate;

  const active_effect_application_result* effect =
      find_active_effect(active, decision.path());
  switch (decision.active()) {
    case pkgplan::planned_active_outcome::activate_incoming: {
      const std::optional<pkgimage::entry_id> incoming =
          active_incoming_entry(decision);
      if (image == nullptr || !incoming)
        throw std::logic_error(
            "incoming result observation lacks image authority");
      const pkgimage::package_entry* entry = image->entry(*incoming);
      if (entry == nullptr)
        throw std::logic_error(
            "incoming result observation cites an absent image entry");
      return match_incoming_entry(after, *entry);
    }

    case pkgplan::planned_active_outcome::retain_observed:
      return match_retained_observation(before, after);

    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      return after.state() == fact_state::not_applicable
          ? result_observation_match::matched
          : result_observation_match::mismatch;

    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      if (effect == nullptr)
        throw std::logic_error(
            "conditional cleanup result lacks its active effect");
      if (effect->result().outcome() ==
          backend_operation_outcome::conditional_retained)
      {
        return match_retained_observation(before, after);
      }
      return after.state() == fact_state::not_applicable
          ? result_observation_match::matched
          : result_observation_match::mismatch;
  }
  throw std::logic_error("invalid planned active result");
}

template<class Decision>
application_path_consequence
completed_path_consequence(
    const active_mutated_application& active,
    const Decision& decision,
    const application_path_observation& before,
    application_path_observation after,
    ownership_publication_status publication)
{
  const active_effect_application_result* active_effect =
      find_active_effect(active, decision.path());
  const rejected_effect_application_result* rejected_effect =
      find_rejected_effect(active, decision.path());

  const application_effect_status active_status = active_effect == nullptr
      ? application_effect_status::completed
      : completion_effect_status(active_effect->result().outcome());
  const application_effect_status rejected_status = rejected_effect == nullptr
      ? application_effect_status::not_attempted
      : rejected_effect_status(rejected_effect->result().outcome());
  const std::optional<rejected_object_record_identity> rejected_record =
      rejected_effect == nullptr ? std::nullopt
                                 : rejected_effect->result().record();

  return application_path_consequence(
      decision.path(), completion_path_role(decision), decision.active(),
      decision.rejected(), active_incoming_entry(decision),
      decision.ownership(), active_status, rejected_status, before,
      std::move(after), rejected_record, publication);
}

template<class Plan>
std::vector<pkgplan::package_path>
result_paths(const Plan& plan)
{
  std::vector<pkgplan::package_path> paths;
  paths.reserve(plan.paths().size());
  for (const auto& decision : plan.paths())
    paths.push_back(decision.path());
  return paths;
}

template<class Request>
void
validate_completion_binding(
    const active_mutated_application& active,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image* image)
{
  validate_target_mutation_lease(request.target(), state, lease);
  const journaled_application& journaled =
      active.rejected().prepared().journaled();
  const application_journal_record& journal = journaled.journal();
  const application_journal_header& header = journal.header();
  if (journal.state() != application_journal_state::effects_visible ||
      header.request() != request.identity() ||
      header.plan() != request.plan().identity() ||
      header.target() != request.target().identity() ||
      header.control() != request.control().identity() ||
      header.state_projection() != state.identity() ||
      header.lease() != lease.identity() ||
      header.attempt().identity() != journaled.admitted().attempt().identity())
  {
    throw std::invalid_argument(
        "application completion inputs differ from active execution");
  }

  if (image != nullptr) {
    if (!journaled.payloads() ||
        journaled.payloads()->image() != image->identity())
    {
      throw std::invalid_argument(
          "application completion image differs from prepared payloads");
    }
  }
  else if (journaled.payloads()) {
    throw std::invalid_argument(
        "archive-free completion retained incoming payload authority");
  }
}

application_durability_profile
with_completed_evidence_durability(
    const application_durability_profile& previous,
    application_durability_status completed)
{
  std::vector<application_durability_fact> facts;
  facts.reserve(previous.facts().size());
  for (const auto& fact : previous.facts()) {
    facts.emplace_back(
        fact.domain(),
        fact.domain() == application_durability_domain::completed_evidence
            ? completed
            : fact.status());
  }
  return application_durability_profile(std::move(facts));
}

std::vector<application_path_consequence>
ineligible_paths(const std::vector<application_path_consequence>& completed)
{
  std::vector<application_path_consequence> paths;
  paths.reserve(completed.size());
  for (const auto& path : completed) {
    paths.emplace_back(
        path.path(), path.role(), path.requested_active(),
        path.requested_rejected(), path.incoming_entry(), path.ownership(),
        path.active_status(), path.rejected_status(), path.before(),
        path.after(), path.rejected_object(),
        ownership_publication_status::ineligible);
  }
  return paths;
}

template<class Request>
completed_application_evidence
make_completed_evidence(
    const Request& request,
    const active_mutated_application& active,
    const lease_bound_state_projection& state,
    std::vector<application_path_consequence> paths,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> evidence)
{
  const application_attempt_identity attempt =
      active.rejected().prepared().journaled().admitted().attempt().identity();
  const application_journal_identity journal =
      active.rejected().prepared().journaled().journal().header().identity();
  if constexpr (std::is_same_v<Request, installation_application_request>) {
    return completed_application_evidence::installation(
        request, attempt, state.identity(), journal, std::move(paths),
        std::move(durability), std::move(evidence));
  }
  else if constexpr (std::is_same_v<Request, upgrade_application_request>) {
    return completed_application_evidence::upgrade(
        request, attempt, state.identity(), journal, std::move(paths),
        std::move(durability), std::move(evidence));
  }
  else {
    return completed_application_evidence::removal(
        request, attempt, state.identity(), journal, std::move(paths),
        std::move(durability), std::move(evidence));
  }
}

template<class Request>
application_receipt
completion_failure_receipt(
    active_mutated_application& active,
    const Request& request,
    const lease_bound_state_projection& state,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    const std::vector<application_path_consequence>& paths,
    std::vector<application_backend_evidence_identity> evidence)
{
  return application_receipt::failed(
      request,
      active.rejected().prepared().journaled().admitted().attempt().identity(),
      state.identity(), outcome, recovery, std::move(durability),
      ineligible_paths(paths),
      active.rejected().prepared().journaled().journal().header().identity(),
      std::move(evidence));
}

void
seal_terminal_receipt(
    journaled_application& journaled,
    application_journal_state state,
    const application_receipt& receipt,
    const std::optional<completed_application_evidence_identity>& evidence =
        std::nullopt)
{
  const application_journal_effect_identity seal = find_effect(
      journaled.journal(), application_journal_effect_kind::seal_receipt).
          identity();
  publish_event(
      journaled, journaled.journal().state(), seal,
      application_journal_event_kind::intent);
  std::vector<application_journal_event> events = journaled.journal().events();
  events.emplace_back(
      static_cast<std::uint64_t>(events.size()),
      application_journal_event_kind::completed, seal);
  publish_snapshot(
      journaled, state, std::move(events), receipt.identity(), evidence);
}

template<class Request>
application_engine_completion
complete_application(
    active_mutated_application active,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image* image)
{
  validate_completion_binding(active, request, state, lease, image);
  journaled_application& journaled =
      active.rejected().prepared().journaled();

  for (const auto& decision : request.plan().paths()) {
    const application_journal_effect_identity observe = find_effect(
        journaled.journal(), application_journal_effect_kind::observe_result,
        decision.path()).identity();
    publish_event(
        journaled, application_journal_state::effects_visible, observe,
        application_journal_event_kind::intent);
  }

  backend_observation_batch observations =
      journaled.admitted().transaction().observe(result_paths(request.plan()));
  std::vector<application_backend_evidence_identity> evidence =
      std::move(active.backend_evidence());
  append_unique_evidence(evidence, observations.evidence());

  std::vector<application_path_consequence> paths;
  paths.reserve(request.plan().paths().size());
  bool mismatch = false;
  bool indeterminate = false;
  for (const auto& decision : request.plan().paths()) {
    const application_path_observation* before =
        journaled.admitted().preconditions().observations().find(
            decision.path());
    const application_path_observation* after =
        observations.find(decision.path());
    if (before == nullptr || after == nullptr)
      throw std::logic_error(
          "result observation lacks admitted path closure");

    const result_observation_match match = match_result_observation(
        active, decision, *before, *after, image);
    mismatch = mismatch || match == result_observation_match::mismatch;
    indeterminate = indeterminate ||
        match == result_observation_match::indeterminate;

    const application_journal_effect_identity observe = find_effect(
        journaled.journal(), application_journal_effect_kind::observe_result,
        decision.path()).identity();
    publish_event(
        journaled, application_journal_state::effects_visible, observe,
        match == result_observation_match::matched
            ? application_journal_event_kind::completed
            : match == result_observation_match::mismatch
                ? application_journal_event_kind::failed
                : application_journal_event_kind::indeterminate,
        observations.evidence());

    if (match == result_observation_match::matched) {
      paths.push_back(completed_path_consequence(
          active, decision, *before, *after,
          ownership_publication_status::eligible));
    }
  }

  if (mismatch || indeterminate) {
    publish_snapshot(
        journaled,
        indeterminate ? application_journal_state::indeterminate
                      : application_journal_state::recovery_pending,
        journaled.journal().events());
    application_durability_profile durability = active.durability();
    rejected_published_application rejected = std::move(active.rejected());
    std::vector<active_effect_application_result> active_effects =
        std::move(active.active_effects());
    return application_engine_completion::interrupted(
        std::move(rejected), std::move(active_effects),
        indeterminate
            ? active_execution_interruption::result_observation_indeterminate
            : active_execution_interruption::result_observation_mismatch,
        std::move(durability), std::move(evidence));
  }

  publish_snapshot(
      journaled, application_journal_state::result_observed,
      journaled.journal().events());

  application_durability_profile completed_durability =
      with_completed_evidence_durability(
          active.durability(), application_durability_status::confirmed);
  completed_application_evidence completed = make_completed_evidence(
      request, active, state, paths, completed_durability, evidence);

  const application_journal_effect_identity publish = find_effect(
      journaled.journal(),
      application_journal_effect_kind::publish_completed_evidence).
          identity();
  publish_event(
      journaled, application_journal_state::result_observed, publish,
      application_journal_event_kind::intent);
  completed_evidence_publication_result publication =
      journaled.admitted().transaction().publish_completed_evidence(completed);
  append_unique_evidence(evidence, publication.evidence());
  publish_event(
      journaled, application_journal_state::result_observed, publish,
      terminal_event(publication.outcome()), publication.evidence());

  if (publication.outcome() != backend_operation_outcome::completed) {
    const bool uncertain = publication.outcome() ==
        backend_operation_outcome::indeterminate;
    application_durability_profile failed_durability =
        with_completed_evidence_durability(
            active.durability(),
            uncertain ? application_durability_status::indeterminate
                      : application_durability_status::unconfirmed);
    application_receipt receipt = completion_failure_receipt(
        active, request, state,
        uncertain ? application_attempt_outcome::indeterminate
                  : application_attempt_outcome::
                        effects_visible_durability_unconfirmed,
        uncertain
            ? application_recovery_state::requires_authoritative_observation
            : application_recovery_state::recovery_assets_retained,
        std::move(failed_durability), paths, std::move(evidence));
    seal_terminal_receipt(
        journaled,
        uncertain ? application_journal_state::indeterminate
                  : application_journal_state::effects_visible,
        receipt);
    return application_engine_completion::sealed(std::move(receipt));
  }
  if (!publication.record() || *publication.record() != completed.identity())
    throw std::logic_error(
        "backend published another completed-evidence record");

  const application_journal_effect_identity synchronize = find_effect(
      journaled.journal(),
      application_journal_effect_kind::synchronize_completed_evidence).
          identity();
  publish_event(
      journaled, application_journal_state::result_observed, synchronize,
      application_journal_event_kind::intent);
  const application_durability_fact synchronized =
      journaled.admitted().transaction().synchronize(
          application_durability_domain::completed_evidence);
  if (synchronized.domain() !=
      application_durability_domain::completed_evidence)
  {
    throw std::logic_error(
        "backend synchronized another completed-evidence domain");
  }
  if (synchronized.status() == application_durability_status::not_attempted)
    throw std::logic_error(
        "backend reported unattempted completed-evidence durability");
  publish_event(
      journaled, application_journal_state::result_observed, synchronize,
      terminal_event(synchronized.status()));

  if (synchronized.status() != application_durability_status::confirmed) {
    const bool uncertain = synchronized.status() ==
        application_durability_status::indeterminate;
    application_durability_profile failed_durability =
        with_completed_evidence_durability(
            active.durability(),
            uncertain ? application_durability_status::indeterminate
                      : application_durability_status::unconfirmed);
    application_receipt receipt = completion_failure_receipt(
        active, request, state,
        uncertain ? application_attempt_outcome::indeterminate
                  : application_attempt_outcome::
                        effects_visible_durability_unconfirmed,
        uncertain
            ? application_recovery_state::requires_authoritative_observation
            : application_recovery_state::recovery_assets_retained,
        std::move(failed_durability), paths, std::move(evidence));
    seal_terminal_receipt(
        journaled,
        uncertain ? application_journal_state::indeterminate
                  : application_journal_state::effects_visible,
        receipt);
    return application_engine_completion::sealed(std::move(receipt));
  }

  const application_recovery_state recovery =
      active.rejected().prepared().captures().empty()
          ? application_recovery_state::unchanged
          : application_recovery_state::recovery_assets_retained;
  application_receipt receipt = application_receipt::completed(
      completed, recovery, evidence);
  seal_terminal_receipt(
      journaled, application_journal_state::application_completed, receipt,
      completed.identity());
  return application_engine_completion::sealed(std::move(receipt));
}

} // namespace

application_engine_completion
application_engine_completion::sealed(application_receipt receipt)
{
  return application_engine_completion(value_type(std::move(receipt)));
}

application_engine_completion
application_engine_completion::interrupted(
    rejected_published_application rejected,
    std::vector<active_effect_application_result> active_effects,
    active_execution_interruption interruption,
    application_durability_profile durability,
    std::vector<application_backend_evidence_identity> backend_evidence)
{
  return application_engine_completion(value_type(
      std::in_place_type<active_interrupted_application>,
      std::move(rejected), std::move(active_effects), interruption,
      std::move(durability), std::move(backend_evidence)));
}

application_engine_completion::application_engine_completion(value_type value)
    : value_(std::move(value))
{
}

bool
application_engine_completion::has_receipt() const noexcept
{
  return std::holds_alternative<application_receipt>(value_);
}

application_receipt*
application_engine_completion::receipt() noexcept
{
  return std::get_if<application_receipt>(&value_);
}

const application_receipt*
application_engine_completion::receipt() const noexcept
{
  return std::get_if<application_receipt>(&value_);
}

active_interrupted_application*
application_engine_completion::interruption() noexcept
{
  return std::get_if<active_interrupted_application>(&value_);
}

const active_interrupted_application*
application_engine_completion::interruption() const noexcept
{
  return std::get_if<active_interrupted_application>(&value_);
}

application_engine_completion
complete_application_engine(
    active_mutated_application active,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image)
{
  return complete_application(
      std::move(active), request, state, lease, &image);
}

application_engine_completion
complete_application_engine(
    active_mutated_application active,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image)
{
  return complete_application(
      std::move(active), request, state, lease, &image);
}

application_engine_completion
complete_application_engine(
    active_mutated_application active,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return complete_application(
      std::move(active), request, state, lease, nullptr);
}

} // namespace pkgapply::detail

namespace pkgapply::detail {
namespace {

struct recovery_effect_result final {
  pkgplan::package_path path;
  backend_operation_result result;
  bool exact_prior_state_possible;
};

application_effect_status
active_effect_status(backend_operation_outcome outcome)
{
  switch (outcome) {
    case backend_operation_outcome::completed:
      return application_effect_status::completed;
    case backend_operation_outcome::conditional_retained:
      return application_effect_status::conditional_retained;
    case backend_operation_outcome::failed:
      return application_effect_status::failed;
    case backend_operation_outcome::indeterminate:
      return application_effect_status::indeterminate;
  }
  throw std::logic_error("invalid active-effect outcome");
}

const active_effect_application_result*
find_active_effect(
    const active_interrupted_application& interrupted,
    const pkgplan::package_path& path) noexcept
{
  const auto item = std::find_if(
      interrupted.active_effects().begin(),
      interrupted.active_effects().end(),
      [&path](const auto& effect) {
        return effect.request().path() == path;
      });
  return item == interrupted.active_effects().end() ? nullptr : &*item;
}

const rejected_effect_application_result*
find_rejected_effect(
    const active_interrupted_application& interrupted,
    const pkgplan::package_path& path) noexcept
{
  const auto& effects = interrupted.rejected().rejected_effects();
  const auto item = std::find_if(
      effects.begin(), effects.end(),
      [&path](const auto& effect) {
        return effect.request().path() == path;
      });
  return item == effects.end() ? nullptr : &*item;
}

const recovery_effect_result*
find_recovery_effect(
    const std::vector<recovery_effect_result>& effects,
    const pkgplan::package_path& path) noexcept
{
  const auto item = std::find_if(
      effects.begin(), effects.end(),
      [&path](const auto& effect) { return effect.path == path; });
  return item == effects.end() ? nullptr : &*item;
}

const old_object_capture_result*
find_completed_capture(
    const active_interrupted_application& interrupted,
    const pkgplan::package_path& path) noexcept
{
  const auto& captures = interrupted.rejected().prepared().captures();
  const auto item = std::find_if(
      captures.begin(), captures.end(),
      [&path](const auto& capture) {
        return capture.captured().path() == path &&
               capture.outcome() == backend_operation_outcome::completed;
      });
  return item == captures.end() ? nullptr : &*item;
}

bool
exact_recovery_possible(
    const active_interrupted_application& interrupted,
    const pkgplan::package_path& path)
{
  const application_path_observation* before =
      interrupted.rejected().prepared().journaled().admitted().
          preconditions().observations().find(path);
  if (before == nullptr)
    throw std::logic_error("recovery path lacks admitted observation");
  if (before->state() == fact_state::not_applicable)
    return true;
  if (before->state() != fact_state::known)
    return false;

  const old_object_capture_result* capture =
      find_completed_capture(interrupted, path);
  return capture != nullptr && capture->exact_recovery_possible();
}

std::vector<const active_effect_application_result*>
recovery_candidates(const active_interrupted_application& interrupted)
{
  std::vector<const active_effect_application_result*> candidates;
  for (auto effect = interrupted.active_effects().rbegin();
       effect != interrupted.active_effects().rend(); ++effect)
  {
    if (effect->changed_target() ||
        effect->result().outcome() ==
            backend_operation_outcome::indeterminate)
    {
      candidates.push_back(&*effect);
    }
  }
  return candidates;
}

template<class Decision>
application_path_role
recovery_path_role(const Decision& decision)
{
  if constexpr (std::is_same_v<Decision, pkgplan::removal_path_decision>)
    return application_path_role::installed_owned_path;
  else
    return rejected_path_role(decision.role());
}

template<class Decision>
application_path_consequence
recovery_path_consequence(
    const active_interrupted_application& interrupted,
    const Decision& decision,
    const std::vector<recovery_effect_result>& recoveries)
{
  const active_effect_application_result* active =
      find_active_effect(interrupted, decision.path());
  const rejected_effect_application_result* rejected =
      find_rejected_effect(interrupted, decision.path());
  const application_path_observation* before =
      interrupted.rejected().prepared().journaled().admitted().
          preconditions().observations().find(decision.path());
  if (before == nullptr)
    throw std::logic_error("failure consequence lacks admitted observation");

  application_effect_status active_status =
      application_effect_status::not_attempted;
  application_path_observation after = *before;
  if (active != nullptr) {
    active_status = active_effect_status(active->result().outcome());
    if (active->changed_target() ||
        active->result().outcome() == backend_operation_outcome::indeterminate)
    {
      const recovery_effect_result* recovery =
          find_recovery_effect(recoveries, decision.path());
      if (recovery == nullptr ||
          recovery->result.outcome() != backend_operation_outcome::completed ||
          !recovery->exact_prior_state_possible)
      {
        after = application_path_observation::unknown(decision.path());
      }
    }
  }

  application_effect_status rejected_status =
      application_effect_status::not_attempted;
  std::optional<rejected_object_record_identity> rejected_record;
  if (rejected != nullptr) {
    rejected_status = rejected_effect_status(rejected->result().outcome());
    rejected_record = rejected->result().record();
  }

  return application_path_consequence(
      decision.path(), recovery_path_role(decision), decision.active(),
      decision.rejected(), active_incoming_entry(decision),
      decision.ownership(), active_status, rejected_status, *before,
      std::move(after), std::move(rejected_record),
      ownership_publication_status::ineligible);
}

template<class Plan>
std::vector<application_path_consequence>
recovery_path_consequences(
    const active_interrupted_application& interrupted,
    const Plan& plan,
    const std::vector<recovery_effect_result>& recoveries)
{
  std::vector<application_path_consequence> paths;
  for (const auto& decision : plan.paths()) {
    if (find_active_effect(interrupted, decision.path()) == nullptr &&
        find_rejected_effect(interrupted, decision.path()) == nullptr)
    {
      continue;
    }
    paths.push_back(
        recovery_path_consequence(interrupted, decision, recoveries));
  }
  return paths;
}

template<class Request>
void
validate_recovery_binding(
    const active_interrupted_application& interrupted,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_target_mutation_lease(request.target(), state, lease);
  const auto& journal =
      interrupted.rejected().prepared().journaled().journal();
  const auto& header = journal.header();
  const bool indeterminate =
      interrupted.interruption() ==
          active_execution_interruption::effect_indeterminate ||
      interrupted.interruption() ==
          active_execution_interruption::durability_indeterminate ||
      interrupted.interruption() ==
          active_execution_interruption::result_observation_indeterminate;
  const application_journal_state expected = indeterminate
      ? application_journal_state::indeterminate
      : application_journal_state::recovery_pending;

  if (journal.state() != expected ||
      header.request() != request.identity() ||
      header.plan() != request.plan().identity() ||
      header.target() != request.target().identity() ||
      header.control() != request.control().identity() ||
      header.state_projection() != state.identity() ||
      header.lease() != lease.identity() ||
      header.attempt().identity() !=
          interrupted.rejected().prepared().journaled().admitted().
              attempt().identity())
  {
    throw std::invalid_argument(
        "recovery inputs differ from interrupted active execution");
  }
}

bool
recovery_selected(const application_execution_control& control)
{
  switch (control.recovery()) {
    case application_recovery_requirement::none:
      return false;
    case application_recovery_requirement::best_effort:
    case application_recovery_requirement::exact_prior_state:
      return true;
  }
  throw std::logic_error("invalid recovery requirement");
}

bool
requires_recovered_namespace_synchronization(
    const application_execution_control& control)
{
  return control.durability() ==
      application_durability_requirement::all_application_domains;
}

application_journal_state
terminal_failure_journal_state(
    const active_interrupted_application& interrupted,
    application_attempt_outcome outcome)
{
  switch (outcome) {
    case application_attempt_outcome::failed_fully_recovered:
      return application_journal_state::recovered;
    case application_attempt_outcome::effects_visible_durability_unconfirmed:
    case application_attempt_outcome::failed_with_partial_effects:
      return application_journal_state::effects_visible;
    case application_attempt_outcome::indeterminate:
      return application_journal_state::indeterminate;
    case application_attempt_outcome::failed_before_target_mutation: {
      const bool rejected_visible = std::any_of(
          interrupted.rejected().rejected_effects().begin(),
          interrupted.rejected().rejected_effects().end(),
          [](const auto& effect) {
            return effect.result().outcome() ==
                backend_operation_outcome::completed;
          });
      return rejected_visible ? application_journal_state::effects_visible
                              : application_journal_state::abandoned;
    }
    case application_attempt_outcome::precondition_refused:
    case application_attempt_outcome::completed:
      break;
  }
  throw std::logic_error("invalid terminal recovery outcome");
}

template<class Request>
application_receipt
seal_recovery_receipt(
    active_interrupted_application& interrupted,
    const Request& request,
    const lease_bound_state_projection& state,
    application_attempt_outcome outcome,
    application_recovery_state recovery,
    application_durability_profile durability,
    const std::vector<recovery_effect_result>& recoveries,
    std::vector<application_backend_evidence_identity> evidence)
{
  std::vector<application_path_consequence> paths =
      recovery_path_consequences(interrupted, request.plan(), recoveries);
  application_receipt receipt = application_receipt::failed(
      request,
      interrupted.rejected().prepared().journaled().admitted().
          attempt().identity(),
      state.identity(), outcome, recovery, std::move(durability),
      std::move(paths),
      interrupted.rejected().prepared().journaled().journal().
          header().identity(),
      std::move(evidence));
  publish_snapshot(
      interrupted.rejected().prepared().journaled(),
      terminal_failure_journal_state(interrupted, outcome),
      interrupted.rejected().prepared().journaled().journal().events(),
      receipt.identity());
  return receipt;
}

template<class Request>
application_receipt
recover_interrupted(
    active_interrupted_application interrupted,
    const Request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_recovery_binding(interrupted, request, state, lease);

  const auto candidates = recovery_candidates(interrupted);
  std::vector<recovery_effect_result> recoveries;
  std::vector<application_backend_evidence_identity> evidence =
      interrupted.backend_evidence();

  if (candidates.empty()) {
    const bool observation_interruption =
        interrupted.interruption() ==
            active_execution_interruption::result_observation_mismatch ||
        interrupted.interruption() ==
            active_execution_interruption::result_observation_indeterminate;
    return seal_recovery_receipt(
        interrupted, request, state,
        observation_interruption
            ? application_attempt_outcome::indeterminate
            : application_attempt_outcome::failed_before_target_mutation,
        observation_interruption
            ? application_recovery_state::requires_authoritative_observation
            : application_recovery_state::unchanged,
        interrupted.durability(), recoveries, std::move(evidence));
  }

  if (!recovery_selected(request.control())) {
    if (interrupted.interruption() ==
            active_execution_interruption::effect_indeterminate ||
        interrupted.interruption() ==
            active_execution_interruption::durability_indeterminate ||
        interrupted.interruption() ==
            active_execution_interruption::result_observation_indeterminate)
    {
      return seal_recovery_receipt(
          interrupted, request, state,
          application_attempt_outcome::indeterminate,
          application_recovery_state::requires_authoritative_observation,
          interrupted.durability(), recoveries, std::move(evidence));
    }

    if (interrupted.interruption() ==
        active_execution_interruption::durability_unconfirmed)
    {
      return seal_recovery_receipt(
          interrupted, request, state,
          application_attempt_outcome::
              effects_visible_durability_unconfirmed,
          application_recovery_state::recovery_not_representable,
          interrupted.durability(), recoveries, std::move(evidence));
    }

    return seal_recovery_receipt(
        interrupted, request, state,
        application_attempt_outcome::failed_with_partial_effects,
        application_recovery_state::known_residual_effects,
        interrupted.durability(), recoveries, std::move(evidence));
  }

  publish_snapshot(
      interrupted.rejected().prepared().journaled(),
      application_journal_state::recovering,
      interrupted.rejected().prepared().journaled().journal().events());

  bool original_indeterminate =
      interrupted.interruption() ==
          active_execution_interruption::effect_indeterminate ||
      interrupted.interruption() ==
          active_execution_interruption::durability_indeterminate ||
      interrupted.interruption() ==
          active_execution_interruption::result_observation_indeterminate;
  bool all_exact = true;
  bool all_recovered = true;
  bool recovery_indeterminate = false;

  for (const active_effect_application_result* candidate : candidates) {
    const pkgplan::package_path& path = candidate->request().path();
    const bool exact = exact_recovery_possible(interrupted, path);
    all_exact = all_exact && exact;

    const application_journal_effect_identity effect = find_effect(
        interrupted.rejected().prepared().journaled().journal(),
        application_journal_effect_kind::recover_active_object,
        path).identity();
    publish_event(
        interrupted.rejected().prepared().journaled(),
        application_journal_state::recovering, effect,
        application_journal_event_kind::intent);

    backend_operation_result result =
        interrupted.rejected().prepared().journaled().admitted().
            transaction().recover(path);
    append_unique_evidence(evidence, result.evidence());
    const backend_operation_outcome result_outcome = result.outcome();
    publish_event(
        interrupted.rejected().prepared().journaled(),
        application_journal_state::recovering, effect,
        terminal_event(result_outcome), result.evidence());
    recoveries.push_back(
        {path, std::move(result), exact});

    if (result_outcome != backend_operation_outcome::completed) {
      all_recovered = false;
      recovery_indeterminate = result_outcome ==
          backend_operation_outcome::indeterminate;
      break;
    }
  }

  application_durability_profile durability = interrupted.durability();
  if (!recoveries.empty() &&
      std::any_of(
          recoveries.begin(), recoveries.end(),
          [](const auto& recovery) {
            return recovery.result.outcome() ==
                backend_operation_outcome::completed;
          }))
  {
    durability = with_active_durability(
        durability, application_durability_status::visible);
  }

  if (all_recovered &&
      requires_recovered_namespace_synchronization(request.control()))
  {
    const application_journal_effect_identity synchronize_effect = find_effect(
        interrupted.rejected().prepared().journaled().journal(),
        application_journal_effect_kind::synchronize_recovered_namespace).
            identity();
    publish_event(
        interrupted.rejected().prepared().journaled(),
        application_journal_state::recovering, synchronize_effect,
        application_journal_event_kind::intent);
    const application_durability_fact synchronized =
        interrupted.rejected().prepared().journaled().admitted().
            transaction().synchronize(
                application_durability_domain::active_namespace);
    if (synchronized.domain() !=
        application_durability_domain::active_namespace)
    {
      throw std::logic_error(
          "backend synchronized another recovered namespace domain");
    }
    if (synchronized.status() ==
        application_durability_status::not_attempted)
    {
      throw std::logic_error(
          "backend reported unattempted recovered namespace durability");
    }
    publish_event(
        interrupted.rejected().prepared().journaled(),
        application_journal_state::recovering, synchronize_effect,
        terminal_event(synchronized.status()));
    durability = with_active_durability(durability, synchronized.status());
  }

  if (all_recovered && all_exact) {
    return seal_recovery_receipt(
        interrupted, request, state,
        application_attempt_outcome::failed_fully_recovered,
        application_recovery_state::exact_prior_state_restored,
        std::move(durability), recoveries, std::move(evidence));
  }

  if (original_indeterminate || recovery_indeterminate ||
      (all_recovered && !all_exact))
  {
    return seal_recovery_receipt(
        interrupted, request, state,
        application_attempt_outcome::indeterminate,
        application_recovery_state::requires_authoritative_observation,
        std::move(durability), recoveries, std::move(evidence));
  }

  return seal_recovery_receipt(
      interrupted, request, state,
      application_attempt_outcome::failed_with_partial_effects,
      application_recovery_state::known_residual_effects,
      std::move(durability), recoveries, std::move(evidence));
}

} // namespace

application_receipt
recover_application_engine(
    active_interrupted_application interrupted,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return recover_interrupted(
      std::move(interrupted), request, state, lease);
}

application_receipt
recover_application_engine(
    active_interrupted_application interrupted,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return recover_interrupted(
      std::move(interrupted), request, state, lease);
}

application_receipt
recover_application_engine(
    active_interrupted_application interrupted,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  return recover_interrupted(
      std::move(interrupted), request, state, lease);
}

} // namespace pkgapply::detail
