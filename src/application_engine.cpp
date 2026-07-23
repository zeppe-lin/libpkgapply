// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_engine.h"

#include <algorithm>
#include <cstddef>
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
          "preparation backend returned a conditional outcome");
  }
  throw std::logic_error("invalid preparation backend outcome");
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
publish_snapshot(journaled_application& application,
                 application_journal_state state,
                 std::vector<application_journal_event> events,
                 std::optional<application_receipt_identity> receipt =
                     std::nullopt)
{
  application_journal_record intended = application_journal_record::make(
      application.journal().header(), state, application.journal().effects(),
      std::move(events), std::move(receipt));
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
