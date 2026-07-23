// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/mutation_lease.h>

#include <utility>

namespace pkgapply {

mutation_lease_error::mutation_lease_error(
    mutation_lease_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

mutation_lease_error_code
mutation_lease_error::code() const noexcept
{
  return code_;
}

void
validate_target_mutation_lease(
    const application_target_context& target,
    const lease_bound_state_projection& state,
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

  if (lease.identity() != state.lease()) {
    throw mutation_lease_error(
        mutation_lease_error_code::state_projection_mismatch,
        "state projection was not established under this lease instance");
  }
}

} // namespace pkgapply
