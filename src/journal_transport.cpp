// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal_transport.h>

#include "journal_transport_access.h"

#include "canonical_record.h"
#include "identity_factory.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace pkgapply {
namespace {

std::string_view byte_view(const application_journal_replay_encoding& bytes)
{
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

void validate_effect_graph(const std::vector<application_journal_effect>& effects)
{
  for (std::size_t index = 0; index < effects.size(); ++index) {
    if (effects[index].ordinal() != index)
      throw std::invalid_argument(
          "application journal declaration effects are not consecutive");
    if (index != 0 && effects[index - 1].identity() == effects[index].identity())
      throw std::invalid_argument(
          "application journal declaration repeats an effect identity");
  }
}

application_journal_declaration_identity identify_declaration(
    const application_journal_header& header,
    const std::vector<application_journal_effect>& effects,
    const application_journal_replay_encoding& replay_seed)
{
  detail::canonical_record record(
      application_journal_declaration_identity::canonical_domain());
  record.append_u16(application_journal_declaration_schema_version);
  record.append_digest(header.identity());
  record.append_u64(static_cast<std::uint64_t>(effects.size()));
  for (const auto& effect : effects)
    record.append_digest(effect.identity());
  record.append_u64(static_cast<std::uint64_t>(replay_seed.size()));
  record.append_bytes(byte_view(replay_seed));
  return detail::identity_factory::from_sha256<
      application_journal_declaration_identity>(record.sha256());
}

application_journal_step_identity identify_step(
    const application_journal_declaration_identity& declaration,
    std::uint64_t sequence,
    const std::optional<application_journal_step_identity>& predecessor,
    application_journal_state state,
    const std::optional<application_journal_event>& event,
    const application_journal_replay_encoding& replay_fact,
    const std::optional<application_receipt_identity>& receipt,
    const std::optional<completed_application_evidence_identity>& completed)
{
  const auto state_value = static_cast<std::uint8_t>(state);
  if (state_value < 1 || state_value > 13)
    throw std::invalid_argument("invalid append-only application journal state");

  detail::canonical_record record(
      application_journal_step_identity::canonical_domain());
  record.append_u16(application_journal_step_schema_version);
  record.append_digest(declaration);
  record.append_u64(sequence);
  record.append_bool(predecessor.has_value());
  if (predecessor)
    record.append_digest(*predecessor);
  record.append_u8(state_value);
  record.append_bool(event.has_value());
  if (event) {
    record.append_u64(event->sequence());
    record.append_u8(static_cast<std::uint8_t>(event->kind()));
    record.append_digest(event->effect());
    record.append_u64(
        static_cast<std::uint64_t>(event->backend_evidence().size()));
    for (const auto& evidence : event->backend_evidence())
      record.append_digest(evidence);
  }
  record.append_u64(static_cast<std::uint64_t>(replay_fact.size()));
  record.append_bytes(byte_view(replay_fact));
  record.append_bool(receipt.has_value());
  if (receipt)
    record.append_digest(*receipt);
  record.append_bool(completed.has_value());
  if (completed)
    record.append_digest(*completed);
  return detail::identity_factory::from_sha256<application_journal_step_identity>(
      record.sha256());
}

application_journal_cursor_identity identify_cursor(
    const application_journal_declaration_identity& declaration,
    std::uint64_t step_count,
    const std::optional<application_journal_step_identity>& latest_step,
    application_journal_state state,
    const std::optional<application_receipt_identity>& receipt,
    const std::optional<completed_application_evidence_identity>& completed)
{
  detail::canonical_record record(
      application_journal_cursor_identity::canonical_domain());
  record.append_u16(application_journal_cursor_schema_version);
  record.append_digest(declaration);
  record.append_u64(step_count);
  record.append_bool(latest_step.has_value());
  if (latest_step)
    record.append_digest(*latest_step);
  record.append_u8(static_cast<std::uint8_t>(state));
  record.append_bool(receipt.has_value());
  if (receipt)
    record.append_digest(*receipt);
  record.append_bool(completed.has_value());
  if (completed)
    record.append_digest(*completed);
  return detail::identity_factory::from_sha256<application_journal_cursor_identity>(
      record.sha256());
}

bool state_may_follow(
    application_journal_state previous,
    application_journal_state next) noexcept
{
  if (previous == next)
    return true;
  switch (previous) {
    case application_journal_state::preparing:
      return next == application_journal_state::prepared ||
             next == application_journal_state::abandoned;
    case application_journal_state::prepared:
      return next == application_journal_state::mutating;
    case application_journal_state::mutating:
      return next == application_journal_state::effects_visible ||
             next == application_journal_state::recovery_pending ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::effects_visible:
      return next == application_journal_state::result_observed ||
             next == application_journal_state::recovery_pending ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::result_observed:
      return next == application_journal_state::application_completed ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::recovery_pending ||
             next == application_journal_state::indeterminate;
    case application_journal_state::recovery_pending:
      return next == application_journal_state::recovering ||
             next == application_journal_state::recovered ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::indeterminate:
      return next == application_journal_state::recovering ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::abandoned;
    case application_journal_state::recovering:
      return next == application_journal_state::recovered ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::application_completed:
    case application_journal_state::external_resolution_pending:
    case application_journal_state::recovered:
    case application_journal_state::finalized:
    case application_journal_state::abandoned:
      return false;
  }
  return false;
}

} // namespace

application_journal_declaration application_journal_declaration::make(
    application_journal_header header,
    std::vector<application_journal_effect> effects,
    application_journal_replay_encoding replay_seed)
{
  validate_effect_graph(effects);
  auto identity = identify_declaration(header, effects, replay_seed);
  return application_journal_declaration(
      std::move(identity), std::move(header), std::move(effects),
      std::move(replay_seed));
}

application_journal_declaration::application_journal_declaration(
    application_journal_declaration_identity identity,
    application_journal_header header,
    std::vector<application_journal_effect> effects,
    application_journal_replay_encoding replay_seed)
    : identity_(std::move(identity)), header_(std::move(header)),
      effects_(std::move(effects)), replay_seed_(std::move(replay_seed))
{
}

std::uint16_t application_journal_declaration::schema_version() const noexcept
{
  return schema_version_;
}
const application_journal_declaration_identity&
application_journal_declaration::identity() const noexcept { return identity_; }
const application_journal_header&
application_journal_declaration::header() const noexcept { return header_; }
const std::vector<application_journal_effect>&
application_journal_declaration::effects() const noexcept { return effects_; }
const application_journal_replay_encoding&
application_journal_declaration::replay_seed() const noexcept { return replay_seed_; }

application_journal_step application_journal_step::make(
    application_journal_declaration_identity declaration,
    std::uint64_t sequence,
    std::optional<application_journal_step_identity> predecessor,
    application_journal_state state,
    std::optional<application_journal_event> event,
    application_journal_replay_encoding replay_fact,
    std::optional<application_receipt_identity> receipt,
    std::optional<completed_application_evidence_identity> completed_evidence)
{
  if ((sequence == 0) != !predecessor.has_value())
    throw std::invalid_argument(
        "application journal step predecessor contradicts its sequence");
  if (completed_evidence && !receipt)
    throw std::invalid_argument(
        "application journal step completed evidence lacks receipt authority");
  auto identity = identify_step(
      declaration, sequence, predecessor, state, event, replay_fact, receipt,
      completed_evidence);
  return application_journal_step(
      std::move(identity), std::move(declaration), sequence,
      std::move(predecessor), state, std::move(event), std::move(replay_fact),
      std::move(receipt), std::move(completed_evidence));
}

application_journal_step::application_journal_step(
    application_journal_step_identity identity,
    application_journal_declaration_identity declaration,
    std::uint64_t sequence,
    std::optional<application_journal_step_identity> predecessor,
    application_journal_state state,
    std::optional<application_journal_event> event,
    application_journal_replay_encoding replay_fact,
    std::optional<application_receipt_identity> receipt,
    std::optional<completed_application_evidence_identity> completed_evidence)
    : identity_(std::move(identity)), declaration_(std::move(declaration)),
      sequence_(sequence), predecessor_(std::move(predecessor)), state_(state),
      event_(std::move(event)), replay_fact_(std::move(replay_fact)),
      receipt_(std::move(receipt)), completed_evidence_(std::move(completed_evidence))
{
}

std::uint16_t application_journal_step::schema_version() const noexcept { return schema_version_; }
const application_journal_step_identity& application_journal_step::identity() const noexcept { return identity_; }
const application_journal_declaration_identity& application_journal_step::declaration() const noexcept { return declaration_; }
std::uint64_t application_journal_step::sequence() const noexcept { return sequence_; }
const std::optional<application_journal_step_identity>& application_journal_step::predecessor() const noexcept { return predecessor_; }
application_journal_state application_journal_step::state() const noexcept { return state_; }
const std::optional<application_journal_event>& application_journal_step::event() const noexcept { return event_; }
const application_journal_replay_encoding& application_journal_step::replay_fact() const noexcept { return replay_fact_; }
const std::optional<application_receipt_identity>& application_journal_step::receipt() const noexcept { return receipt_; }
const std::optional<completed_application_evidence_identity>& application_journal_step::completed_evidence() const noexcept { return completed_evidence_; }

application_journal_cursor application_journal_cursor::initial(
    const application_journal_declaration& declaration,
    application_journal_state state)
{
  if (state != application_journal_state::preparing)
    throw std::invalid_argument(
        "initial application journal cursor must begin in preparing state");
  auto identity = identify_cursor(
      declaration.identity(), 0, std::nullopt, state, std::nullopt,
      std::nullopt);
  return application_journal_cursor(
      std::move(identity), declaration.identity(), 0, std::nullopt, state,
      std::nullopt, std::nullopt);
}

application_journal_cursor application_journal_cursor::advance(
    const application_journal_cursor& current,
    const application_journal_step& step)
{
  if (step.declaration() != current.declaration())
    throw std::invalid_argument(
        "application journal step belongs to another declaration");
  if (step.sequence() != current.step_count())
    throw std::invalid_argument(
        "application journal step is not the exact cursor successor");
  if (step.predecessor() != current.latest_step())
    throw std::invalid_argument(
        "application journal step predecessor differs from the cursor head");
  if (!state_may_follow(current.state(), step.state()))
    throw std::invalid_argument(
        "application journal step has an invalid state transition");
  if (current.receipt() && step.receipt() != current.receipt())
    throw std::invalid_argument(
        "application journal step changes retained receipt authority");
  if (current.completed_evidence() &&
      step.completed_evidence() != current.completed_evidence())
    throw std::invalid_argument(
        "application journal step changes retained completed evidence");

  auto identity = identify_cursor(
      current.declaration(), current.step_count() + 1, step.identity(),
      step.state(), step.receipt(), step.completed_evidence());
  return application_journal_cursor(
      std::move(identity), current.declaration(), current.step_count() + 1,
      step.identity(), step.state(), step.receipt(), step.completed_evidence());
}

application_journal_cursor::application_journal_cursor(
    application_journal_cursor_identity identity,
    application_journal_declaration_identity declaration,
    std::uint64_t step_count,
    std::optional<application_journal_step_identity> latest_step,
    application_journal_state state,
    std::optional<application_receipt_identity> receipt,
    std::optional<completed_application_evidence_identity> completed_evidence)
    : identity_(std::move(identity)), declaration_(std::move(declaration)),
      step_count_(step_count), latest_step_(std::move(latest_step)), state_(state),
      receipt_(std::move(receipt)), completed_evidence_(std::move(completed_evidence))
{
}

std::uint16_t application_journal_cursor::schema_version() const noexcept { return schema_version_; }
const application_journal_cursor_identity& application_journal_cursor::identity() const noexcept { return identity_; }
const application_journal_declaration_identity& application_journal_cursor::declaration() const noexcept { return declaration_; }
std::uint64_t application_journal_cursor::step_count() const noexcept { return step_count_; }
const std::optional<application_journal_step_identity>& application_journal_cursor::latest_step() const noexcept { return latest_step_; }
application_journal_state application_journal_cursor::state() const noexcept { return state_; }
const std::optional<application_receipt_identity>& application_journal_cursor::receipt() const noexcept { return receipt_; }
const std::optional<completed_application_evidence_identity>& application_journal_cursor::completed_evidence() const noexcept { return completed_evidence_; }


application_journal_cursor
detail::application_journal_cursor_codec_access::restore(
    application_journal_declaration_identity declaration,
    std::uint64_t step_count,
    std::optional<application_journal_step_identity> latest_step,
    application_journal_state state,
    std::optional<application_receipt_identity> receipt,
    std::optional<completed_application_evidence_identity> completed_evidence)
{
  if ((step_count == 0) != !latest_step.has_value())
    throw std::invalid_argument(
        "application journal cursor head contradicts its step count");
  if (step_count == 0 &&
      (state != application_journal_state::preparing || receipt ||
       completed_evidence))
  {
    throw std::invalid_argument(
        "empty application journal cursor is not initial");
  }
  if (completed_evidence && !receipt)
    throw std::invalid_argument(
        "application journal cursor completed evidence lacks receipt authority");

  auto identity = identify_cursor(
      declaration, step_count, latest_step, state, receipt, completed_evidence);
  return application_journal_cursor(
      std::move(identity), std::move(declaration), step_count,
      std::move(latest_step), state, std::move(receipt),
      std::move(completed_evidence));
}

application_journal_store::~application_journal_store() = default;

} // namespace pkgapply
