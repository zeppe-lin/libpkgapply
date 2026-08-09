// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file result.h
 *  \brief Completed application evidence and truthful terminal receipts.
 */
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

/*! \brief Schema version of completed_application_evidence. */
inline constexpr std::uint16_t completed_application_evidence_schema_version = 1;
/*! \brief Schema version of application_receipt. */
inline constexpr std::uint16_t application_receipt_schema_version = 1;

/*! \brief Semantic outcome of one physical application attempt. */
enum class application_attempt_outcome : std::uint8_t {
  precondition_refused = 1, /*!< Fresh preconditions refused all effects. */
  failed_before_target_mutation = 2, /*!< No managed active path was mutated. */
  completed = 3, /*!< Every planned consequence completed truthfully. */
  failed_fully_recovered = 4, /*!< Failure followed by exact restoration. */
  failed_with_partial_effects = 5, /*!< Known residual target effects remain. */
  effects_visible_durability_unconfirmed = 6, /*!< Effects visible, durability unresolved. */
  indeterminate = 7, /*!< Authoritative observation is required. */
};

/*! \brief Recovery state of the managed active-object namespace. */
enum class application_recovery_state : std::uint8_t {
  unchanged = 1, /*!< Target remained at the admitted prior state. */
  exact_prior_state_restored = 2, /*!< Recovery restored exact prior state. */
  recovery_assets_retained = 3, /*!< Success retained recovery material. */
  known_residual_effects = 4, /*!< Known target differences remain. */
  recovery_not_representable = 5, /*!< Exact recovery cannot be represented. */
  requires_authoritative_observation = 6, /*!< Target truth must be re-observed. */
};

/*! \brief Independently reported application durability domain. */
enum class application_durability_domain : std::uint8_t {
  journal = 1, /*!< Application journal storage. */
  incoming_staging = 2, /*!< Incoming payload staging storage. */
  recovery_staging = 3, /*!< Old-object recovery staging storage. */
  active_namespace = 4, /*!< Active target object namespace. */
  rejected_object_store = 5, /*!< Rejected-object evidence store. */
  completed_evidence = 6, /*!< Completed-evidence storage. */
};

/*! \brief Persistence knowledge for one durability domain. */
enum class application_durability_status : std::uint8_t {
  not_attempted = 1, /*!< Domain was outside the attempted durability set. */
  visible = 2, /*!< Effects are visible but persistence was not required. */
  confirmed = 3, /*!< Required persistence was confirmed. */
  unconfirmed = 4, /*!< Persistence was attempted but not confirmed. */
  indeterminate = 5, /*!< Durability truth cannot be established. */
};

/*! \brief One domain-qualified durability result. */
class application_durability_fact final {
public:
  /*! \brief Validate and construct one durability fact.
   *  \param domain Exact application-owned durability domain.
   *  \param status Persistence knowledge for that domain.
   *  \throws std::invalid_argument If either enum value is outside the schema.
   */
  application_durability_fact(application_durability_domain domain,
                              application_durability_status status);

  /*!
   * \brief Return the qualified durability domain.
  *  \return The qualified durability domain.
   */
  [[nodiscard]] application_durability_domain domain() const noexcept;
  /*!
   * \brief Return persistence knowledge for that domain.
  *  \return Persistence knowledge for that domain.
   */
  [[nodiscard]] application_durability_status status() const noexcept;

  /*!
   * \brief Compare durability facts for equality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs are equal.
   */
  friend bool operator==(const application_durability_fact& lhs,
                         const application_durability_fact& rhs) noexcept;
  /*!
   * \brief Compare durability facts for inequality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs differ.
   */
  friend bool operator!=(const application_durability_fact& lhs,
                         const application_durability_fact& rhs) noexcept;
  /*!
   * \brief Order durability facts by domain and then status.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend bool operator<(const application_durability_fact& lhs,
                        const application_durability_fact& rhs) noexcept;

private:
  application_durability_domain domain_;
  application_durability_status status_;
};

/*! \brief Complete six-domain durability profile in canonical order. */
class application_durability_profile final {
public:
  /*! \brief Normalize and construct a complete durability profile.
   *  \param facts Exactly one fact for each application durability domain.
   *  \throws std::invalid_argument If a domain is missing or duplicated.
   */
  explicit application_durability_profile(
      std::vector<application_durability_fact> facts);

  /*!
   * \brief Return all six facts in canonical domain order.
  *  \return All six facts in canonical domain order.
   */
  [[nodiscard]] const std::vector<application_durability_fact>&
  facts() const noexcept;

  /*! \brief Return persistence knowledge for one exact domain.
   *  \param domain Durability domain to query.
   *  \return Status retained for that domain.
   *  \throws std::invalid_argument If `domain` is outside the schema.
   */
  [[nodiscard]] application_durability_status
  status(application_durability_domain domain) const;

