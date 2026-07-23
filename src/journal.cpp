// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pkgapply {
namespace {

std::uint8_t
canonical_kind(pkgplan::operation_kind kind)
{
  switch (kind) {
    case pkgplan::operation_kind::install:
      return 1;
    case pkgplan::operation_kind::upgrade:
      return 2;
    case pkgplan::operation_kind::remove:
      return 3;
  }
  throw std::invalid_argument("invalid journal operation kind");
}

std::uint8_t
canonical_state(application_journal_state state)
{
  const auto value = static_cast<std::uint8_t>(state);
  if (value < 1 || value > 12)
    throw std::invalid_argument("invalid application journal state");
  return value;
}

std::uint8_t
canonical_effect(application_journal_effect_kind kind)
{
  const auto value = static_cast<std::uint8_t>(kind);
  if (value < 1 || value > 9)
    throw std::invalid_argument("invalid application journal effect kind");
  return value;
}

std::uint8_t
canonical_event(application_journal_event_kind kind)
{
  const auto value = static_cast<std::uint8_t>(kind);
  if (value < 1 || value > 4)
    throw std::invalid_argument("invalid application journal event kind");
  return value;
}

bool
requires_path(application_journal_effect_kind kind)
{
  switch (kind) {
    case application_journal_effect_kind::capture_old_object:
    case application_journal_effect_kind::stage_incoming_payload:
    case application_journal_effect_kind::publish_active_object:
    case application_journal_effect_kind::publish_rejected_object:
    case application_journal_effect_kind::observe_result:
      return true;

    case application_journal_effect_kind::synchronize_journal:
    case application_journal_effect_kind::synchronize_active_namespace:
    case application_journal_effect_kind::synchronize_rejected_store:
    case application_journal_effect_kind::seal_receipt:
      return false;
  }
  throw std::invalid_argument("invalid application journal effect kind");
}

void
normalize_evidence(
    std::vector<application_backend_evidence_identity>& evidence)
{
  std::sort(evidence.begin(), evidence.end());
  if (std::adjacent_find(evidence.begin(), evidence.end()) != evidence.end())
    throw std::invalid_argument("duplicate journal backend evidence");
}

application_journal_identity
identify_header(
    pkgplan::operation_kind kind,
    const application_request_identity& request,
    const pkgplan::operation_plan_identity& plan,
    const application_attempt_identity& attempt,
    const application_target_context_identity& target,
    const application_execution_control_identity& control,
    const lease_bound_state_projection_identity& state_projection,
    const mutation_lease_instance_identity& lease,
    const mutation_backend_identity& backend)
{
  detail::canonical_record record(
      application_journal_identity::canonical_domain());
  record.append_u16(application_journal_schema_version);
  record.append_u8(canonical_kind(kind));
  record.append_digest(request);
  record.append_bytes(plan.string());
  record.append_digest(attempt);
  record.append_digest(target);
  record.append_digest(control);
  record.append_digest(state_projection);
  record.append_digest(lease);
  record.append_digest(backend);
  return detail::identity_factory::from_sha256<application_journal_identity>(
      record.sha256());
}

application_journal_effect_identity
identify_effect(
    std::uint64_t ordinal,
    application_journal_effect_kind kind,
    const std::optional<pkgplan::package_path>& path)
{
  detail::canonical_record record(
      application_journal_effect_identity::canonical_domain());
  record.append_u16(application_journal_effect_schema_version);
  record.append_u64(ordinal);
  record.append_u8(canonical_effect(kind));
  record.append_bool(path.has_value());
  if (path)
    record.append_bytes(path->string());
  return detail::identity_factory::from_sha256<
      application_journal_effect_identity>(record.sha256());
}

struct effect_progress final {
  bool intended = false;
  std::optional<application_journal_event_kind> terminal;
};

std::vector<effect_progress>
validate_events(
    const std::vector<application_journal_effect>& effects,
    const std::vector<application_journal_event>& events)
{
  std::vector<effect_progress> progress(effects.size());

  for (std::size_t index = 0; index < events.size(); ++index) {
    const auto& event = events[index];
    if (event.sequence() != index)
      throw std::invalid_argument("journal event sequence is not consecutive");

    const auto effect = std::find_if(
        effects.begin(), effects.end(),
        [&event](const auto& item) {
          return item.identity() == event.effect();
        });
    if (effect == effects.end())
      throw std::invalid_argument("journal event cites an unknown effect");

    auto& state = progress[effect->ordinal()];
    if (event.kind() == application_journal_event_kind::intent) {
      if (state.intended || state.terminal)
        throw std::invalid_argument("duplicate or late journal effect intent");
      state.intended = true;
      continue;
    }

    if (!state.intended)
      throw std::invalid_argument("journal terminal event precedes intent");
    if (state.terminal)
      throw std::invalid_argument("journal effect has multiple terminal events");
    state.terminal = event.kind();
  }

  return progress;
}

bool
all_effects_completed(const std::vector<effect_progress>& progress) noexcept
{
  return std::all_of(
      progress.begin(), progress.end(),
      [](const auto& item) {
        return item.intended && item.terminal ==
            application_journal_event_kind::completed;
      });
}

void
validate_resolution(
    application_journal_state state,
    const std::vector<effect_progress>& progress,
    const std::optional<application_receipt_identity>& receipt,
    const std::optional<completed_application_evidence_identity>& evidence)
{
  if (evidence && !receipt)
    throw std::invalid_argument(
        "journal completed evidence lacks its application receipt");

  const bool successful_state =
      state == application_journal_state::application_completed ||
      state == application_journal_state::external_resolution_pending ||
      state == application_journal_state::finalized;

  if (successful_state) {
    if (!receipt || !evidence)
      throw std::invalid_argument(
          "completed journal state lacks receipt or completed evidence");
    if (!all_effects_completed(progress))
      throw std::invalid_argument(
          "completed journal state has unfinished effects");
  } else if (evidence) {
    throw std::invalid_argument(
        "non-completed journal state contains completed evidence");
  }

  if (state == application_journal_state::recovered && !receipt)
    throw std::invalid_argument("recovered journal state lacks failure receipt");

  if (state == application_journal_state::abandoned && evidence)
    throw std::invalid_argument("abandoned journal contains completed evidence");
}

application_journal_record_identity
identify_record(
    const application_journal_header& header,
    application_journal_state state,
    const std::vector<application_journal_effect>& effects,
    const std::vector<application_journal_event>& events,
    const std::optional<application_receipt_identity>& receipt,
    const std::optional<completed_application_evidence_identity>& evidence)
{
  detail::canonical_record record(
      application_journal_record_identity::canonical_domain());
  record.append_u16(application_journal_record_schema_version);
  record.append_digest(header.identity());
  record.append_u8(canonical_state(state));

  record.append_u64(effects.size());
  for (const auto& effect : effects)
    record.append_digest(effect.identity());

  record.append_u64(events.size());
  for (const auto& event : events) {
    record.append_u64(event.sequence());
    record.append_u8(canonical_event(event.kind()));
    record.append_digest(event.effect());
    record.append_u64(event.backend_evidence().size());
    for (const auto& item : event.backend_evidence())
      record.append_digest(item);
  }

  record.append_bool(receipt.has_value());
  if (receipt)
    record.append_digest(*receipt);
  record.append_bool(evidence.has_value());
  if (evidence)
    record.append_digest(*evidence);

  return detail::identity_factory::from_sha256<
      application_journal_record_identity>(record.sha256());
}

} // namespace

