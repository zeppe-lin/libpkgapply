// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "journal_history.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool value, std::string_view message)
{
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template<class Identity>
Identity application_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const auto byte = static_cast<std::uint8_t>(value + index);
    text += hex[(byte >> 4) & 0x0fU];
    text += hex[byte & 0x0fU];
  }
  return Identity::parse(text);
}

template<class Identity>
Identity planning_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return Identity::from_sha256(bytes);
}

pkgapply::application_journal_header make_header()
{
  pkgapply::application_attempt_nonce::byte_array nonce{};
  for (std::size_t index = 0; index < nonce.size(); ++index)
    nonce[index] = static_cast<std::uint8_t>(31 + index);
  const auto request =
      application_identity<pkgapply::application_request_identity>(1);
  const auto target =
      application_identity<pkgapply::application_target_context_identity>(2);
  const auto backend =
      application_identity<pkgapply::mutation_backend_identity>(3);
  const auto lease =
      application_identity<pkgapply::mutation_lease_instance_identity>(4);
  const auto attempt = pkgapply::application_attempt::make(
      request, target, backend,
      pkgapply::application_attempt_nonce::from_bytes(nonce));
  return pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install, request,
      planning_identity<pkgplan::operation_plan_identity>(5), attempt, target,
      application_identity<pkgapply::application_execution_control_identity>(6),
      pkgapply::lease_bound_state_projection::make(
          lease,
          planning_identity<pkgplan::installed_state_snapshot_identity>(7),
          planning_identity<pkgplan::ownership_inventory_identity>(8),
          pkgapply::state_projection_completeness::complete, {},
          application_identity<
              pkgapply::state_projection_evidence_identity>(9)),
      lease, backend);
}

class memory_store final : public pkgapply::application_journal_store {
public:
  pkgapply::application_journal_declaration publish_declaration(
      const pkgapply::application_journal_declaration& declaration) override
  {
    if (declaration_ && declaration_->identity() != declaration.identity())
      throw std::logic_error("conflicting declaration");
    declaration_ = declaration;
    return *declaration_;
  }

  pkgapply::application_journal_step publish_step(
      const pkgapply::application_journal_step& step) override
  {
    const auto found = steps_.find(step.sequence());
    if (found != steps_.end() && found->second.identity() != step.identity())
      throw std::logic_error("conflicting step");
    steps_.insert_or_assign(step.sequence(), step);
    return steps_.at(step.sequence());
  }

  pkgapply::application_journal_cursor compare_and_publish_cursor(
      const std::optional<
          pkgapply::application_journal_cursor_identity>& expected,
      const pkgapply::application_journal_cursor& cursor) override
  {
    ++cursor_publications_;
    const auto current = cursor_ ? std::optional(cursor_->identity())
                                 : std::nullopt;
    if (current != expected)
      throw std::logic_error("cursor compare failed");
    cursor_ = cursor;
    return *cursor_;
  }

  std::optional<pkgapply::application_journal_declaration> load_declaration(
      const pkgapply::application_journal_declaration_identity&
          identity) override
  {
    ++declaration_loads_;
    if (!declaration_ || declaration_->identity() != identity)
      return std::nullopt;
    return declaration_;
  }

  std::optional<pkgapply::application_journal_cursor> load_cursor(
      const pkgapply::application_journal_declaration_identity&
          declaration) override
  {
    ++cursor_loads_;
    if (!cursor_ || cursor_->declaration() != declaration)
      return std::nullopt;
    return cursor_;
  }

  std::optional<pkgapply::application_journal_step> load_step(
      const pkgapply::application_journal_declaration_identity& declaration,
      std::uint64_t sequence) override
  {
    ++step_loads_;
    if (!declaration_ || declaration_->identity() != declaration)
      return std::nullopt;
    const auto found = steps_.find(sequence);
    return found == steps_.end()
        ? std::nullopt
        : std::optional(found->second);
  }

  void erase_step(std::uint64_t sequence) { steps_.erase(sequence); }

