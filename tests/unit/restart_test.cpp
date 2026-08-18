// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/restart.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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
  pkgapply::application_attempt_nonce::byte_array nonce_bytes{};
  for (std::size_t index = 0; index < nonce_bytes.size(); ++index)
    nonce_bytes[index] = static_cast<std::uint8_t>(20 + index);

  const auto request =
      application_identity<pkgapply::application_request_identity>(1);
  const auto target = application_identity<
      pkgapply::application_target_context_identity>(2);
  const auto backend =
      application_identity<pkgapply::mutation_backend_identity>(3);
  const auto attempt = pkgapply::application_attempt::make(
      request,
      target,
      backend,
      pkgapply::application_attempt_nonce::from_bytes(nonce_bytes));

  return pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install,
      request,
      planning_identity<pkgplan::operation_plan_identity>(4),
      attempt,
      target,
      application_identity<
          pkgapply::application_execution_control_identity>(5),
      pkgapply::lease_bound_state_projection::make(
          application_identity<pkgapply::mutation_lease_instance_identity>(7),
          planning_identity<pkgplan::installed_state_snapshot_identity>(6),
          planning_identity<pkgplan::ownership_inventory_identity>(9),
          pkgapply::state_projection_completeness::complete, {},
          application_identity<pkgapply::state_projection_evidence_identity>(10)),
      application_identity<pkgapply::mutation_lease_instance_identity>(7),
      backend);
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
          pkgapply::application_journal_effect_kind::recover_active_object,
          pkgplan::package_path::parse("usr/bin/tool")),
  };
}

pkgapply::application_journal_record
record(pkgapply::application_journal_state state,
       std::vector<pkgapply::application_journal_event> events = {})
{
  return pkgapply::application_journal_record::make(
      header(), state, effects(), std::move(events));
}



} // namespace

int
main()
{
  using disposition = pkgapply::application_restart_disposition;
  using event_kind = pkgapply::application_journal_event_kind;
  using state = pkgapply::application_journal_state;

  const auto preparing = pkgapply::assess_application_restart(
      record(state::preparing));
  require(preparing.disposition() == disposition::resume_forward &&
              preparing.resumable(),
          "preparing journal was not forward resumable");

  const auto recovery_pending = pkgapply::assess_application_restart(
      record(state::recovery_pending));
  require(recovery_pending.disposition() == disposition::resume_recovery &&
              recovery_pending.resumable(),
          "recovery-pending journal was not recovery resumable");

  const auto unresolved_active = pkgapply::assess_application_restart(
      record(state::mutating,
             {{0, event_kind::intent, effects()[0].identity()}}));
  require(unresolved_active.disposition() == disposition::resume_recovery,
          "unresolved active intent was allowed to resume forward");

  const auto completed_prefix = pkgapply::assess_application_restart(
      record(state::mutating,
             {{0, event_kind::intent, effects()[0].identity()},
              {1, event_kind::completed, effects()[0].identity()}}));
  require(completed_prefix.disposition() == disposition::resume_forward,
          "durably completed active prefix was forced into recovery");

  const auto terminal_visible = pkgapply::assess_application_restart(
      pkgapply::application_journal_record::make(
          header(), state::effects_visible, effects(), {},
          application_identity<pkgapply::application_receipt_identity>(8)));
  require(terminal_visible.disposition() == disposition::terminal &&
              !terminal_visible.resumable(),
          "receipt-bearing visible journal was considered resumable");

  const auto abandoned = pkgapply::assess_application_restart(
      record(state::abandoned));
  require(abandoned.disposition() == disposition::terminal &&
              !abandoned.resumable(),
          "abandoned journal was considered resumable");

  const auto indeterminate = pkgapply::assess_application_restart(
      record(state::indeterminate));
  require(indeterminate.disposition() ==
              disposition::external_resolution_required &&
              !indeterminate.resumable(),
          "indeterminate journal was considered automatically resumable");

  require(preparing.journal() != recovery_pending.journal(),
          "restart assessment ignored journal state identity");



  return 0;
}
