// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file journal.h
 *  \brief Immutable snapshots of one write-ahead application journal.
 */
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

/*! \brief Schema version of application_journal_header. */
inline constexpr std::uint16_t application_journal_schema_version = 1;
/*! \brief Schema version of application_journal_effect. */
inline constexpr std::uint16_t application_journal_effect_schema_version = 1;
/*! \brief Schema version of application_journal_record. */
inline constexpr std::uint16_t application_journal_record_schema_version = 1;

/*! \brief Durable execution phase represented by one journal snapshot. */
enum class application_journal_state : std::uint8_t {
  preparing = 1, /*!< Resources and effect graph are being prepared. */
  prepared = 2, /*!< Preparation completed before target mutation. */
  mutating = 3, /*!< At least one target mutation may be in flight. */
  effects_visible = 4, /*!< Planned target effects are visible. */
  result_observed = 5, /*!< Resulting path universe was observed. */
  application_completed = 6, /*!< Successful completed evidence exists. */
  external_resolution_pending = 7, /*!< Caller must publish/finalize externally. */
  recovering = 8, /*!< Recovery effects are being applied. */
  recovered = 9, /*!< Recovery completed and failure receipt exists. */
  finalized = 10, /*!< External resolution and final durability completed. */
  abandoned = 11, /*!< Attempt was explicitly abandoned before completion. */
  indeterminate = 12, /*!< Truthful completion state cannot be established. */
  recovery_pending = 13, /*!< Recovery is required but not yet complete. */
};

/*! \brief Semantic mechanism step recorded by the journal. */
enum class application_journal_effect_kind : std::uint8_t {
  capture_old_object = 1, /*!< Capture one pre-mutation object. */
  stage_incoming_payload = 2, /*!< Stage one incoming regular payload. */
  publish_active_object = 3, /*!< Apply one active namespace consequence. */
  publish_rejected_object = 4, /*!< Publish one rejected-object consequence. */
  observe_result = 5, /*!< Observe one resulting path. */
  synchronize_journal = 6, /*!< Synchronize journal storage. */
  synchronize_active_namespace = 7, /*!< Synchronize active target storage. */
  synchronize_rejected_store = 8, /*!< Synchronize rejected-object storage. */
  seal_receipt = 9, /*!< Seal terminal application receipt authority. */
  synchronize_incoming_staging = 10, /*!< Synchronize incoming staging. */
  synchronize_recovery_staging = 11, /*!< Synchronize recovery staging. */
  synchronize_completed_evidence = 12, /*!< Synchronize completed evidence. */
  recover_active_object = 13, /*!< Restore one active target object. */
  synchronize_recovered_namespace = 14, /*!< Synchronize recovered target state. */
  publish_completed_evidence = 15, /*!< Publish completed-evidence bytes. */
};

/*! \brief Append-only event class for one journal effect. */
enum class application_journal_event_kind : std::uint8_t {
  intent = 1, /*!< Write-ahead intent before invoking the mechanism. */
  completed = 2, /*!< Mechanism completed successfully. */
  failed = 3, /*!< Mechanism failed determinately. */
  indeterminate = 4, /*!< Completion could not be established. */
};

/*! \brief Fixed identity-bearing header of one application journal. */
class application_journal_header final {
public:
  /*! \brief Validate, identify, and construct a journal header.
   *  \param kind Operation kind of the immutable request.
   *  \param request Application-request identity.
   *  \param plan Accepted operation-plan identity.
   *  \param attempt Physical application attempt.
   *  \param target Exact target-context identity.
   *  \param control Execution-control identity.
   *  \param state_projection Admitted lease-bound state projection.
   *  \param lease Physical mutation-lease acquisition identity.
   *  \param backend Exact mutation-backend identity.
   *  \return Immutable journal header.
   *  \throws std::invalid_argument If attempt request, target, or backend
   *          bindings differ from the supplied authorities.
   */
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

  /*!
   * \brief Return the journal-header schema version.
  *  \return The journal-header schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical journal identity.
  *  \return The canonical journal identity.
   */
  [[nodiscard]] const application_journal_identity& identity() const noexcept;
  /*!
   * \brief Return the immutable operation kind.
  *  \return The immutable operation kind.
   */
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  /*!
   * \brief Return the application-request identity.
  *  \return The application-request identity.
   */
  [[nodiscard]] const application_request_identity& request() const noexcept;
  /*!
   * \brief Return the accepted operation-plan identity.
  *  \return The accepted operation-plan identity.
   */
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  /*!
   * \brief Return the physical application attempt.
  *  \return The physical application attempt.
   */
  [[nodiscard]] const application_attempt& attempt() const noexcept;
  /*!
   * \brief Return the exact target-context identity.
  *  \return The exact target-context identity.
   */
  [[nodiscard]] const application_target_context_identity&
  target() const noexcept;
  /*!
   * \brief Return the execution-control identity.
  *  \return The execution-control identity.
   */
  [[nodiscard]] const application_execution_control_identity&
  control() const noexcept;
  /*!
   * \brief Return the admitted state-projection identity.
  *  \return The admitted state-projection identity.
   */
  [[nodiscard]] const lease_bound_state_projection_identity&
  state_projection() const noexcept;
  /*!
   * \brief Return the mutation-lease acquisition identity.
  *  \return The mutation-lease acquisition identity.
   */
  [[nodiscard]] const mutation_lease_instance_identity& lease() const noexcept;
  /*!
   * \brief Return the mutation-backend identity.
  *  \return The mutation-backend identity.
   */
  [[nodiscard]] const mutation_backend_identity& backend() const noexcept;

private:
  /*! \brief Construct validated authority already identified by make(). */
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

/*! \brief One deterministic node in the application effect graph. */
class application_journal_effect final {
public:
  /*! \brief Validate, identify, and construct one journal effect.
   *  \param ordinal Zero-based effect-graph position.
   *  \param kind Semantic mechanism step.
   *  \param path Path governed by path-scoped effects, otherwise empty.
   *  \return Immutable identified effect.
   *  \throws std::invalid_argument If path presence contradicts effect kind.
   */
  [[nodiscard]] static application_journal_effect make(
      std::uint64_t ordinal,
      application_journal_effect_kind kind,
      std::optional<pkgplan::package_path> path = std::nullopt);

