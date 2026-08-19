// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/scripted_journal_store.h"

#include <libpkgapply/restart.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
          application_identity<pkgapply::state_projection_evidence_identity>(9)),
      lease, backend);
}

pkgapply::application_journal_declaration declaration()
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
    pkgapply::test::scripted_journal_store& store,
    pkgapply::application_journal_cursor& cursor,
    const pkgapply::application_journal_step& step)
{
  const auto published = store.publish_step(step);
  require(published.identity() == step.identity(),
          "scripted store changed immutable step");
  const auto next = pkgapply::application_journal_cursor::advance(cursor, step);
  const auto durable = store.compare_and_publish_cursor(cursor.identity(), next);
  require(durable.identity() == next.identity(),
          "scripted store changed cursor successor");
  cursor = next;
}

void test_missing_initial_cursor_is_owner_recovered()
{
  pkgapply::test::scripted_journal_store store;
  const auto durable = store.publish_declaration(declaration());

  const auto record = pkgapply::rehydrate_application_journal(
      store, durable.identity());
  require(record.header().identity() == durable.header().identity() &&
              record.state() == pkgapply::application_journal_state::preparing &&
              record.events().empty(),
          "owner rehydration did not reconstruct empty durable history");

  const auto cursor = store.load_cursor(durable.identity());
  require(cursor && cursor->step_count() == 0 &&
              cursor->state() == pkgapply::application_journal_state::preparing,
          "owner rehydration did not publish the missing initial cursor");
}

void test_terminal_orphan_is_adopted_before_classification()
{
  pkgapply::test::scripted_journal_store store;
  const auto durable = store.publish_declaration(declaration());
  auto cursor = pkgapply::application_journal_cursor::initial(durable);
  cursor = store.compare_and_publish_cursor(std::nullopt, cursor);

  const auto prepared = pkgapply::application_journal_step::make(
      durable.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  publish_committed(store, cursor, prepared);

  const auto intent = pkgapply::application_journal_step::make(
      durable.identity(), 1, prepared.identity(),
      pkgapply::application_journal_state::mutating,
      pkgapply::application_journal_event(
          0, pkgapply::application_journal_event_kind::intent,
          durable.effects()[0].identity()));
  publish_committed(store, cursor, intent);

  const auto completed = pkgapply::application_journal_step::make(
      durable.identity(), 2, intent.identity(),
      pkgapply::application_journal_state::effects_visible,
      pkgapply::application_journal_event(
          1, pkgapply::application_journal_event_kind::completed,
          durable.effects()[0].identity()));
  publish_committed(store, cursor, completed);

  const auto observed = pkgapply::application_journal_step::make(
      durable.identity(), 3, completed.identity(),
      pkgapply::application_journal_state::result_observed);
  publish_committed(store, cursor, observed);

  const auto seal_intent = pkgapply::application_journal_step::make(
      durable.identity(), 4, observed.identity(),
      pkgapply::application_journal_state::result_observed,
      pkgapply::application_journal_event(
          2, pkgapply::application_journal_event_kind::intent,
          durable.effects()[1].identity()));
  publish_committed(store, cursor, seal_intent);

  const auto terminal = pkgapply::application_journal_step::make(
      durable.identity(), 5, seal_intent.identity(),
      pkgapply::application_journal_state::application_completed,
      pkgapply::application_journal_event(
          3, pkgapply::application_journal_event_kind::completed,
          durable.effects()[1].identity()),
      {}, application_identity<pkgapply::application_receipt_identity>(40),
      application_identity<
          pkgapply::completed_application_evidence_identity>(41));
  static_cast<void>(store.publish_step(terminal));

  const auto record = pkgapply::rehydrate_application_journal(
      store, durable.identity());
  const auto assessment = pkgapply::assess_application_restart(record);
  require(record.state() ==
              pkgapply::application_journal_state::application_completed &&
              record.receipt() && record.completed_evidence() &&
              assessment.disposition() ==
                  pkgapply::application_restart_disposition::terminal,
          "owner rehydration did not adopt and classify terminal orphan");

  const auto adopted = store.load_cursor(durable.identity());
  require(adopted && adopted->step_count() == 6 &&
              adopted->latest_step() == terminal.identity(),
          "terminal crash orphan was not admitted into bounded cursor");
}

void test_missing_committed_step_fails_closed()
{
  pkgapply::test::scripted_journal_store store;
  const auto durable = store.publish_declaration(declaration());
  auto cursor = pkgapply::application_journal_cursor::initial(durable);
  cursor = store.compare_and_publish_cursor(std::nullopt, cursor);

  const auto prepared = pkgapply::application_journal_step::make(
      durable.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  publish_committed(store, cursor, prepared);
  store.erase_step(durable.identity(), 0);

  try {
    static_cast<void>(pkgapply::rehydrate_application_journal(
        store, durable.identity()));
  }
  catch (const std::invalid_argument&) {
    return;
  }
  require(false, "owner rehydration accepted a missing committed step");
}

} // namespace

int main()
{
  test_missing_initial_cursor_is_owner_recovered();
  test_terminal_orphan_is_adopted_before_classification();
  test_missing_committed_step_fails_closed();
  return 0;
}
