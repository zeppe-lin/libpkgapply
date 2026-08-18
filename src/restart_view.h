// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <libpkgapply/restart.h>

#include "journal_history.h"

namespace pkgapply::detail {

class application_restart_view_builder final {
public:
  [[nodiscard]] static application_restart_view construct(
      application_journal_declaration_identity declaration,
      application_attempt attempt,
      backend_observation_batch admitted_observations,
      std::optional<backend_operation_result> incoming_payload,
      std::vector<application_restart_capture> captures,
      std::vector<application_restart_rejected_effect> rejected_effects,
      std::vector<application_restart_active_effect> active_effects,
      std::vector<application_restart_recovery_effect> recovery_effects,
      std::vector<application_restart_synchronization> synchronizations,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence,
      std::optional<completed_application_evidence> completed_evidence);

  [[nodiscard]] static application_restart_view build(
      const application_journal_history& history,
      const installation_application_request& request);
  [[nodiscard]] static application_restart_view build(
      const application_journal_history& history,
      const upgrade_application_request& request);
  [[nodiscard]] static application_restart_view build(
      const application_journal_history& history,
      const removal_application_request& request);
};

} // namespace pkgapply::detail
