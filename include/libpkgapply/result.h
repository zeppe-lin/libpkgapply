// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <libpkgapply/digest.h>
#include <libpkgapply/path_consequence.h>
#include <libpkgapply/request.h>
#include <libpkgapply/state_projection.h>
#include <libpkgplan/digest.h>

namespace pkgapply {

inline constexpr std::uint16_t completed_application_evidence_schema_version = 1;
inline constexpr std::uint16_t application_receipt_schema_version = 1;

/*! \brief Semantic outcome of one physical application attempt. */
enum class application_attempt_outcome : std::uint8_t {
  precondition_refused = 1,
  failed_before_target_mutation = 2,
  completed = 3,
  failed_fully_recovered = 4,
  failed_with_partial_effects = 5,
  effects_visible_durability_unconfirmed = 6,
  indeterminate = 7,
};

/*! \brief Recovery state established by one application attempt. */
enum class application_recovery_state : std::uint8_t {
  unchanged = 1,
  exact_prior_state_restored = 2,
  recovery_assets_retained = 3,
  known_residual_effects = 4,
  recovery_not_representable = 5,
  requires_authoritative_observation = 6,
};

/*! \brief Independently reported application durability domain. */
enum class application_durability_domain : std::uint8_t {
  journal = 1,
  incoming_staging = 2,
  recovery_staging = 3,
  active_namespace = 4,
  rejected_object_store = 5,
  completed_evidence = 6,
};

/*! \brief Persistence knowledge for one durability domain. */
enum class application_durability_status : std::uint8_t {
  not_attempted = 1,
  visible = 2,
  confirmed = 3,
  unconfirmed = 4,
  indeterminate = 5,
};

/*! \brief One domain-qualified durability result. */
class application_durability_fact final {
public:
  application_durability_fact(application_durability_domain domain,
                              application_durability_status status);
  [[nodiscard]] application_durability_domain domain() const noexcept;
  [[nodiscard]] application_durability_status status() const noexcept;

  friend bool operator==(const application_durability_fact& lhs,
                         const application_durability_fact& rhs) noexcept;
  friend bool operator!=(const application_durability_fact& lhs,
                         const application_durability_fact& rhs) noexcept;
  friend bool operator<(const application_durability_fact& lhs,
                        const application_durability_fact& rhs) noexcept;

private:
  application_durability_domain domain_;
  application_durability_status status_;
};

/*! \brief Complete six-domain durability profile in canonical order. */
class application_durability_profile final {
public:
  explicit application_durability_profile(
      std::vector<application_durability_fact> facts);
  [[nodiscard]] const std::vector<application_durability_fact>&
  facts() const noexcept;
  [[nodiscard]] application_durability_status
  status(application_durability_domain domain) const;

  friend bool operator==(const application_durability_profile& lhs,
                         const application_durability_profile& rhs) noexcept;
  friend bool operator!=(const application_durability_profile& lhs,
                         const application_durability_profile& rhs) noexcept;

private:
  std::vector<application_durability_fact> facts_;
};

/*! \brief Publication-eligible proof of a completely applied plan. */
class completed_application_evidence final {
public:
  [[nodiscard]] static completed_application_evidence
  installation(const installation_application_request& request,
               application_attempt_identity attempt,
               lease_bound_state_projection_identity state_projection,
               application_journal_identity journal,
               std::vector<application_path_consequence> paths,
               application_durability_profile durability,
               std::vector<application_backend_evidence_identity>
                   backend_evidence = {});

  [[nodiscard]] static completed_application_evidence
  upgrade(const upgrade_application_request& request,
          application_attempt_identity attempt,
          lease_bound_state_projection_identity state_projection,
          application_journal_identity journal,
          std::vector<application_path_consequence> paths,
          application_durability_profile durability,
          std::vector<application_backend_evidence_identity>
              backend_evidence = {});