  [[nodiscard]] std::size_t step_loads() const noexcept { return step_loads_; }
  [[nodiscard]] std::size_t cursor_publications() const noexcept
  {
    return cursor_publications_;
  }

private:
  std::optional<pkgapply::application_journal_declaration> declaration_;
  std::map<std::uint64_t, pkgapply::application_journal_step> steps_;
  std::optional<pkgapply::application_journal_cursor> cursor_;
  std::size_t declaration_loads_ = 0;
  std::size_t cursor_loads_ = 0;
  std::size_t step_loads_ = 0;
  std::size_t cursor_publications_ = 0;
};

pkgapply::application_journal_declaration small_declaration()
{
  std::vector<pkgapply::application_journal_effect> effects;
  effects.push_back(pkgapply::application_journal_effect::make(
      0, pkgapply::application_journal_effect_kind::publish_active_object,
      pkgplan::package_path::parse("usr/bin/tool")));
  effects.push_back(pkgapply::application_journal_effect::make(
      1, pkgapply::application_journal_effect_kind::seal_receipt));
  return pkgapply::application_journal_declaration::make(
      make_header(), std::move(effects));
}

void publish_committed(
    memory_store& store,
    pkgapply::application_journal_cursor& cursor,
    const pkgapply::application_journal_step& step)
{
  const auto published = store.publish_step(step);
  require(published.identity() == step.identity(),
          "memory store changed immutable step");
  const auto next = pkgapply::application_journal_cursor::advance(cursor, step);
  const auto durable =
      store.compare_and_publish_cursor(cursor.identity(), next);
  require(durable.identity() == next.identity(),
          "memory store changed cursor successor");
  cursor = next;
}

void test_exact_rehydration_and_orphan_adoption()
{
  memory_store store;
  const auto declaration = store.publish_declaration(small_declaration());
  auto cursor = pkgapply::application_journal_cursor::initial(declaration);
  cursor = store.compare_and_publish_cursor(std::nullopt, cursor);

  const auto prepared = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  publish_committed(store, cursor, prepared);

  const auto intent = pkgapply::application_journal_step::make(
      declaration.identity(), 1, prepared.identity(),
      pkgapply::application_journal_state::mutating,
      pkgapply::application_journal_event(
          0, pkgapply::application_journal_event_kind::intent,
          declaration.effects()[0].identity()));
  publish_committed(store, cursor, intent);

  const auto completed = pkgapply::application_journal_step::make(
      declaration.identity(), 2, intent.identity(),
      pkgapply::application_journal_state::effects_visible,
      pkgapply::application_journal_event(
          1, pkgapply::application_journal_event_kind::completed,
          declaration.effects()[0].identity()));
  publish_committed(store, cursor, completed);

  const auto observed = pkgapply::application_journal_step::make(
      declaration.identity(), 3, completed.identity(),
      pkgapply::application_journal_state::result_observed);
  publish_committed(store, cursor, observed);

  const auto seal_intent = pkgapply::application_journal_step::make(
      declaration.identity(), 4, observed.identity(),
      pkgapply::application_journal_state::result_observed,
      pkgapply::application_journal_event(
          2, pkgapply::application_journal_event_kind::intent,
          declaration.effects()[1].identity()));
  publish_committed(store, cursor, seal_intent);

  const auto terminal = pkgapply::application_journal_step::make(
      declaration.identity(), 5, seal_intent.identity(),
      pkgapply::application_journal_state::application_completed,
      pkgapply::application_journal_event(
          3, pkgapply::application_journal_event_kind::completed,
          declaration.effects()[1].identity()),
      {}, application_identity<pkgapply::application_receipt_identity>(40),
      application_identity<
          pkgapply::completed_application_evidence_identity>(41));
  static_cast<void>(store.publish_step(terminal));
  const auto publications_before = store.cursor_publications();

  auto history = pkgapply::detail::application_journal_history::load(
      store, declaration.identity());
  require(history.cursor().step_count() == 6 &&
              history.cursor().latest_step() == terminal.identity(),
          "rehydration did not adopt exact durable successor");
  require(store.cursor_publications() == publications_before + 1,
          "orphan adoption did not advance the bounded cursor exactly once");
  require(store.step_loads() == 6,
          "rehydration performed more than exact committed plus orphan reads");

  const auto snapshot = history.snapshot();
  require(snapshot.state() ==
              pkgapply::application_journal_state::application_completed &&
              snapshot.events().size() == 4 && snapshot.receipt() &&
              snapshot.completed_evidence(),
          "derived snapshot disagrees with append-only history");
}

