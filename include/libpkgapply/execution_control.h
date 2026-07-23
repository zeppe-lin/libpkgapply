// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>

#include <libpkgapply/digest.h>

namespace pkgapply {

inline constexpr std::uint16_t application_execution_control_schema_version = 1;

/*! \brief Required recovery guarantee for one application attempt. */
enum class application_recovery_requirement : std::uint8_t {
  none = 1,
  best_effort = 2,
  exact_prior_state = 3,
};

/*! \brief Required persistence boundary for successful application. */
enum class application_durability_requirement : std::uint8_t {
  visibility_only = 1,
  journal_and_recovery = 2,
  all_application_domains = 3,
};

/*! \brief Cancellation behavior after the target mutation boundary. */
enum class application_cancellation_policy : std::uint8_t {
  before_target_mutation_only = 1,
  recover_after_target_mutation = 2,
};

/*! \brief Immutable actuator-level guarantees, never path policy. */
class application_execution_control final {
public:
  [[nodiscard]] static application_execution_control
  make(application_recovery_requirement recovery,
       application_durability_requirement durability,
       application_cancellation_policy cancellation,
       std::optional<std::uint64_t> maximum_staging_bytes = std::nullopt,
       std::optional<std::uint64_t> maximum_recovery_bytes = std::nullopt);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_execution_control_identity&
  identity() const noexcept;

  [[nodiscard]] application_recovery_requirement recovery() const noexcept;
  [[nodiscard]] application_durability_requirement durability() const noexcept;
  [[nodiscard]] application_cancellation_policy cancellation() const noexcept;

  [[nodiscard]] const std::optional<std::uint64_t>&
  maximum_staging_bytes() const noexcept;

  [[nodiscard]] const std::optional<std::uint64_t>&
  maximum_recovery_bytes() const noexcept;

  friend bool operator==(const application_execution_control& lhs,
                         const application_execution_control& rhs) noexcept;
  friend bool operator!=(const application_execution_control& lhs,
                         const application_execution_control& rhs) noexcept;

private:
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