application_journal_header
application_journal_header::make(
    pkgplan::operation_kind kind,
    application_request_identity request,
    pkgplan::operation_plan_identity plan,
    application_attempt_identity attempt,
    application_target_context_identity target,
    application_execution_control_identity control,
    lease_bound_state_projection_identity state_projection,
    mutation_lease_instance_identity lease,
    mutation_backend_identity backend)
{
  auto identity = identify_header(
      kind, request, plan, attempt, target, control, state_projection, lease,
      backend);
  return application_journal_header(
      std::move(identity), kind, std::move(request), std::move(plan),
      std::move(attempt), std::move(target), std::move(control),
      std::move(state_projection), std::move(lease), std::move(backend));
}

application_journal_header::application_journal_header(
    application_journal_identity identity,
    pkgplan::operation_kind kind,
    application_request_identity request,
    pkgplan::operation_plan_identity plan,
    application_attempt_identity attempt,
    application_target_context_identity target,
    application_execution_control_identity control,
    lease_bound_state_projection_identity state_projection,
    mutation_lease_instance_identity lease,
    mutation_backend_identity backend)
    : identity_(std::move(identity)), kind_(kind), request_(std::move(request)),
      plan_(std::move(plan)), attempt_(std::move(attempt)),
      target_(std::move(target)), control_(std::move(control)),
      state_projection_(std::move(state_projection)), lease_(std::move(lease)),
      backend_(std::move(backend))
{
}

std::uint16_t application_journal_header::schema_version() const noexcept
{ return schema_version_; }
const application_journal_identity&
application_journal_header::identity() const noexcept { return identity_; }
pkgplan::operation_kind application_journal_header::kind() const noexcept
{ return kind_; }
const application_request_identity&
application_journal_header::request() const noexcept { return request_; }
const pkgplan::operation_plan_identity&
application_journal_header::plan() const noexcept { return plan_; }
const application_attempt_identity&
application_journal_header::attempt() const noexcept { return attempt_; }
const application_target_context_identity&
application_journal_header::target() const noexcept { return target_; }
const application_execution_control_identity&
application_journal_header::control() const noexcept { return control_; }
const lease_bound_state_projection_identity&
application_journal_header::state_projection() const noexcept
{ return state_projection_; }
const mutation_lease_instance_identity&
application_journal_header::lease() const noexcept { return lease_; }
const mutation_backend_identity&
application_journal_header::backend() const noexcept { return backend_; }