void test_missing_committed_step_refused()
{
  memory_store store;
  const auto declaration = store.publish_declaration(small_declaration());
  auto cursor = pkgapply::application_journal_cursor::initial(declaration);
  cursor = store.compare_and_publish_cursor(std::nullopt, cursor);

  const auto prepared = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  publish_committed(store, cursor, prepared);
  const auto intent = pkgapply::application_journal_step::make(
      declaration.identity(), 1, prepared.identity(),
      pkgapply::application_journal_state::mutating,
      pkgapply::application_journal_event(
          0, pkgapply::application_journal_event_kind::intent,
          declaration.effects()[0].identity()));
  publish_committed(store, cursor, intent);
  store.erase_step(0);

  bool refused = false;
  try {
    static_cast<void>(pkgapply::detail::application_journal_history::load(
        store, declaration.identity()));
  }
  catch (const std::invalid_argument&) {
    refused = true;
  }
  require(refused, "rehydration accepted a missing committed step");
}

void test_invalid_orphan_refused()
{
  memory_store store;
  const auto declaration = store.publish_declaration(small_declaration());
  auto cursor = pkgapply::application_journal_cursor::initial(declaration);
  cursor = store.compare_and_publish_cursor(std::nullopt, cursor);

  const auto branch = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared,
      pkgapply::application_journal_event(
          1, pkgapply::application_journal_event_kind::intent,
          declaration.effects()[0].identity()));
  static_cast<void>(store.publish_step(branch));
  const auto publications_before = store.cursor_publications();

  bool refused = false;
  try {
    static_cast<void>(pkgapply::detail::application_journal_history::load(
        store, declaration.identity()));
  }
  catch (const std::invalid_argument&) {
    refused = true;
  }
  require(refused, "rehydration adopted an invalid orphan step");
  require(store.cursor_publications() == publications_before,
          "invalid orphan changed cursor authority");
}


void test_validation_does_not_advance_memory()
{
  const auto declaration = small_declaration();
  auto history = pkgapply::detail::application_journal_history::initial(
      declaration);
  const auto prepared = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);

  const auto candidate = history.validate(prepared);
  require(history.cursor().step_count() == 0 &&
              history.state() == pkgapply::application_journal_state::preparing &&
              candidate.step_count() == 1 &&
              candidate.state() == pkgapply::application_journal_state::prepared,
          "journal validation advanced the in-memory committed head");

  history.append(prepared);
  require(history.cursor().identity() == candidate.identity(),
          "journal commit disagreed with the prevalidated cursor");
}

