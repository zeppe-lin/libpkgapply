// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file admission.h
 *  \brief Cross-authority admission before package target mutation.
 */
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgapply/request.h>
#include <libpkgapply/state_projection.h>
#include <libpkgimage/package_archive.h>
#include <libpkgplan/package_path.h>

namespace pkgapply {

/*! \brief Stable reason that application authorities were refused. */
enum class application_admission_error_code {
  unsupported_request_schema, /*!< Application request schema is unsupported. */
  unsupported_plan_schema, /*!< Accepted planner schema is unsupported. */
  operation_kind_mismatch, /*!< Request body and plan operation differ. */
  target_context_mismatch, /*!< Plan and application target differ. */
  backend_identity_mismatch, /*!< Mutation provider differs from target context. */
  observation_backend_mismatch, /*!< Observation provider differs from context. */
  capability_profile_mismatch, /*!< Provider capabilities differ from context. */
  incomplete_state_projection, /*!< State projection is not admission-complete. */
  installed_snapshot_mismatch, /*!< Installed snapshot differs from plan input. */
  ownership_inventory_mismatch, /*!< Ownership inventory differs from plan input. */
  state_path_universe_mismatch, /*!< Projected and planned path universes differ. */
  state_path_owners_mismatch, /*!< Current owners differ from plan preconditions. */
  incoming_archive_precondition_missing, /*!< Incoming plan lacks archive authority. */
  unexpected_incoming_archive_precondition, /*!< Removal carries archive authority. */
  archive_digest_mismatch, /*!< Supplied archive bytes differ from admitted bytes. */
  package_image_mismatch, /*!< Normalized image differs from admitted image. */
  inspection_receipt_mismatch, /*!< Inspection receipt differs from admission. */
  incoming_entry_missing, /*!< Plan references an absent incoming image entry. */
  incoming_entry_path_mismatch, /*!< Referenced image entry has another path. */
  transaction_backend_mismatch, /*!< Transaction reports another mutation backend. */
  transaction_observation_backend_mismatch, /*!< Transaction reports another observer. */
  transaction_capability_mismatch, /*!< Transaction reports another capability set. */
  transaction_target_mismatch, /*!< Transaction belongs to another target context. */
  transaction_lease_mismatch, /*!< Transaction belongs to another lease instance. */
};

/*! \brief Invalid, stale, or cross-bound authority universe. */
class application_admission_error final : public std::invalid_argument {
public:
  /*! \brief Construct an admission refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   *  \param paths Canonical paths implicated by the refusal, when applicable.
   */
  application_admission_error(
      application_admission_error_code code,
      std::string message,
      std::vector<pkgplan::package_path> paths = {});

  /*! \brief Destroy the polymorphic refusal. */
  ~application_admission_error() override;

  /*! \brief Return the stable refusal category. */
  [[nodiscard]] application_admission_error_code code() const noexcept;

  /*! \brief Return canonical paths implicated by the refusal. */
  [[nodiscard]] const std::vector<pkgplan::package_path>&
  paths() const noexcept;

private:
  application_admission_error_code code_;
  std::vector<pkgplan::package_path> paths_;
};

/*! \brief Validate an installation authority universe before mutation.
 *  \param request Immutable installation application request.
 *  \param state Complete state projection established under `lease`.
 *  \param lease Borrowed caller-held target mutation lease.
 *  \param backend Semantic application backend selected by the target context.
 *  \param archive Exact incoming archive retained by the caller.
 *  \throws application_admission_error If any authority, identity, provider,
 *          path universe, archive, image, receipt, or incoming entry differs.
 *  \throws mutation_lease_error If the lease is stale or cross-bound.
 */
void validate_application_admission(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Validate an upgrade authority universe before mutation.
 *  \param request Immutable upgrade application request.
 *  \param state Complete state projection established under `lease`.
 *  \param lease Borrowed caller-held target mutation lease.
 *  \param backend Semantic application backend selected by the target context.
 *  \param archive Exact incoming archive retained by the caller.
 *  \throws application_admission_error If any authority, identity, provider,
 *          path universe, archive, image, receipt, or incoming entry differs.
 *  \throws mutation_lease_error If the lease is stale or cross-bound.
 */
void validate_application_admission(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Validate a removal authority universe before mutation.
 *  \param request Immutable removal application request.
 *  \param state Complete state projection established under `lease`.
 *  \param lease Borrowed caller-held target mutation lease.
 *  \param backend Semantic application backend selected by the target context.
 *  \throws application_admission_error If any authority, identity, provider,
 *          path universe, or removal precondition differs.
 *  \throws mutation_lease_error If the lease is stale or cross-bound.
 */
void validate_application_admission(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend);

/*! \brief Validate a backend transaction opened for admitted authorities.
 *  \param target Immutable target context.
 *  \param lease Borrowed caller-held mutation lease.
 *  \param backend Backend that opened `transaction`.
 *  \param transaction Open backend transaction to validate.
 *  \throws application_admission_error If the transaction reports another
 *          provider, capability profile, target, or lease acquisition.
 */
void validate_backend_transaction(
    const application_target_context& target,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_backend_transaction& transaction);

} // namespace pkgapply
