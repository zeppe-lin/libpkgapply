// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>

#include <libpkgapply/digest.h>
#include <libpkgplan/digest.h>

namespace pkgapply {

inline constexpr std::uint16_t application_target_context_schema_version = 1;

/*! \brief Immutable actuator projection of one managed target context. */
class application_target_context final {
public:
  /*! \brief Validate, identify, and construct one target application context. */
  [[nodiscard]] static application_target_context
  make(pkgplan::target_system_context_identity target,
       managed_target_identity managed_target,
       root_view_identity root_view,
       observation_backend_identity observation_backend,
       mutation_backend_identity mutation_backend,
       mutation_exclusion_domain_identity mutation_exclusion_domain,
       active_object_namespace_identity active_namespace,
       rejected_object_store_identity rejected_store,
       staging_namespace_identity staging_namespace,
       journal_namespace_identity journal_namespace,
       execution_capability_profile_identity capabilities,
       std::optional<lifecycle_executor_identity> lifecycle_executor =
           std::nullopt);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  [[nodiscard]] const application_target_context_identity&
  identity() const noexcept;

  [[nodiscard]] const pkgplan::target_system_context_identity&
  target() const noexcept;

  [[nodiscard]] const managed_target_identity& managed_target() const noexcept;
  [[nodiscard]] const root_view_identity& root_view() const noexcept;

  [[nodiscard]] const observation_backend_identity&
  observation_backend() const noexcept;

  [[nodiscard]] const mutation_backend_identity&
  mutation_backend() const noexcept;

  [[nodiscard]] const mutation_exclusion_domain_identity&
  mutation_exclusion_domain() const noexcept;

  [[nodiscard]] const active_object_namespace_identity&
  active_namespace() const noexcept;

  [[nodiscard]] const rejected_object_store_identity&
  rejected_store() const noexcept;

  [[nodiscard]] const staging_namespace_identity&
  staging_namespace() const noexcept;

  [[nodiscard]] const journal_namespace_identity&
  journal_namespace() const noexcept;

  [[nodiscard]] const execution_capability_profile_identity&
  capabilities() const noexcept;

  [[nodiscard]] const std::optional<lifecycle_executor_identity>&
  lifecycle_executor() const noexcept;

  friend bool operator==(const application_target_context& lhs,
                         const application_target_context& rhs) noexcept;
  friend bool operator!=(const application_target_context& lhs,
                         const application_target_context& rhs) noexcept;

private:
  application_target_context(
      application_target_context_identity identity,
      pkgplan::target_system_context_identity target,
      managed_target_identity managed_target,
      root_view_identity root_view,
      observation_backend_identity observation_backend,
      mutation_backend_identity mutation_backend,
      mutation_exclusion_domain_identity mutation_exclusion_domain,
      active_object_namespace_identity active_namespace,
      rejected_object_store_identity rejected_store,
      staging_namespace_identity staging_namespace,
      journal_namespace_identity journal_namespace,
      execution_capability_profile_identity capabilities,
      std::optional<lifecycle_executor_identity> lifecycle_executor);

  std::uint16_t schema_version_ = application_target_context_schema_version;
  application_target_context_identity identity_;
  pkgplan::target_system_context_identity target_;
  managed_target_identity managed_target_;
  root_view_identity root_view_;
  observation_backend_identity observation_backend_;
  mutation_backend_identity mutation_backend_;
  mutation_exclusion_domain_identity mutation_exclusion_domain_;
  active_object_namespace_identity active_namespace_;
  rejected_object_store_identity rejected_store_;
  staging_namespace_identity staging_namespace_;
  journal_namespace_identity journal_namespace_;
  execution_capability_profile_identity capabilities_;
  std::optional<lifecycle_executor_identity> lifecycle_executor_;
};

} // namespace pkgapply
