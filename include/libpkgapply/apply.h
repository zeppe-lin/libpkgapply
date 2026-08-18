// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file apply.h
 *  \brief Terminal execution of admitted package-operation plans.
 */
#pragma once

#include <libpkgapply/export.h>

#include <libpkgapply/backend.h>
#include <libpkgapply/journal_transport.h>
#include <libpkgapply/mutation_lease.h>
#include <libpkgapply/request.h>
#include <libpkgapply/result.h>
#include <libpkgapply/state_projection.h>
#include <libpkgimage/package_archive.h>

namespace pkgapply {

/*! \brief Apply one accepted installation plan through one backend transaction.
 *
 *  The caller retains the outer target mutation lease for the complete call.
 *  Typed physical interruptions are recovered according to execution control
 *  before this function returns a truthful terminal receipt.
 *
 *  \param request Exact immutable installation authority.
 *  \param state Current state projection bound to `lease`.
 *  \param lease Mutable borrowed caller-held mutation lease.
 *  \param backend Physical mechanism provider selected by the controller.
 *  \param journal_store Separate durable store for owner-authored journal history.
 *  \param archive Exact incoming archive retained by the caller.
 *  \return Truthful terminal receipt for this physical attempt.
 *  \throws application_admission_error If authority, state, lease, backend, or
 *          archive facts do not bind exactly.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
apply(const installation_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      application_journal_store& journal_store,
      const pkgimage::package_archive& archive);

/*! \brief Apply one accepted upgrade plan through one backend transaction.
 *
 *  The incoming archive remains caller-owned and is replayed only through its
 *  retained package-archive authority.
 *
 *  \param request Exact immutable upgrade authority.
 *  \param state Current state projection bound to `lease`.
 *  \param lease Mutable borrowed caller-held mutation lease.
 *  \param backend Physical mechanism provider selected by the controller.
 *  \param journal_store Separate durable store for owner-authored journal history.
 *  \param archive Exact incoming archive retained by the caller.
 *  \return Truthful terminal receipt for this physical attempt.
 *  \throws application_admission_error If authority, state, lease, backend, or
 *          archive facts do not bind exactly.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
apply(const upgrade_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      application_journal_store& journal_store,
      const pkgimage::package_archive& archive);

/*! \brief Apply one accepted removal plan without incoming archive authority.
 *  \param request Exact immutable removal authority.
 *  \param state Current state projection bound to `lease`.
 *  \param lease Mutable borrowed caller-held mutation lease.
 *  \param backend Physical mechanism provider selected by the controller.
 *  \param journal_store Separate durable store for owner-authored journal history.
 *  \return Truthful terminal receipt for this physical attempt.
 *  \throws application_admission_error If authority, state, lease, or backend
 *          facts do not bind exactly.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
apply(const removal_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      application_journal_store& journal_store);

} // namespace pkgapply
