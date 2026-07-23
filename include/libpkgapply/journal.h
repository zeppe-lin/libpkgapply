// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <libpkgapply/attempt.h>
#include <libpkgapply/digest.h>
#include <libpkgplan/digest.h>
#include <libpkgplan/package_path.h>
#include <libpkgplan/plan.h>

namespace pkgapply {

inline constexpr std::uint16_t application_journal_schema_version = 1;
inline constexpr std::uint16_t application_journal_effect_schema_version = 1;
inline constexpr std::uint16_t application_journal_record_schema_version = 1;

/*! \brief Durable execution phase represented by one journal snapshot. */
enum class application_journal_state : std::uint8_t {
  preparing = 1,
  prepared = 2,
  mutating = 3,
  effects_visible = 4,
  result_observed = 5,
  application_completed = 6,
  external_resolution_pending = 7,
  recovering = 8,
  recovered = 9,
  finalized = 10,
  abandoned = 11,
  indeterminate = 12,
  recovery_pending = 13,
};

/*! \brief Semantic mechanism step recorded by the application journal. */
enum class application_journal_effect_kind : std::uint8_t {
  capture_old_object = 1,
  stage_incoming_payload = 2,
  publish_active_object = 3,
  publish_rejected_object = 4,
  observe_result = 5,
  synchronize_journal = 6,
  synchronize_active_namespace = 7,
  synchronize_rejected_store = 8,
  seal_receipt = 9,
  synchronize_incoming_staging = 10,
  synchronize_recovery_staging = 11,
  synchronize_completed_evidence = 12,
};

/*! \brief Append-only event class for one journal effect. */
enum class application_journal_event_kind : std::uint8_t {
  intent = 1,
  completed = 2,
  failed = 3,
  indeterminate = 4,
};

/*! \brief Fixed identity-bearing header of one durable application journal. */
class application_journal_header final {
public:
  [[nodiscard]] static application_journal_header make(
      pkgplan::operation_kind kind,
      application_request_identity request,
      pkgplan::operation_plan_identity plan,
      application_attempt attempt,
      application_target_context_identity target,
      application_execution_control_identity control,
      lease_bound_state_projection_identity state_projection,
      mutation_lease_instance_identity lease,
      mutation_backend_identity backend);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_journal_identity& identity() const noexcept;
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] const application_request_identity& request() const noexcept;
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  [[nodiscard]] const application_attempt& attempt() const noexcept;
  [[nodiscard]] const application_target_context_identity& target() const noexcept;
  [[nodiscard]] const application_execution_control_identity& control() const noexcept;
  [[nodiscard]] const lease_bound_state_projection_identity&
  state_projection() const noexcept;
  [[nodiscard]] const mutation_lease_instance_identity& lease() const noexcept;
  [[nodiscard]] const mutation_backend_identity& backend() const noexcept;

private:
  application_journal_header(
      application_journal_identity identity,
      pkgplan::operation_kind kind,
      application_request_identity request,
      pkgplan::operation_plan_identity plan,
      application_attempt attempt,
      application_target_context_identity target,
      application_execution_control_identity control,
      lease_bound_state_projection_identity state_projection,
      mutation_lease_instance_identity lease,
      mutation_backend_identity backend);

  std::uint16_t schema_version_ = application_journal_schema_version;
  application_journal_identity identity_;
  pkgplan::operation_kind kind_;
  application_request_identity request_;
  pkgplan::operation_plan_identity plan_;
  application_attempt attempt_;
  application_target_context_identity target_;
  application_execution_control_identity control_;
  lease_bound_state_projection_identity state_projection_;
  mutation_lease_instance_identity lease_;
  mutation_backend_identity backend_;
};

/*! \brief One deterministic step in the application effect graph. */
class application_journal_effect final {
public:
  [[nodiscard]] static application_journal_effect make(
      std::uint64_t ordinal,
      application_journal_effect_kind kind,
      std::optional<pkgplan::package_path> path = std::nullopt);

  [[nodiscard]] const application_journal_effect_identity&
  identity() const noexcept;
  [[nodiscard]] std::uint64_t ordinal() const noexcept;
  [[nodiscard]] application_journal_effect_kind kind() const noexcept;
  [[nodiscard]] const std::optional<pkgplan::package_path>& path() const noexcept;

  friend bool operator<(const application_journal_effect& lhs,
                        const application_journal_effect& rhs) noexcept;

private:
  application_journal_effect(
      application_journal_effect_identity identity,
      std::uint64_t ordinal,
      application_journal_effect_kind kind,
      std::optional<pkgplan::package_path> path);

  application_journal_effect_identity identity_;
  std::uint64_t ordinal_;
  application_journal_effect_kind kind_;
  std::optional<pkgplan::package_path> path_;
};

/*! \brief One append-only write-ahead or terminal event. */
class application_journal_event final {
public:
  application_journal_event(
      std::uint64_t sequence,
      application_journal_event_kind kind,
      application_journal_effect_identity effect,
      std::vector<application_backend_evidence_identity> backend_evidence = {});

  [[nodiscard]] std::uint64_t sequence() const noexcept;
  [[nodiscard]] application_journal_event_kind kind() const noexcept;
  [[nodiscard]] const application_journal_effect_identity& effect() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  std::uint64_t sequence_;
  application_journal_event_kind kind_;
  application_journal_effect_identity effect_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

/*! \brief Immutable identified snapshot of one durable application journal. */
class application_journal_record final {
public:
  [[nodiscard]] static application_journal_record make(
      application_journal_header header,
      application_journal_state state,
      std::vector<application_journal_effect> effects,
      std::vector<application_journal_event> events,
      std::optional<application_receipt_identity> receipt = std::nullopt,
      std::optional<completed_application_evidence_identity>
          completed_evidence = std::nullopt);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_journal_record_identity&
  identity() const noexcept;
  [[nodiscard]] const application_journal_header& header() const noexcept;
  [[nodiscard]] application_journal_state state() const noexcept;
  [[nodiscard]] const std::vector<application_journal_effect>&
  effects() const noexcept;
  [[nodiscard]] const std::vector<application_journal_event>&
  events() const noexcept;
  [[nodiscard]] const std::optional<application_receipt_identity>&
  receipt() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  completed_evidence() const noexcept;

private:
  application_journal_record(
      application_journal_record_identity identity,
      application_journal_header header,
      application_journal_state state,
      std::vector<application_journal_effect> effects,
      std::vector<application_journal_event> events,
      std::optional<application_receipt_identity> receipt,
      std::optional<completed_application_evidence_identity>
          completed_evidence);

  std::uint16_t schema_version_ = application_journal_record_schema_version;
  application_journal_record_identity identity_;
  application_journal_header header_;
  application_journal_state state_;
  std::vector<application_journal_effect> effects_;
  std::vector<application_journal_event> events_;
  std::optional<application_receipt_identity> receipt_;
  std::optional<completed_application_evidence_identity> completed_evidence_;
};

} // namespace pkgapply