  /*!
   * \brief Return the canonical effect identity.
  *  \return The canonical effect identity.
   */
  [[nodiscard]] const application_journal_effect_identity&
  identity() const noexcept;
  /*!
   * \brief Return the zero-based effect position.
  *  \return The zero-based effect position.
   */
  [[nodiscard]] std::uint64_t ordinal() const noexcept;
  /*!
   * \brief Return the semantic mechanism kind.
  *  \return The semantic mechanism kind.
   */
  [[nodiscard]] application_journal_effect_kind kind() const noexcept;
  /*!
   * \brief Return the governed path for path-scoped effects.
  *  \return The governed path for path-scoped effects.
   */
  [[nodiscard]] const std::optional<pkgplan::package_path>&
  path() const noexcept;

  /*!
   * \brief Order effects by ordinal and canonical identity.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend bool operator<(const application_journal_effect& lhs,
                        const application_journal_effect& rhs) noexcept;

private:
  /*! \brief Construct authority already identified by make(). */
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

/*! \brief One append-only write-ahead or terminal effect event. */
class application_journal_event final {
public:
  /*! \brief Validate and construct one journal event.
   *  \param sequence Zero-based append-only event sequence.
   *  \param kind Intent or terminal event kind.
   *  \param effect Identity of the governed effect.
   *  \param backend_evidence Backend evidence supporting the event.
   *  \throws std::invalid_argument If evidence identities are duplicated or
   *          the event kind is outside the represented protocol.
   */
  application_journal_event(
      std::uint64_t sequence,
      application_journal_event_kind kind,
      application_journal_effect_identity effect,
      std::vector<application_backend_evidence_identity> backend_evidence = {});

  /*!
   * \brief Return the zero-based append sequence.
  *  \return The zero-based append sequence.
   */
  [[nodiscard]] std::uint64_t sequence() const noexcept;
  /*!
   * \brief Return intent or terminal event kind.
  *  \return Intent or terminal event kind.
   */
  [[nodiscard]] application_journal_event_kind kind() const noexcept;
  /*!
   * \brief Return the governed effect identity.
  *  \return The governed effect identity.
   */
  [[nodiscard]] const application_journal_effect_identity&
  effect() const noexcept;
  /*!
   * \brief Return supporting backend evidence in canonical order.
  *  \return Supporting backend evidence in canonical order.
   */
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
  /*! \brief Validate, normalize, identify, and construct a journal snapshot.
   *  \param header Fixed application-attempt authority.
   *  \param state Durable execution phase.
   *  \param effects Complete deterministic effect graph.
   *  \param events Append-only effect-event history.
   *  \param receipt Optional terminal application receipt identity.
   *  \param completed_evidence Optional successful completed-evidence identity.
   *  \return Immutable journal snapshot.
   *  \throws std::invalid_argument If effect ordinals, event sequencing,
   *          intent/terminal transitions, or terminal evidence contradict the
   *          represented journal state.
   */
  [[nodiscard]] static application_journal_record make(
      application_journal_header header,
      application_journal_state state,
      std::vector<application_journal_effect> effects,
      std::vector<application_journal_event> events,
      std::optional<application_receipt_identity> receipt = std::nullopt,
      std::optional<completed_application_evidence_identity>
          completed_evidence = std::nullopt);

  /*!
   * \brief Return the journal-record schema version.
  *  \return The journal-record schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical journal-record identity.
  *  \return The canonical journal-record identity.
   */
  [[nodiscard]] const application_journal_record_identity&
  identity() const noexcept;
  /*!
   * \brief Return fixed application-attempt authority.
  *  \return Fixed application-attempt authority.
   */
  [[nodiscard]] const application_journal_header& header() const noexcept;
  /*!
   * \brief Return the durable execution phase.
  *  \return The durable execution phase.
   */
  [[nodiscard]] application_journal_state state() const noexcept;
  /*!
   * \brief Return the complete effect graph in ordinal order.
  *  \return The complete effect graph in ordinal order.
   */
  [[nodiscard]] const std::vector<application_journal_effect>&
  effects() const noexcept;
  /*!
   * \brief Return append-only events in sequence order.
  *  \return Append-only events in sequence order.
   */
  [[nodiscard]] const std::vector<application_journal_event>&
  events() const noexcept;
  /*!
   * \brief Return optional terminal receipt identity.
  *  \return Optional terminal receipt identity.
   */
  [[nodiscard]] const std::optional<application_receipt_identity>&
  receipt() const noexcept;
  /*!
   * \brief Return optional successful completed-evidence identity.
  *  \return Optional successful completed-evidence identity.
   */
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  completed_evidence() const noexcept;

private:
  /*! \brief Construct normalized authority already identified by make(). */
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
