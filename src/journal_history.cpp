// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal_history.h"

#include <stdexcept>
#include <utility>

namespace pkgapply::detail {
namespace {

bool success_effect_required(application_journal_effect_kind kind) noexcept
{
  return kind != application_journal_effect_kind::recover_active_object &&
         kind !=
             application_journal_effect_kind::synchronize_recovered_namespace;
}

bool successful_state(application_journal_state state) noexcept
{
  return state == application_journal_state::application_completed ||
         state == application_journal_state::external_resolution_pending ||
         state == application_journal_state::finalized;
}

} // namespace

std::size_t
application_journal_history::digest_hash::operator()(
    const application_journal_effect_identity& identity) const noexcept
{
  // FNV-1a over the fixed SHA-256 payload. Hashing is an in-memory lookup
  // accelerator only; identity equality remains the semantic discriminator.
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

application_journal_history
application_journal_history::initial(
    application_journal_declaration declaration)
{
  auto cursor = application_journal_cursor::initial(declaration);
  return application_journal_history(
      std::move(declaration), std::move(cursor));
}

application_journal_history
application_journal_history::load(
    application_journal_store& store,
    const application_journal_declaration_identity& declaration_identity)
{
  auto declaration = store.load_declaration(declaration_identity);
  if (!declaration || declaration->identity() != declaration_identity)
    throw std::invalid_argument(
        "application journal declaration is missing or changed");

  auto loaded_cursor = store.load_cursor(declaration_identity);
  if (!loaded_cursor) {
    auto initial_cursor = application_journal_cursor::initial(*declaration);
    loaded_cursor = store.compare_and_publish_cursor(
        std::nullopt, initial_cursor);
    if (loaded_cursor->identity() != initial_cursor.identity())
      throw std::logic_error(
          "journal store changed the initial application cursor");
  }
  if (loaded_cursor->declaration() != declaration_identity)
    throw std::invalid_argument(
        "application journal cursor belongs to another declaration");

  const application_journal_cursor committed = *loaded_cursor;
  application_journal_history history = initial(std::move(*declaration));
  for (std::uint64_t sequence = 0;
       sequence < committed.step_count();
       ++sequence) {
    auto step = store.load_step(declaration_identity, sequence);
    if (!step)
      throw std::invalid_argument(
          "application journal committed sequence contains a missing step");
    history.append(*step);
  }
  if (history.cursor().identity() != committed.identity())
    throw std::invalid_argument(
        "application journal cursor disagrees with committed step history");

  // The append protocol can leave at most one durable successor beyond the
  // cursor: another semantic step cannot be produced until the prior cursor
  // advance commits. Probe exactly that sequence and never enumerate storage.
  auto orphan = store.load_step(declaration_identity, committed.step_count());
  if (orphan) {
    const application_journal_cursor candidate = history.validate(*orphan);
    const auto adopted = store.compare_and_publish_cursor(
        committed.identity(), candidate);
    if (adopted.identity() != candidate.identity())
      throw std::logic_error(
          "journal store changed an adopted application cursor");
    history.append(*orphan);
  }

  return history;
}

application_journal_history::application_journal_history(
    application_journal_declaration declaration,
    application_journal_cursor cursor)
    : declaration_(std::move(declaration)), cursor_(std::move(cursor))
{
  progress_.resize(declaration_.effects().size());
  effect_ordinals_.reserve(declaration_.effects().size());
  for (std::size_t ordinal = 0;
       ordinal < declaration_.effects().size();
       ++ordinal) {
    const auto& effect = declaration_.effects()[ordinal];
    const auto inserted = effect_ordinals_.emplace(effect.identity(), ordinal);
    if (!inserted.second)
      throw std::invalid_argument(
          "application journal declaration repeats an effect identity");
    if (success_effect_required(effect.kind()))
      ++required_success_effects_;
  }
}

std::size_t
application_journal_history::validate_event(
    const application_journal_event& event) const
{
  if (event.sequence() != events_.size())
    throw std::invalid_argument(
        "application journal step event sequence is not consecutive");

  const auto found = effect_ordinals_.find(event.effect());
  if (found == effect_ordinals_.end())
    throw std::invalid_argument(
        "application journal step event cites an unknown effect");

  const auto& progress = progress_[found->second];
  if (event.kind() == application_journal_event_kind::intent) {
    if (progress.intended || progress.terminal)
      throw std::invalid_argument(
          "application journal step repeats or delays an effect intent");
  }
  else {
    if (!progress.intended)
      throw std::invalid_argument(
          "application journal step terminal event precedes intent");
    if (progress.terminal)
      throw std::invalid_argument(
          "application journal step repeats an effect terminal event");
  }
  return found->second;
}

void
application_journal_history::validate_resolution(
    const application_journal_cursor& candidate,
    std::size_t completed_success_effects) const
{
  if (candidate.completed_evidence() && !candidate.receipt())
    throw std::invalid_argument(
        "application journal completed evidence lacks its receipt");

  if (successful_state(candidate.state())) {
    if (!candidate.receipt() || !candidate.completed_evidence())
      throw std::invalid_argument(
          "completed application journal state lacks terminal authority");
    if (completed_success_effects != required_success_effects_)
      throw std::invalid_argument(
          "completed application journal state has unfinished effects");
  }
  else if (candidate.completed_evidence()) {
    throw std::invalid_argument(
        "non-completed application journal state retains completed evidence");
  }

  if (candidate.state() == application_journal_state::recovered &&
      !candidate.receipt())
  {
    throw std::invalid_argument(
        "recovered application journal state lacks a failure receipt");
  }
}

application_journal_history::step_validation
application_journal_history::validate_step(
    const application_journal_step& step) const
{
  application_journal_cursor candidate =
      application_journal_cursor::advance(cursor_, step);

  std::optional<std::size_t> ordinal;
  std::size_t completed = completed_success_effects_;
  if (step.event()) {
    ordinal = validate_event(*step.event());
    if (step.event()->kind() == application_journal_event_kind::completed &&
        success_effect_required(declaration_.effects()[*ordinal].kind()))
    {
      ++completed;
    }
  }

  if ((!step.event() ||
       step.event()->kind() == application_journal_event_kind::intent) &&
      !step.replay_fact().empty())
  {
    throw std::invalid_argument(
        "application journal replay fact is not a terminal effect result");
  }

  const bool introduces_receipt = !cursor_.receipt() && candidate.receipt();
  if (introduces_receipt && step.event()) {
    if (step.event()->kind() != application_journal_event_kind::completed ||
        declaration_.effects()[*ordinal].kind() !=
            application_journal_effect_kind::seal_receipt)
    {
      throw std::invalid_argument(
          "application journal receipt event is not receipt sealing");
    }
  }
  if (introduces_receipt && successful_state(candidate.state()) &&
      !step.event())
  {
    throw std::invalid_argument(
        "successful application journal receipt lacks receipt sealing");
  }
  validate_resolution(candidate, completed);

  return {std::move(candidate), ordinal, completed};
}

application_journal_cursor
application_journal_history::validate(
    const application_journal_step& step) const
{
  return validate_step(step).candidate;
}

void
application_journal_history::append(const application_journal_step& step)
{
  auto validated = validate_step(step);

  if (step.event()) {
    auto& progress = progress_[*validated.ordinal];
    if (step.event()->kind() == application_journal_event_kind::intent)
      progress.intended = true;
    else
      progress.terminal = step.event()->kind();
    events_.push_back(*step.event());
  }
  completed_success_effects_ = validated.completed_success_effects;
  cursor_ = std::move(validated.candidate);
}

const application_journal_declaration&
application_journal_history::declaration() const noexcept
{
  return declaration_;
}

const application_journal_header&
application_journal_history::header() const noexcept
{
  return declaration_.header();
}

const std::vector<application_journal_effect>&
application_journal_history::effects() const noexcept
{
  return declaration_.effects();
}

const application_journal_cursor&
application_journal_history::cursor() const noexcept
{
  return cursor_;
}

application_journal_state
application_journal_history::state() const noexcept
{
  return cursor_.state();
}

const std::vector<application_journal_event>&
application_journal_history::events() const noexcept
{
  return events_;
}

const std::optional<application_receipt_identity>&
application_journal_history::receipt() const noexcept
{
  return cursor_.receipt();
}

const std::optional<completed_application_evidence_identity>&
application_journal_history::completed_evidence() const noexcept
{
  return cursor_.completed_evidence();
}

application_journal_record
application_journal_history::snapshot() const
{
  return application_journal_record::make(
      declaration_.header(), cursor_.state(), declaration_.effects(), events_,
      cursor_.receipt(), cursor_.completed_evidence());
}

} // namespace pkgapply::detail