  [[nodiscard]] static completed_application_evidence
  removal(const removal_application_request& request,
          application_attempt_identity attempt,
          lease_bound_state_projection_identity state_projection,
          application_journal_identity journal,
          std::vector<application_path_consequence> paths,
          application_durability_profile durability,
          std::vector<application_backend_evidence_identity>
              backend_evidence = {});

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const completed_application_evidence_identity&
  identity() const noexcept;
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] const application_request_identity& request() const noexcept;
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  [[nodiscard]] const application_attempt_identity& attempt() const noexcept;
  [[nodiscard]] const application_target_context_identity& target() const noexcept;
  [[nodiscard]] const application_execution_control_identity& control() const noexcept;
  [[nodiscard]] const lease_bound_state_projection_identity&
  state_projection() const noexcept;
  [[nodiscard]] const application_journal_identity& journal() const noexcept;
  [[nodiscard]] const std::vector<application_path_consequence>&
  paths() const noexcept;
  [[nodiscard]] const application_durability_profile& durability() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  [[nodiscard]] static completed_application_evidence make(
      pkgplan::operation_kind kind,
      application_request_identity request,
      pkgplan::operation_plan_identity plan,
      application_attempt_identity attempt,
      application_target_context_identity target,
      application_execution_control_identity control,
      lease_bound_state_projection_identity state_projection,
      application_journal_identity journal,
      std::vector<application_path_consequence> paths,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  completed_application_evidence(
      completed_application_evidence_identity identity,
      pkgplan::operation_kind kind,
      application_request_identity request,
      pkgplan::operation_plan_identity plan,
      application_attempt_identity attempt,
      application_target_context_identity target,
      application_execution_control_identity control,
      lease_bound_state_projection_identity state_projection,
      application_journal_identity journal,
      std::vector<application_path_consequence> paths,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  std::uint16_t schema_version_ = completed_application_evidence_schema_version;
  completed_application_evidence_identity identity_;
  pkgplan::operation_kind kind_;
  application_request_identity request_;
  pkgplan::operation_plan_identity plan_;
  application_attempt_identity attempt_;
  application_target_context_identity target_;
  application_execution_control_identity control_;
  lease_bound_state_projection_identity state_projection_;
  application_journal_identity journal_;
  std::vector<application_path_consequence> paths_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

/*! \brief Truthful result of one attempted package application. */
class application_receipt final {
public:
  [[nodiscard]] static application_receipt completed(
      completed_application_evidence evidence,
      application_recovery_state recovery,
      std::vector<application_backend_evidence_identity>
          backend_evidence = {});

  [[nodiscard]] static application_receipt failed(
      const installation_application_request& request,
      application_attempt_identity attempt,
      lease_bound_state_projection_identity state_projection,
      application_attempt_outcome outcome,
      application_recovery_state recovery,
      application_durability_profile durability,
      std::vector<application_path_consequence> paths,
      std::optional<application_journal_identity> journal,
      std::vector<application_backend_evidence_identity>
          backend_evidence = {});

  [[nodiscard]] static application_receipt failed(
      const upgrade_application_request& request,
      application_attempt_identity attempt,
      lease_bound_state_projection_identity state_projection,
      application_attempt_outcome outcome,
      application_recovery_state recovery,
      application_durability_profile durability,
      std::vector<application_path_consequence> paths,
      std::optional<application_journal_identity> journal,
      std::vector<application_backend_evidence_identity>
          backend_evidence = {});

  [[nodiscard]] static application_receipt failed(
      const removal_application_request& request,
      application_attempt_identity attempt,
      lease_bound_state_projection_identity state_projection,
      application_attempt_outcome outcome,
      application_recovery_state recovery,
      application_durability_profile durability,
      std::vector<application_path_consequence> paths,
      std::optional<application_journal_identity> journal,
      std::vector<application_backend_evidence_identity>
          backend_evidence = {});

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_receipt_identity& identity() const noexcept;
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] const application_request_identity& request() const noexcept;
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  [[nodiscard]] const application_attempt_identity& attempt() const noexcept;
  [[nodiscard]] const application_target_context_identity& target() const noexcept;
  [[nodiscard]] const application_execution_control_identity& control() const noexcept;
  [[nodiscard]] const lease_bound_state_projection_identity&
  state_projection() const noexcept;
  [[nodiscard]] application_attempt_outcome outcome() const noexcept;
  [[nodiscard]] application_recovery_state recovery() const noexcept;
  [[nodiscard]] const application_durability_profile& durability() const noexcept;
  [[nodiscard]] const std::vector<application_path_consequence>& paths() const noexcept;
  [[nodiscard]] const std::optional<application_journal_identity>&
  journal() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence>&
  completed_evidence() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  [[nodiscard]] static application_receipt make_failed(
      pkgplan::operation_kind kind,
      application_request_identity request,
      pkgplan::operation_plan_identity plan,
      application_attempt_identity attempt,
      application_target_context_identity target,
      application_execution_control_identity control,
      lease_bound_state_projection_identity state_projection,
      application_attempt_outcome outcome,
      application_recovery_state recovery,
      application_durability_profile durability,
      std::vector<application_path_consequence> paths,
      std::optional<application_journal_identity> journal,
      std::vector<application_backend_evidence_identity> backend_evidence);

  application_receipt(
      application_receipt_identity identity,
      pkgplan::operation_kind kind,
      application_request_identity request,
      pkgplan::operation_plan_identity plan,
      application_attempt_identity attempt,
      application_target_context_identity target,
      application_execution_control_identity control,
      lease_bound_state_projection_identity state_projection,
      application_attempt_outcome outcome,
      application_recovery_state recovery,
      application_durability_profile durability,
      std::vector<application_path_consequence> paths,
      std::optional<application_journal_identity> journal,
      std::optional<completed_application_evidence> completed_evidence,
      std::vector<application_backend_evidence_identity> backend_evidence);

  std::uint16_t schema_version_ = application_receipt_schema_version;
  application_receipt_identity identity_;
  pkgplan::operation_kind kind_;
  application_request_identity request_;
  pkgplan::operation_plan_identity plan_;
  application_attempt_identity attempt_;
  application_target_context_identity target_;
  application_execution_control_identity control_;
  lease_bound_state_projection_identity state_projection_;
  application_attempt_outcome outcome_;
  application_recovery_state recovery_;
  application_durability_profile durability_;
  std::vector<application_path_consequence> paths_;
  std::optional<application_journal_identity> journal_;
  std::optional<completed_application_evidence> completed_evidence_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

} // namespace pkgapply
