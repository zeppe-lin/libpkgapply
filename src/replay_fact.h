// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <variant>

#include <libpkgapply/journal_transport.h>
#include <libpkgapply/restart.h>

namespace pkgapply::detail {

using application_replay_fact = std::variant<
    backend_operation_result,
    application_restart_capture,
    application_restart_rejected_effect,
    application_restart_active_effect,
    application_restart_recovery_effect,
    application_restart_synchronization,
    completed_application_evidence>;

[[nodiscard]] application_journal_replay_encoding encode_replay_seed(
    const backend_observation_batch& observations);
[[nodiscard]] backend_observation_batch decode_replay_seed(
    const application_journal_replay_encoding& encoding);

[[nodiscard]] application_journal_replay_encoding encode_replay_fact(
    const application_replay_fact& fact);

[[nodiscard]] application_replay_fact decode_replay_fact(
    const application_journal_replay_encoding& encoding,
    const installation_application_request& request);
[[nodiscard]] application_replay_fact decode_replay_fact(
    const application_journal_replay_encoding& encoding,
    const upgrade_application_request& request);
[[nodiscard]] application_replay_fact decode_replay_fact(
    const application_journal_replay_encoding& encoding,
    const removal_application_request& request);

} // namespace pkgapply::detail
