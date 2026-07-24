// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <libpkgapply/backend.h>
#include <libpkgapply/journal.h>
#include <libpkgapply/request.h>
#include <libpkgapply/state_projection.h>
#include <libpkgimage/package_archive.h>

namespace pkgapply {

/*! \brief Required controller action for one validated durable journal. */
enum class application_restart_disposition : std::uint8_t {
  resume_forward = 1,
  resume_recovery = 2,
  terminal = 3,
  external_resolution_required = 4,
};

/*! \brief Why a durable attempt cannot be reopened automatically. */
enum class application_restart_error_code : std::uint8_t {
  journal_not_resumable = 1,
  journal_operation_kind_mismatch = 2,
  journal_request_mismatch = 3,
  journal_plan_mismatch = 4,
  journal_target_mismatch = 5,
  journal_control_mismatch = 6,
  journal_backend_mismatch = 7,
  transaction_attempt_nonce_mismatch = 8,
  transaction_journal_mismatch = 9,
};

/*! \brief Invalid restart authority or backend reopen binding. */
class application_restart_error final : public std::invalid_argument {
public:
  application_restart_error(application_restart_error_code code,
                            std::string message);

  [[nodiscard]] application_restart_error_code code() const noexcept;

private:
  application_restart_error_code code_;
};

/*! \brief Pure assessment of one durable application journal snapshot. */
class application_restart_assessment final {
public:
  application_restart_assessment(
      application_journal_record_identity journal,
      application_journal_state state,
      application_restart_disposition disposition);

  [[nodiscard]] const application_journal_record_identity&
  journal() const noexcept;
  [[nodiscard]] application_journal_state state() const noexcept;
  [[nodiscard]] application_restart_disposition disposition() const noexcept;
  [[nodiscard]] bool resumable() const noexcept;

private:
  application_journal_record_identity journal_;
  application_journal_state state_;
  application_restart_disposition disposition_;
};

/*! \brief Classify restart handling without touching a backend or target. */
[[nodiscard]] application_restart_assessment
assess_application_restart(const application_journal_record& journal);

void validate_application_restart(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive);

void validate_application_restart(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive);

void validate_application_restart(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal);

void validate_restarted_backend_transaction(
    const application_target_context& target,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const application_backend_transaction& transaction);

} // namespace pkgapply
