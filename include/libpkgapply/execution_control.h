// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file execution_control.h
 *  \brief Actuator-level recovery, durability, and cancellation requirements.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstdint>
#include <optional>

#include <libpkgapply/digest.h>

namespace pkgapply {

/*! \brief Schema version of application_execution_control. */
inline constexpr std::uint16_t application_execution_control_schema_version = 1;

/*! \brief Recovery guarantee required for one application attempt. */
enum class application_recovery_requirement : std::uint8_t {
  none = 1, /*!< No old-object recovery authority is required. */
  best_effort = 2, /*!< Retain recovery material where the backend can do so. */
  exact_prior_state = 3, /*!< Require exact restoration of the admitted state. */
};

/*! \brief Persistence boundary required before reporting success. */
enum class application_durability_requirement : std::uint8_t {
  visibility_only = 1, /*!< Require only active-target visibility. */
  journal_and_recovery = 2, /*!< Persist journal and recovery authorities. */
  all_application_domains = 3, /*!< Synchronize every application-owned domain. */
};

/*! \brief Cancellation behavior after the target-mutation boundary. */
enum class application_cancellation_policy : std::uint8_t {
  before_target_mutation_only = 1, /*!< Cancellation stops before first mutation. */
  recover_after_target_mutation = 2, /*!< Cancellation triggers recovery afterward. */
};

/*! \brief Immutable actuator guarantees, never path-selection policy. */
class PKGAPPLY_API application_execution_control final {
public:
  /*! \brief Validate, identify, and construct execution control.
   *  \param recovery Required recovery guarantee.
   *  \param durability Required persistence boundary.
   *  \param cancellation Cancellation behavior at the mutation boundary.
   *  \param maximum_staging_bytes Optional nonzero staged-payload ceiling.
   *  \param maximum_recovery_bytes Optional nonzero recovery-data ceiling.
   *  \return Immutable normalized execution control.
   *  \throws std::invalid_argument For zero ceilings, unknown enum values, or
   *          exact recovery combined with pre-mutation-only cancellation.
   */
  [[nodiscard]] static application_execution_control
  make(application_recovery_requirement recovery,
       application_durability_requirement durability,
       application_cancellation_policy cancellation,
       std::optional<std::uint64_t> maximum_staging_bytes = std::nullopt,
       std::optional<std::uint64_t> maximum_recovery_bytes = std::nullopt);

  /*!
   * \brief Return the execution-control schema version.
  *  \return The execution-control schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  /*!
   * \brief Return the canonical execution-control identity.
  *  \return The canonical execution-control identity.
   */
  [[nodiscard]] const application_execution_control_identity&
  identity() const noexcept;

  /*!
   * \brief Return the required recovery guarantee.
  *  \return The required recovery guarantee.
   */
  [[nodiscard]] application_recovery_requirement recovery() const noexcept;

  /*!
   * \brief Return the required durability boundary.
  *  \return The required durability boundary.
   */
  [[nodiscard]] application_durability_requirement durability() const noexcept;

  /*!
   * \brief Return the cancellation policy.
  *  \return The cancellation policy.
   */
  [[nodiscard]] application_cancellation_policy cancellation() const noexcept;

  /*!
   * \brief Return the optional staged-payload byte ceiling.
  *  \return The optional staged-payload byte ceiling.
   */
  [[nodiscard]] const std::optional<std::uint64_t>&
  maximum_staging_bytes() const noexcept;

  /*!
   * \brief Return the optional recovery-data byte ceiling.
  *  \return The optional recovery-data byte ceiling.
   */
  [[nodiscard]] const std::optional<std::uint64_t>&
  maximum_recovery_bytes() const noexcept;

  /*!
   * \brief Compare complete execution controls for equality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs are equal.
   */
  friend PKGAPPLY_API bool operator==(const application_execution_control& lhs,
                         const application_execution_control& rhs) noexcept;

  /*!
   * \brief Compare complete execution controls for inequality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs differ.
   */
  friend PKGAPPLY_API bool operator!=(const application_execution_control& lhs,
                         const application_execution_control& rhs) noexcept;

private:
  /*! \brief Construct validated control already identified by make(). */
  application_execution_control(
      application_execution_control_identity identity,
      application_recovery_requirement recovery,
      application_durability_requirement durability,
      application_cancellation_policy cancellation,
      std::optional<std::uint64_t> maximum_staging_bytes,
      std::optional<std::uint64_t> maximum_recovery_bytes);

  std::uint16_t schema_version_ = application_execution_control_schema_version;
  application_execution_control_identity identity_;
  application_recovery_requirement recovery_;
  application_durability_requirement durability_;
  application_cancellation_policy cancellation_;
  std::optional<std::uint64_t> maximum_staging_bytes_;
  std::optional<std::uint64_t> maximum_recovery_bytes_;
};

} // namespace pkgapply
