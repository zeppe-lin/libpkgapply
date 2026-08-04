// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file target_context.h
 *  \brief Immutable application-facing projection of a managed target.
 */
#pragma once

#include <cstdint>
#include <optional>

#include <libpkgapply/digest.h>
#include <libpkgplan/digest.h>

namespace pkgapply {

/*! \brief Schema version of application_target_context. */
inline constexpr std::uint16_t application_target_context_schema_version = 1;

/*! \brief Immutable actuator projection of one managed target context.
 *
 *  The context binds planner-owned target identity to the exact root view,
 *  observation and mutation providers, exclusion domain, storage namespaces,
 *  capability profile, and optional lifecycle executor selected by
 *  orchestration. It identifies authority; it does not acquire resources.
 */
class application_target_context final {
public:
  /*! \brief Validate, identify, and construct one target context.
   *  \param target Planner-owned managed target identity.
   *  \param managed_target Orchestrator identity of that managed target.
   *  \param root_view Root view used for observation and mutation.
   *  \param observation_backend Exact observation provider identity.
   *  \param mutation_backend Exact mutation provider identity.
   *  \param mutation_exclusion_domain Shared mutation lock-ordering domain.
   *  \param active_namespace Namespace containing active target objects.
   *  \param rejected_store Store receiving rejected-object evidence.
   *  \param staging_namespace Namespace used for incoming payload staging.
   *  \param journal_namespace Namespace used for durable application journals.
   *  \param capabilities Backend capability-profile identity.
   *  \param lifecycle_executor Optional lifecycle-executor identity.
   *  \return Immutable identified target context.
   *  \throws std::invalid_argument If any two storage namespaces are equal.
   */
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

  /*! \brief Return the target-context schema version.
   *  \return application_target_context_schema_version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  /*! \brief Return the canonical context identity.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const application_target_context_identity&
  identity() const noexcept;

  /*! \brief Return the planner-owned target identity.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const pkgplan::target_system_context_identity&
  target() const noexcept;

  /*! \brief Return the orchestrator-managed target identity. */
  [[nodiscard]] const managed_target_identity& managed_target() const noexcept;

  /*! \brief Return the exact root-view identity. */
  [[nodiscard]] const root_view_identity& root_view() const noexcept;

  /*! \brief Return the observation-provider identity. */
  [[nodiscard]] const observation_backend_identity&
  observation_backend() const noexcept;

  /*! \brief Return the mutation-provider identity. */
  [[nodiscard]] const mutation_backend_identity&
  mutation_backend() const noexcept;

  /*! \brief Return the shared mutation-exclusion domain. */
  [[nodiscard]] const mutation_exclusion_domain_identity&
  mutation_exclusion_domain() const noexcept;

  /*! \brief Return the active-object namespace identity. */
  [[nodiscard]] const active_object_namespace_identity&
  active_namespace() const noexcept;

  /*! \brief Return the rejected-object store identity. */
  [[nodiscard]] const rejected_object_store_identity&
  rejected_store() const noexcept;

  /*! \brief Return the staging namespace identity. */
  [[nodiscard]] const staging_namespace_identity&
  staging_namespace() const noexcept;

  /*! \brief Return the application-journal namespace identity. */
  [[nodiscard]] const journal_namespace_identity&
  journal_namespace() const noexcept;

  /*! \brief Return the execution capability-profile identity. */
  [[nodiscard]] const execution_capability_profile_identity&
  capabilities() const noexcept;

  /*! \brief Return the optional lifecycle-executor identity. */
  [[nodiscard]] const std::optional<lifecycle_executor_identity>&
  lifecycle_executor() const noexcept;

  /*! \brief Compare complete target contexts for equality.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when every bound authority is equal.
   */
  friend bool operator==(const application_target_context& lhs,
                         const application_target_context& rhs) noexcept;

  /*! \brief Compare complete target contexts for inequality.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when any bound authority differs.
   */
  friend bool operator!=(const application_target_context& lhs,
                         const application_target_context& rhs) noexcept;

private:
  /*! \brief Construct a validated context already identified by make(). */
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