  /*!
   * \brief Compare durability profiles for equality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs are equal.
   */
  friend bool operator==(const application_durability_profile& lhs,
                         const application_durability_profile& rhs) noexcept;
  /*!
   * \brief Compare durability profiles for inequality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs differ.
   */
  friend bool operator!=(const application_durability_profile& lhs,
                         const application_durability_profile& rhs) noexcept;

private:
  std::vector<application_durability_fact> facts_;
};

/*! \brief Publication-eligible proof of a completely applied plan.
 *
 *  Completed evidence binds every path consequence and required durability
 *  fact to one immutable request, physical attempt, admitted state projection,
 *  and durable journal. After restart, this is the current projection validated
 *  under the newly acquired lease; the journal header continues to retain the
 *  original process's admission projection. It is evidence for a
 *  state-publication adapter; it does not publish installed state itself.
 */
class completed_application_evidence final {
public:
  /*! \brief Construct completed evidence for an installation request.
   *  \param request Exact immutable installation request.
   *  \param attempt Physical attempt identity.
   *  \param state_projection Admitted state-projection identity.
   *  \param journal Durable journal identity.
   *  \param paths Complete application path consequences.
   *  \param durability Complete six-domain durability profile.
   *  \param backend_evidence Supporting backend evidence identities.
   *  \return Canonical publication-eligible completed evidence.
   *  \throws std::invalid_argument If path universe, planned consequences,
   *          completion, publication eligibility, durability, or evidence
   *          uniqueness contradict the request.
   */
  [[nodiscard]] static completed_application_evidence
  installation(const installation_application_request& request,
               application_attempt_identity attempt,
               lease_bound_state_projection_identity state_projection,
               application_journal_identity journal,
               std::vector<application_path_consequence> paths,
               application_durability_profile durability,
               std::vector<application_backend_evidence_identity>
                   backend_evidence = {});

  /*! \brief Construct completed evidence for an upgrade request.
   *  \param request Exact immutable upgrade request.
   *  \param attempt Physical attempt identity.
   *  \param state_projection Admitted state-projection identity.
   *  \param journal Durable journal identity.
   *  \param paths Complete application path consequences.
   *  \param durability Complete six-domain durability profile.
   *  \param backend_evidence Supporting backend evidence identities.
   *  \return Canonical publication-eligible completed evidence.
   *  \throws std::invalid_argument If path universe, planned consequences,
   *          completion, publication eligibility, durability, or evidence
   *          uniqueness contradict the request.
   */
  [[nodiscard]] static completed_application_evidence
  upgrade(const upgrade_application_request& request,
          application_attempt_identity attempt,
          lease_bound_state_projection_identity state_projection,
          application_journal_identity journal,
          std::vector<application_path_consequence> paths,
          application_durability_profile durability,
          std::vector<application_backend_evidence_identity>
              backend_evidence = {});

  /*! \brief Construct completed evidence for a removal request.
   *  \param request Exact immutable removal request.
   *  \param attempt Physical attempt identity.
   *  \param state_projection Admitted state-projection identity.
   *  \param journal Durable journal identity.
   *  \param paths Complete application path consequences.
   *  \param durability Complete six-domain durability profile.
   *  \param backend_evidence Supporting backend evidence identities.
   *  \return Canonical publication-eligible completed evidence.
   *  \throws std::invalid_argument If path universe, planned consequences,
   *          completion, publication eligibility, durability, or evidence
   *          uniqueness contradict the request.
   */
  [[nodiscard]] static completed_application_evidence
  removal(const removal_application_request& request,
          application_attempt_identity attempt,
          lease_bound_state_projection_identity state_projection,
          application_journal_identity journal,
          std::vector<application_path_consequence> paths,
          application_durability_profile durability,
          std::vector<application_backend_evidence_identity>
              backend_evidence = {});

  /*!
   * \brief Return the completed-evidence schema version.
  *  \return The completed-evidence schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical completed-evidence identity.
  *  \return The canonical completed-evidence identity.
   */
  [[nodiscard]] const completed_application_evidence_identity&
  identity() const noexcept;
  /*!
   * \brief Return the completed operation kind.
  *  \return The completed operation kind.
   */
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  /*!
   * \brief Return the immutable application-request identity.
  *  \return The immutable application-request identity.
   */
  [[nodiscard]] const application_request_identity& request() const noexcept;
  /*!
   * \brief Return the accepted operation-plan identity.
  *  \return The accepted operation-plan identity.
   */
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  /*!
   * \brief Return the physical application-attempt identity.
  *  \return The physical application-attempt identity.
   */
  [[nodiscard]] const application_attempt_identity& attempt() const noexcept;
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
   * \brief Return the durable journal identity.
  *  \return The durable journal identity.
   */
  [[nodiscard]] const application_journal_identity& journal() const noexcept;
  /*!
   * \brief Return complete consequences in canonical path order.
  *  \return Complete consequences in canonical path order.
   */
  [[nodiscard]] const std::vector<application_path_consequence>&
  paths() const noexcept;
  /*!
   * \brief Return the complete durability profile.
  *  \return The complete durability profile.
   */
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  /*!
   * \brief Return supporting backend evidence in canonical order.
  *  \return Supporting backend evidence in canonical order.
   */
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

/*! \brief Truthful terminal result of one attempted package application. */
class application_receipt final {
public:
  /*! \brief Construct a successful terminal receipt.
   *  \param evidence Publication-eligible completed application evidence.
   *  \param recovery Unchanged or retained-recovery-assets state.
   *  \param backend_evidence Additional terminal backend evidence identities.
   *  \return Canonical completed application receipt.
   *  \throws std::invalid_argument If recovery state or evidence uniqueness is
   *          inconsistent with completed application.
   */
  [[nodiscard]] static application_receipt completed(
      completed_application_evidence evidence,
      application_recovery_state recovery,
      std::vector<application_backend_evidence_identity>
          backend_evidence = {});

