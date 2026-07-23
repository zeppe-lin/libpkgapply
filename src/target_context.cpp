// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/target_context.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace pkgapply {
namespace {

application_target_context_identity
identify_context(
    const pkgplan::target_system_context_identity& target,
    const managed_target_identity& managed_target,
    const root_view_identity& root_view,
    const observation_backend_identity& observation_backend,
    const mutation_backend_identity& mutation_backend,
    const mutation_exclusion_domain_identity& mutation_exclusion_domain,
    const active_object_namespace_identity& active_namespace,
    const rejected_object_store_identity& rejected_store,
    const staging_namespace_identity& staging_namespace,
    const journal_namespace_identity& journal_namespace,
    const execution_capability_profile_identity& capabilities,
    const std::optional<lifecycle_executor_identity>& lifecycle_executor)
{
  detail::canonical_record record(
      application_target_context_identity::canonical_domain());
  record.append_u16(application_target_context_schema_version);
  record.append_bytes(target.string());
  record.append_digest(managed_target);
  record.append_digest(root_view);
  record.append_digest(observation_backend);
  record.append_digest(mutation_backend);
  record.append_digest(mutation_exclusion_domain);
  record.append_digest(active_namespace);
  record.append_digest(rejected_store);
  record.append_digest(staging_namespace);
  record.append_digest(journal_namespace);
  record.append_digest(capabilities);
  record.append_bool(lifecycle_executor.has_value());
  if (lifecycle_executor)
    record.append_digest(*lifecycle_executor);

  return detail::identity_factory::from_sha256<
      application_target_context_identity>(record.sha256());
}

void
validate_distinct_namespaces(
    const active_object_namespace_identity& active_namespace,
    const rejected_object_store_identity& rejected_store,
    const staging_namespace_identity& staging_namespace,
    const journal_namespace_identity& journal_namespace)
{
  const std::string active = active_namespace.string();
  const std::string rejected = rejected_store.string();
  const std::string staging = staging_namespace.string();
  const std::string journal = journal_namespace.string();

  if (active == rejected || active == staging || active == journal ||
      rejected == staging || rejected == journal || staging == journal)
  {
    throw std::invalid_argument(
        "application target context namespaces must be distinct");
  }
}

} // namespace

application_target_context
application_target_context::make(
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
    std::optional<lifecycle_executor_identity> lifecycle_executor)
{
  validate_distinct_namespaces(active_namespace,
                               rejected_store,
                               staging_namespace,
                               journal_namespace);

  application_target_context_identity identity = identify_context(
      target,
      managed_target,
      root_view,
      observation_backend,
      mutation_backend,
      mutation_exclusion_domain,
      active_namespace,
      rejected_store,
      staging_namespace,
      journal_namespace,
      capabilities,
      lifecycle_executor);

  return application_target_context(
      std::move(identity),
      std::move(target),
      std::move(managed_target),
      std::move(root_view),
      std::move(observation_backend),
      std::move(mutation_backend),
      std::move(mutation_exclusion_domain),
      std::move(active_namespace),
      std::move(rejected_store),
      std::move(staging_namespace),
      std::move(journal_namespace),
      std::move(capabilities),
      std::move(lifecycle_executor));
}

application_target_context::application_target_context(
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
    std::optional<lifecycle_executor_identity> lifecycle_executor)
    : identity_(std::move(identity)),
      target_(std::move(target)),
      managed_target_(std::move(managed_target)),
      root_view_(std::move(root_view)),
      observation_backend_(std::move(observation_backend)),
      mutation_backend_(std::move(mutation_backend)),
      mutation_exclusion_domain_(std::move(mutation_exclusion_domain)),
      active_namespace_(std::move(active_namespace)),
      rejected_store_(std::move(rejected_store)),
      staging_namespace_(std::move(staging_namespace)),
      journal_namespace_(std::move(journal_namespace)),
      capabilities_(std::move(capabilities)),
      lifecycle_executor_(std::move(lifecycle_executor))
{
}

std::uint16_t
application_target_context::schema_version() const noexcept
{
  return schema_version_;
}

const application_target_context_identity&
application_target_context::identity() const noexcept
{
  return identity_;
}

const pkgplan::target_system_context_identity&
application_target_context::target() const noexcept
{
  return target_;
}

const managed_target_identity&
application_target_context::managed_target() const noexcept
{
  return managed_target_;
}

const root_view_identity&
application_target_context::root_view() const noexcept
{
  return root_view_;
}

const observation_backend_identity&
application_target_context::observation_backend() const noexcept
{
  return observation_backend_;
}

const mutation_backend_identity&
application_target_context::mutation_backend() const noexcept
{
  return mutation_backend_;
}

const mutation_exclusion_domain_identity&
application_target_context::mutation_exclusion_domain() const noexcept
{
  return mutation_exclusion_domain_;
}

const active_object_namespace_identity&
application_target_context::active_namespace() const noexcept
{
  return active_namespace_;
}

const rejected_object_store_identity&
application_target_context::rejected_store() const noexcept
{
  return rejected_store_;
}

const staging_namespace_identity&
application_target_context::staging_namespace() const noexcept
{
  return staging_namespace_;
}

const journal_namespace_identity&
application_target_context::journal_namespace() const noexcept
{
  return journal_namespace_;
}

const execution_capability_profile_identity&
application_target_context::capabilities() const noexcept
{
  return capabilities_;
}

const std::optional<lifecycle_executor_identity>&
application_target_context::lifecycle_executor() const noexcept
{
  return lifecycle_executor_;
}

bool
operator==(const application_target_context& lhs,
           const application_target_context& rhs) noexcept
{
  return lhs.identity_ == rhs.identity_ && lhs.target_ == rhs.target_ &&
         lhs.managed_target_ == rhs.managed_target_ &&
         lhs.root_view_ == rhs.root_view_ &&
         lhs.observation_backend_ == rhs.observation_backend_ &&
         lhs.mutation_backend_ == rhs.mutation_backend_ &&
         lhs.mutation_exclusion_domain_ == rhs.mutation_exclusion_domain_ &&
         lhs.active_namespace_ == rhs.active_namespace_ &&
         lhs.rejected_store_ == rhs.rejected_store_ &&
         lhs.staging_namespace_ == rhs.staging_namespace_ &&
         lhs.journal_namespace_ == rhs.journal_namespace_ &&
         lhs.capabilities_ == rhs.capabilities_ &&
         lhs.lifecycle_executor_ == rhs.lifecycle_executor_;
}

bool
operator!=(const application_target_context& lhs,
           const application_target_context& rhs) noexcept
{
  return !(lhs == rhs);
}

} // namespace pkgapply
