// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal_transport.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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
  const auto request = application_identity<pkgapply::application_request_identity>(1);
  const auto target = application_identity<pkgapply::application_target_context_identity>(2);
  const auto backend = application_identity<pkgapply::mutation_backend_identity>(3);
  const auto lease = application_identity<pkgapply::mutation_lease_instance_identity>(4);
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

} // namespace

int main()
{
  std::vector<pkgapply::application_journal_effect> effects;
  effects.push_back(pkgapply::application_journal_effect::make(
      0, pkgapply::application_journal_effect_kind::publish_active_object,
      pkgplan::package_path::parse("usr/bin/tool")));
  effects.push_back(pkgapply::application_journal_effect::make(
      1, pkgapply::application_journal_effect_kind::observe_result,
      pkgplan::package_path::parse("usr/bin/tool")));

  pkgapply::application_journal_replay_encoding seed{
      std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};
  const auto declaration = pkgapply::application_journal_declaration::make(
      make_header(), effects, seed);
  require(declaration.effects().size() == 2 && declaration.replay_seed() == seed,
          "journal declaration did not retain fixed authority");

  auto cursor = pkgapply::application_journal_cursor::initial(declaration);
  require(cursor.step_count() == 0 && !cursor.latest_step(),
          "initial journal cursor is not bounded and empty");

  bool skipped_prepared_refused = false;
  try {
    const auto skipped = pkgapply::application_journal_step::make(
        declaration.identity(), 0, std::nullopt,
        pkgapply::application_journal_state::mutating);
    (void)pkgapply::application_journal_cursor::advance(cursor, skipped);
  }
  catch (const std::invalid_argument&) {
    skipped_prepared_refused = true;
  }
  require(skipped_prepared_refused,
          "journal cursor admitted preparing to mutating transition");

  const auto prepared = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  cursor = pkgapply::application_journal_cursor::advance(cursor, prepared);
  require(cursor.step_count() == 1 && cursor.latest_step() == prepared.identity() &&
              cursor.state() == pkgapply::application_journal_state::prepared,
          "journal cursor did not retain state-only preparation completion");

  const auto intent = pkgapply::application_journal_step::make(
      declaration.identity(), 1, prepared.identity(),
      pkgapply::application_journal_state::mutating,
      pkgapply::application_journal_event(
          0, pkgapply::application_journal_event_kind::intent,
          effects[0].identity()));
  cursor = pkgapply::application_journal_cursor::advance(cursor, intent);
  require(cursor.step_count() == 2 && cursor.latest_step() == intent.identity(),
          "journal cursor did not advance to exact immutable effect intent");

  const auto completed = pkgapply::application_journal_step::make(
      declaration.identity(), 2, intent.identity(),
      pkgapply::application_journal_state::effects_visible,
      pkgapply::application_journal_event(
          1, pkgapply::application_journal_event_kind::completed,
          effects[0].identity(),
          {application_identity<pkgapply::application_backend_evidence_identity>(20)}),
      {std::byte{0x55}});
  const auto second = pkgapply::application_journal_cursor::advance(cursor, completed);
  require(second.step_count() == 3 && second.latest_step() == completed.identity(),
          "journal cursor did not retain exact terminal effect step");

  bool branch_refused = false;
  try {
    const auto branch = pkgapply::application_journal_step::make(
        declaration.identity(), 2,
        application_identity<pkgapply::application_journal_step_identity>(99),
        pkgapply::application_journal_state::effects_visible);
    (void)pkgapply::application_journal_cursor::advance(cursor, branch);
  }
  catch (const std::invalid_argument&) {
    branch_refused = true;
  }
  require(branch_refused, "journal cursor admitted a foreign predecessor branch");

  bool gap_refused = false;
  try {
    const auto gap = pkgapply::application_journal_step::make(
        declaration.identity(), 4, completed.identity(),
        pkgapply::application_journal_state::result_observed);
    (void)pkgapply::application_journal_cursor::advance(second, gap);
  }
  catch (const std::invalid_argument&) {
    gap_refused = true;
  }
  require(gap_refused, "journal cursor admitted a skipped sequence");

  bool state_regression_refused = false;
  try {
    const auto regression = pkgapply::application_journal_step::make(
        declaration.identity(), 3, completed.identity(),
        pkgapply::application_journal_state::mutating);
    (void)pkgapply::application_journal_cursor::advance(second, regression);
  }
  catch (const std::invalid_argument&) {
    state_regression_refused = true;
  }
  require(state_regression_refused,
          "journal cursor admitted an invalid lifecycle regression");

  bool terminal_without_receipt_refused = false;
  try {
    (void)pkgapply::application_journal_step::make(
        declaration.identity(), 3, completed.identity(),
        pkgapply::application_journal_state::application_completed,
        std::nullopt, {}, std::nullopt,
        application_identity<pkgapply::completed_application_evidence_identity>(40));
  }
  catch (const std::invalid_argument&) {
    terminal_without_receipt_refused = true;
  }
  require(terminal_without_receipt_refused,
          "journal step admitted completed evidence without receipt authority");

  return 0;
}
