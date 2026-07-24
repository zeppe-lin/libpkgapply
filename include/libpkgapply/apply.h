// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <libpkgapply/backend.h>
#include <libpkgapply/mutation_lease.h>
#include <libpkgapply/request.h>
#include <libpkgapply/result.h>
#include <libpkgapply/state_projection.h>
#include <libpkgimage/package_archive.h>

namespace pkgapply {

/*!
 * \brief Apply one accepted installation plan through one backend transaction.
 *
 * The caller retains the outer target mutation lease for the complete call.
 * Typed physical interruptions are recovered according to the request's
 * execution control before this function returns a terminal receipt.
 */
[[nodiscard]] application_receipt
apply(const installation_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      const pkgimage::package_archive& archive);

/*!
 * \brief Apply one accepted upgrade plan through one backend transaction.
 *
 * The incoming archive remains caller-owned and is replayed only through its
 * retained package_archive authority.
 */
[[nodiscard]] application_receipt
apply(const upgrade_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      const pkgimage::package_archive& archive);

/*!
 * \brief Apply one accepted removal plan without incoming archive authority.
 */
[[nodiscard]] application_receipt
apply(const removal_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend);

} // namespace pkgapply
