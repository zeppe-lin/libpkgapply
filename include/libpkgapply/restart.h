// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

/*! \brief Durable capture fact retained by one reopened backend attempt. */
class application_restart_capture final {
public:
  explicit application_restart_capture(old_object_capture_result result);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] const old_object_capture_result& result() const noexcept;

  friend bool operator<(const application_restart_capture& lhs,
                        const application_restart_capture& rhs) noexcept;

private:
  old_object_capture_result result_;
};

/*! \brief Durable rejected-object result retained across process restart. */
class application_restart_rejected_effect final {
public:
  application_restart_rejected_effect(
      pkgplan::package_path path,
      rejected_object_publication_result result);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] const rejected_object_publication_result&
  result() const noexcept;

  friend bool operator<(const application_restart_rejected_effect& lhs,
                        const application_restart_rejected_effect& rhs) noexcept;

private:
  pkgplan::package_path path_;
  rejected_object_publication_result result_;
};

/*! \brief Durable active-effect result retained across process restart. */
class application_restart_active_effect final {
public:
  application_restart_active_effect(
      pkgplan::package_path path,
      backend_operation_result result);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] const backend_operation_result& result() const noexcept;

  friend bool operator<(const application_restart_active_effect& lhs,
                        const application_restart_active_effect& rhs) noexcept;

private:
  pkgplan::package_path path_;
  backend_operation_result result_;
};

/*! \brief Exact backend-owned material required to replay one durable attempt. */
class application_restart_checkpoint final {
public:
  [[nodiscard]] static application_restart_checkpoint make(
      application_journal_record_identity journal,
      backend_observation_batch admitted_observations,
      std::optional<backend_operation_result> incoming_payload,
      std::vector<application_restart_capture> captures,
      std::vector<application_restart_rejected_effect> rejected_effects,
      std::vector<application_restart_active_effect> active_effects,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence = {},
      std::optional<completed_application_evidence> completed_evidence =
          std::nullopt);

  [[nodiscard]] const application_journal_record_identity&
  journal() const noexcept;
  [[nodiscard]] const backend_observation_batch&
  admitted_observations() const noexcept;
  [[nodiscard]] const std::optional<backend_operation_result>&
  incoming_payload() const noexcept;
  [[nodiscard]] const std::vector<application_restart_capture>&
  captures() const noexcept;
  [[nodiscard]] const std::vector<application_restart_rejected_effect>&
  rejected_effects() const noexcept;
  [[nodiscard]] const std::vector<application_restart_active_effect>&
  active_effects() const noexcept;
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence>&
  completed_evidence() const noexcept;

  [[nodiscard]] const application_restart_capture*
  find_capture(const pkgplan::package_path& path) const noexcept;
  [[nodiscard]] const application_restart_rejected_effect*
  find_rejected_effect(const pkgplan::package_path& path) const noexcept;
  [[nodiscard]] const application_restart_active_effect*
  find_active_effect(const pkgplan::package_path& path) const noexcept;

private:
  application_restart_checkpoint(
      application_journal_record_identity journal,
      backend_observation_batch admitted_observations,
      std::optional<backend_operation_result> incoming_payload,
      std::vector<application_restart_capture> captures,
      std::vector<application_restart_rejected_effect> rejected_effects,
      std::vector<application_restart_active_effect> active_effects,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence,
      std::optional<completed_application_evidence> completed_evidence);

  application_journal_record_identity journal_;
  backend_observation_batch admitted_observations_;
  std::optional<backend_operation_result> incoming_payload_;
  std::vector<application_restart_capture> captures_;
  std::vector<application_restart_rejected_effect> rejected_effects_;
  std::vector<application_restart_active_effect> active_effects_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
  std::optional<completed_application_evidence> completed_evidence_;
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