application_journal_effect
application_journal_effect::make(
    std::uint64_t ordinal,
    application_journal_effect_kind kind,
    std::optional<pkgplan::package_path> path)
{
  if (requires_path(kind) != path.has_value())
    throw std::invalid_argument("journal effect path applicability mismatch");
  auto identity = identify_effect(ordinal, kind, path);
  return application_journal_effect(
      std::move(identity), ordinal, kind, std::move(path));
}

application_journal_effect::application_journal_effect(
    application_journal_effect_identity identity,
    std::uint64_t ordinal,
    application_journal_effect_kind kind,
    std::optional<pkgplan::package_path> path)
    : identity_(std::move(identity)), ordinal_(ordinal), kind_(kind),
      path_(std::move(path))
{
}

const application_journal_effect_identity&
application_journal_effect::identity() const noexcept { return identity_; }
std::uint64_t application_journal_effect::ordinal() const noexcept
{ return ordinal_; }
application_journal_effect_kind
application_journal_effect::kind() const noexcept { return kind_; }
const std::optional<pkgplan::package_path>&
application_journal_effect::path() const noexcept { return path_; }
bool operator<(const application_journal_effect& lhs,
               const application_journal_effect& rhs) noexcept
{ return lhs.ordinal_ < rhs.ordinal_; }

application_journal_event::application_journal_event(
    std::uint64_t sequence,
    application_journal_event_kind kind,
    application_journal_effect_identity effect,
    std::vector<application_backend_evidence_identity> backend_evidence)
    : sequence_(sequence), kind_(kind), effect_(std::move(effect)),
      backend_evidence_(std::move(backend_evidence))
{
  static_cast<void>(canonical_event(kind_));
  normalize_evidence(backend_evidence_);
}

std::uint64_t application_journal_event::sequence() const noexcept
{ return sequence_; }
application_journal_event_kind
application_journal_event::kind() const noexcept { return kind_; }
const application_journal_effect_identity&
application_journal_event::effect() const noexcept { return effect_; }
const std::vector<application_backend_evidence_identity>&
application_journal_event::backend_evidence() const noexcept
{ return backend_evidence_; }

application_journal_record
application_journal_record::make(
    application_journal_header header,
    application_journal_state state,
    std::vector<application_journal_effect> effects,
    std::vector<application_journal_event> events,
    std::optional<application_receipt_identity> receipt,
    std::optional<completed_application_evidence_identity> completed_evidence)
{
  static_cast<void>(canonical_state(state));
  std::sort(effects.begin(), effects.end());
  for (std::size_t index = 0; index < effects.size(); ++index) {
    if (effects[index].ordinal() != index)
      throw std::invalid_argument("journal effect ordinals are not consecutive");
  }

  const auto progress = validate_events(effects, events);
  validate_resolution(state, progress, receipt, completed_evidence);
  auto identity = identify_record(
      header, state, effects, events, receipt, completed_evidence);
  return application_journal_record(
      std::move(identity), std::move(header), state, std::move(effects),
      std::move(events), std::move(receipt), std::move(completed_evidence));
}

application_journal_record::application_journal_record(
    application_journal_record_identity identity,
    application_journal_header header,
    application_journal_state state,
    std::vector<application_journal_effect> effects,
    std::vector<application_journal_event> events,
    std::optional<application_receipt_identity> receipt,
    std::optional<completed_application_evidence_identity> completed_evidence)
    : identity_(std::move(identity)), header_(std::move(header)), state_(state),
      effects_(std::move(effects)), events_(std::move(events)),
      receipt_(std::move(receipt)),
      completed_evidence_(std::move(completed_evidence))
{
}

std::uint16_t application_journal_record::schema_version() const noexcept
{ return schema_version_; }
const application_journal_record_identity&
application_journal_record::identity() const noexcept { return identity_; }
const application_journal_header&
application_journal_record::header() const noexcept { return header_; }
application_journal_state
application_journal_record::state() const noexcept { return state_; }
const std::vector<application_journal_effect>&
application_journal_record::effects() const noexcept { return effects_; }
const std::vector<application_journal_event>&
application_journal_record::events() const noexcept { return events_; }
const std::optional<application_receipt_identity>&
application_journal_record::receipt() const noexcept { return receipt_; }
const std::optional<completed_application_evidence_identity>&
application_journal_record::completed_evidence() const noexcept
{ return completed_evidence_; }

} // namespace pkgapply
