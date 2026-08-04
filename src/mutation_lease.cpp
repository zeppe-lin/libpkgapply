// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/mutation_lease.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <utility>

namespace pkgapply {

target_mutation_lease::~target_mutation_lease() = default;

mutation_lease_nonce
mutation_lease_nonce::from_bytes(byte_array bytes)
{
  return mutation_lease_nonce(std::move(bytes));
}

mutation_lease_nonce::mutation_lease_nonce(byte_array bytes)
    : bytes_(std::move(bytes))
{
}

const mutation_lease_nonce::byte_array&
mutation_lease_nonce::bytes() const noexcept
{
  return bytes_;
}

bool
operator==(const mutation_lease_nonce& lhs,
           const mutation_lease_nonce& rhs) noexcept
{
  return lhs.bytes_ == rhs.bytes_;
}

bool
operator!=(const mutation_lease_nonce& lhs,
           const mutation_lease_nonce& rhs) noexcept
{
  return !(lhs == rhs);
}

bool
operator<(const mutation_lease_nonce& lhs,
          const mutation_lease_nonce& rhs) noexcept
{
  return lhs.bytes_ < rhs.bytes_;
}

mutation_lease_acquisition
mutation_lease_acquisition::make(
    application_target_context_identity target,
    mutation_exclusion_domain_identity exclusion_domain,
    mutation_lease_nonce nonce)
{
  detail::canonical_record record(
      mutation_lease_instance_identity::canonical_domain());
  record.append_u16(mutation_lease_acquisition_schema_version);
  record.append_digest(target);
  record.append_digest(exclusion_domain);
  for (const auto byte : nonce.bytes())
    record.append_u8(byte);

  auto identity = detail::identity_factory::from_sha256<
      mutation_lease_instance_identity>(record.sha256());
  return mutation_lease_acquisition(
      std::move(identity), std::move(target), std::move(exclusion_domain),
      std::move(nonce));
}

mutation_lease_acquisition::mutation_lease_acquisition(
    mutation_lease_instance_identity identity,
    application_target_context_identity target,
    mutation_exclusion_domain_identity exclusion_domain,
    mutation_lease_nonce nonce)
    : identity_(std::move(identity)), target_(std::move(target)),
      exclusion_domain_(std::move(exclusion_domain)), nonce_(std::move(nonce))
{
}

std::uint16_t
mutation_lease_acquisition::schema_version() const noexcept
{
  return schema_version_;
}

const mutation_lease_instance_identity&
mutation_lease_acquisition::identity() const noexcept
{
  return identity_;
}

const application_target_context_identity&
mutation_lease_acquisition::target() const noexcept
{
  return target_;
}

const mutation_exclusion_domain_identity&
mutation_lease_acquisition::exclusion_domain() const noexcept
{
  return exclusion_domain_;
}

const mutation_lease_nonce&
mutation_lease_acquisition::nonce() const noexcept
{
  return nonce_;
}

mutation_lease_error::mutation_lease_error(
    mutation_lease_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

mutation_lease_error::~mutation_lease_error() = default;

mutation_lease_error_code
mutation_lease_error::code() const noexcept
{
  return code_;
}

void
validate_target_mutation_lease_scope(
    const application_target_context& target,
    const target_mutation_lease& lease)
{
  if (!lease.held()) {
    throw mutation_lease_error(
        mutation_lease_error_code::not_held,
        "target mutation lease is not held");
  }

  if (lease.target() != target.identity()) {
    throw mutation_lease_error(
        mutation_lease_error_code::target_context_mismatch,
        "target mutation lease belongs to another application context");
  }

  if (lease.exclusion_domain() != target.mutation_exclusion_domain()) {
    throw mutation_lease_error(
        mutation_lease_error_code::exclusion_domain_mismatch,
        "target mutation lease belongs to another exclusion domain");
  }
}

void
validate_target_mutation_lease(
    const application_target_context& target,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease)
{
  validate_target_mutation_lease_scope(target, lease);

  if (lease.identity() != state.lease()) {
    throw mutation_lease_error(
        mutation_lease_error_code::state_projection_mismatch,
        "state projection was not established under this lease instance");
  }
}

} // namespace pkgapply
