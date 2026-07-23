// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void
require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template<class Identity>
Identity
application_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hexadecimal[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const auto byte = static_cast<std::uint8_t>(value + index);
    text += hexadecimal[(byte >> 4) & 0x0fU];
    text += hexadecimal[byte & 0x0fU];
  }
  return Identity::parse(text);
}

template<class Identity>
Identity
planning_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return Identity::from_sha256(bytes);
}

pkgapply::application_journal_header
header()
{
  return pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install,
      application_identity<pkgapply::application_request_identity>(1),
      planning_identity<pkgplan::operation_plan_identity>(2),
      application_identity<pkgapply::application_attempt_identity>(3),
      application_identity<pkgapply::application_target_context_identity>(4),
      application_identity<
          pkgapply::application_execution_control_identity>(5),
      application_identity<
          pkgapply::lease_bound_state_projection_identity>(6),
      application_identity<pkgapply::mutation_lease_instance_identity>(7),
      application_identity<pkgapply::mutation_backend_identity>(8));
}

std::vector<pkgapply::application_journal_effect>
effects()
{
  return {
      pkgapply::application_journal_effect::make(
          0,
          pkgapply::application_journal_effect_kind::publish_active_object,
          pkgplan::package_path::parse("usr/bin/tool")),
      pkgapply::application_journal_effect::make(
          1,
          pkgapply::application_journal_effect_kind::synchronize_journal),
  };
}

std::vector<pkgapply::application_journal_event>
events(const std::vector<pkgapply::application_journal_effect>& effects)
{
  return {
      {0, pkgapply::application_journal_event_kind::intent,
       effects[0].identity()},
      {1, pkgapply::application_journal_event_kind::completed,
       effects[0].identity(),
       {application_identity<
           pkgapply::application_backend_evidence_identity>(20)}},
      {2, pkgapply::application_journal_event_kind::intent,
       effects[1].identity()},
      {3, pkgapply::application_journal_event_kind::completed,
       effects[1].identity()},
  };
}

} // namespace

int
main()
{
  const auto journal_header = header();
  const auto journal_effects = effects();
  const auto journal_events = events(journal_effects);
  const auto receipt =
      application_identity<pkgapply::application_receipt_identity>(30);
  const auto evidence = application_identity<
      pkgapply::completed_application_evidence_identity>(31);

  const auto record = pkgapply::application_journal_record::make(
      journal_header,
      pkgapply::application_journal_state::external_resolution_pending,
      journal_effects,
      journal_events,
      receipt,
      evidence);

  require(journal_header.identity().string() == "v1:sha256:db0547334422d489e21888814bf1e5c80195c280bca4683d59c5529e5c976bea",
          "application journal header identity vector changed");
  require(journal_effects[0].identity().string() == "v1:sha256:1e1a549c38805eff0cac6b13aae44814560780bff354342f7d0fd48ea038e228",
          "application journal effect identity vector changed");
  require(record.identity().string() == "v1:sha256:512c0de816897456afe39dc51439caac06b26d99cdb3059bedc984808dab8f3e",
          "application journal record identity vector changed");

  auto reversed = journal_effects;
  std::reverse(reversed.begin(), reversed.end());
  const auto normalized = pkgapply::application_journal_record::make(
      journal_header,
      pkgapply::application_journal_state::external_resolution_pending,
      reversed,
      journal_events,
      receipt,
      evidence);
  require(normalized.identity() == record.identity(),
          "effect input order changed journal record identity");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::application_journal_effect::make(
        3,
        pkgapply::application_journal_effect_kind::synchronize_journal,
        pkgplan::package_path::parse("usr/bin/tool")));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "non-path journal effect accepted a path");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_journal_record::make(
        journal_header,
        pkgapply::application_journal_state::mutating,
        journal_effects,
        {{0, pkgapply::application_journal_event_kind::completed,
          journal_effects[0].identity()}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "journal terminal event preceding intent was accepted");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_journal_record::make(
        journal_header,
        pkgapply::application_journal_state::application_completed,
        journal_effects,
        journal_events,
        receipt,
        std::nullopt));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "completed journal state accepted missing evidence");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_journal_record::make(
        journal_header,
        pkgapply::application_journal_state::mutating,
        journal_effects,
        {{0, pkgapply::application_journal_event_kind::intent,
          application_identity<
              pkgapply::application_journal_effect_identity>(40)}}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "journal event citing unknown effect was accepted");

  rejected = false;
  try {
    auto duplicate_terminal = journal_events;
    duplicate_terminal.push_back(
        {4, pkgapply::application_journal_event_kind::failed,
         journal_effects[1].identity()});
    static_cast<void>(pkgapply::application_journal_record::make(
        journal_header,
        pkgapply::application_journal_state::indeterminate,
        journal_effects,
        std::move(duplicate_terminal)));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "journal effect accepted multiple terminal events");

  return 0;
}