  /*! \brief Construct a non-completed installation receipt.
   *  \param request Exact immutable installation request.
   *  \param attempt Physical attempt identity.
   *  \param state_projection Admitted state-projection identity.
   *  \param outcome Truthful non-completed attempt outcome.
   *  \param recovery Truthful active-namespace recovery state.
   *  \param durability Complete six-domain durability profile.
   *  \param paths Known path consequences, if any.
   *  \param journal Journal identity when a post-mutation attempt exists.
   *  \param backend_evidence Supporting backend evidence identities.
   *  \return Canonical terminal failure/refusal receipt.
   *  \throws std::invalid_argument If outcome, recovery, journal, paths,
   *          publication eligibility, durability, or evidence contradict.
   */
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

  /*! \brief Construct a non-completed upgrade receipt.
   *  \param request Exact immutable upgrade request.
   *  \param attempt Physical attempt identity.
   *  \param state_projection Admitted state-projection identity.
   *  \param outcome Truthful non-completed attempt outcome.
   *  \param recovery Truthful active-namespace recovery state.
   *  \param durability Complete six-domain durability profile.
   *  \param paths Known path consequences, if any.
   *  \param journal Journal identity when a post-mutation attempt exists.
   *  \param backend_evidence Supporting backend evidence identities.
   *  \return Canonical terminal failure/refusal receipt.
   *  \throws std::invalid_argument If outcome, recovery, journal, paths,
   *          publication eligibility, durability, or evidence contradict.
   */
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

  /*! \brief Construct a non-completed removal receipt.
   *  \param request Exact immutable removal request.
   *  \param attempt Physical attempt identity.
   *  \param state_projection Admitted state-projection identity.
   *  \param outcome Truthful non-completed attempt outcome.
   *  \param recovery Truthful active-namespace recovery state.
   *  \param durability Complete six-domain durability profile.
   *  \param paths Known path consequences, if any.
   *  \param journal Journal identity when a post-mutation attempt exists.
   *  \param backend_evidence Supporting backend evidence identities.
   *  \return Canonical terminal failure/refusal receipt.
   *  \throws std::invalid_argument If outcome, recovery, journal, paths,
   *          publication eligibility, durability, or evidence contradict.
   */
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

  /*!
   * \brief Return the application-receipt schema version.
  *  \return The application-receipt schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical receipt identity.
  *  \return The canonical receipt identity.
   */
  [[nodiscard]] const application_receipt_identity& identity() const noexcept;
  /*!
   * \brief Return the attempted operation kind.
  *  \return The attempted operation kind.
   */
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  /*!
   * \brief Return the immutable application-request identity.
  *  \return The immutable application-request identity.
   */
  [[nodiscard]] const application_request_identity& request() const noexcept;
  /*!
   * \brief Return the accepted operation-plan identity.
  *  \return The accepted operation-plan identity.
   */
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  /*!
   * \brief Return the physical application-attempt identity.
  *  \return The physical application-attempt identity.
   */
  [[nodiscard]] const application_attempt_identity& attempt() const noexcept;
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
   * \brief Return the truthful attempt outcome.
  *  \return The truthful attempt outcome.
   */
  [[nodiscard]] application_attempt_outcome outcome() const noexcept;
  /*!
   * \brief Return the truthful active-namespace recovery state.
  *  \return The truthful active-namespace recovery state.
   */
  [[nodiscard]] application_recovery_state recovery() const noexcept;
  /*!
   * \brief Return the complete durability profile.
  *  \return The complete durability profile.
   */
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  /*!
   * \brief Return known path consequences in canonical order.
  *  \return Known path consequences in canonical order.
   */
  [[nodiscard]] const std::vector<application_path_consequence>&
  paths() const noexcept;
  /*!
   * \brief Return journal identity when a durable attempt exists.
  *  \return Journal identity when a durable attempt exists.
   */
  [[nodiscard]] const std::optional<application_journal_identity>&
  journal() const noexcept;
  /*!
   * \brief Return completed evidence only for a successful receipt.
  *  \return Completed evidence only for a successful receipt.
   */
  [[nodiscard]] const std::optional<completed_application_evidence>&
  completed_evidence() const noexcept;
  /*!
   * \brief Return supporting backend evidence in canonical order.
  *  \return Supporting backend evidence in canonical order.
   */
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