void test_incremental_semantics_fail_closed()
{
  const auto declaration = small_declaration();
  auto history = pkgapply::detail::application_journal_history::initial(
      declaration);
  const auto prepared = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  history.append(prepared);

  bool terminal_without_intent = false;
  try {
    const auto invalid = pkgapply::application_journal_step::make(
        declaration.identity(), 1, prepared.identity(),
        pkgapply::application_journal_state::mutating,
        pkgapply::application_journal_event(
            0, pkgapply::application_journal_event_kind::completed,
            declaration.effects()[0].identity()));
    history.append(invalid);
  }
  catch (const std::invalid_argument&) {
    terminal_without_intent = true;
  }
  require(terminal_without_intent,
          "incremental history accepted a terminal event before intent");

  bool unknown_effect = false;
  try {
    const auto invalid = pkgapply::application_journal_step::make(
        declaration.identity(), 1, prepared.identity(),
        pkgapply::application_journal_state::mutating,
        pkgapply::application_journal_event(
            0, pkgapply::application_journal_event_kind::intent,
            application_identity<
                pkgapply::application_journal_effect_identity>(80)));
    history.append(invalid);
  }
  catch (const std::invalid_argument&) {
    unknown_effect = true;
  }
  require(unknown_effect,
          "incremental history accepted an event for an unknown effect");

  const auto intent = pkgapply::application_journal_step::make(
      declaration.identity(), 1, prepared.identity(),
      pkgapply::application_journal_state::mutating,
      pkgapply::application_journal_event(
          0, pkgapply::application_journal_event_kind::intent,
          declaration.effects()[0].identity()));
  history.append(intent);

  bool duplicate_intent = false;
  try {
    const auto invalid = pkgapply::application_journal_step::make(
        declaration.identity(), 2, intent.identity(),
        pkgapply::application_journal_state::mutating,
        pkgapply::application_journal_event(
            1, pkgapply::application_journal_event_kind::intent,
            declaration.effects()[0].identity()));
    history.append(invalid);
  }
  catch (const std::invalid_argument&) {
    duplicate_intent = true;
  }
  require(duplicate_intent,
          "incremental history accepted a duplicate effect intent");
}

void test_large_history_is_exactly_indexed()
{
  constexpr std::size_t effect_count = 10000;
  std::vector<pkgapply::application_journal_effect> effects;
  effects.reserve(effect_count);
  for (std::size_t ordinal = 0; ordinal < effect_count; ++ordinal) {
    effects.push_back(pkgapply::application_journal_effect::make(
        ordinal,
        pkgapply::application_journal_effect_kind::synchronize_active_namespace));
  }

  memory_store store;
  const auto declaration = store.publish_declaration(
      pkgapply::application_journal_declaration::make(
          make_header(), std::move(effects)));
  const auto initial =
      pkgapply::application_journal_cursor::initial(declaration);
  static_cast<void>(store.compare_and_publish_cursor(std::nullopt, initial));

  auto committed = initial;
  std::optional<pkgapply::application_journal_step_identity> predecessor;
  std::uint64_t sequence = 0;
  std::uint64_t event_sequence = 0;
  for (const auto& effect : declaration.effects()) {
    const auto intent = pkgapply::application_journal_step::make(
        declaration.identity(), sequence++, predecessor,
        pkgapply::application_journal_state::preparing,
        pkgapply::application_journal_event(
            event_sequence++, pkgapply::application_journal_event_kind::intent,
            effect.identity()));
    static_cast<void>(store.publish_step(intent));
    committed = pkgapply::application_journal_cursor::advance(
        committed, intent);
    predecessor = intent.identity();

    const auto completed = pkgapply::application_journal_step::make(
        declaration.identity(), sequence++, predecessor,
        pkgapply::application_journal_state::preparing,
        pkgapply::application_journal_event(
            event_sequence++,
            pkgapply::application_journal_event_kind::completed,
            effect.identity()));
    static_cast<void>(store.publish_step(completed));
    committed = pkgapply::application_journal_cursor::advance(
        committed, completed);
    predecessor = completed.identity();
  }
  static_cast<void>(store.compare_and_publish_cursor(
      initial.identity(), committed));

  const auto loads_before = store.step_loads();
  const auto history = pkgapply::detail::application_journal_history::load(
      store, declaration.identity());
  require(history.cursor().step_count() == 20000 &&
              history.events().size() == 20000,
          "large retained history did not rehydrate completely");
  require(store.step_loads() - loads_before == 20001,
          "large rehydration did not use exact sequence reads plus one "
          "orphan probe");
}

} // namespace

int main()
{
  test_exact_rehydration_and_orphan_adoption();
  test_missing_committed_step_refused();
  test_invalid_orphan_refused();
  test_validation_does_not_advance_memory();
  test_incremental_semantics_fail_closed();
  test_large_history_is_exactly_indexed();
  return 0;
}
