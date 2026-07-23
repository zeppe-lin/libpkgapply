// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

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

/*! \brief Structured reason that immutable application authorities disagree. */
enum class application_admission_error_code {
  unsupported_request_schema,
  unsupported_plan_schema,
  operation_kind_mismatch,
  target_context_mismatch,
  backend_identity_mismatch,
  observation_backend_mismatch,
  capability_profile_mismatch,
  incomplete_state_projection,
  installed_snapshot_mismatch,
  ownership_inventory_mismatch,
  state_path_universe_mismatch,
  state_path_owners_mismatch,
  incoming_archive_precondition_missing,
  unexpected_incoming_archive_precondition,
  archive_digest_mismatch,
  package_image_mismatch,
  inspection_receipt_mismatch,
  incoming_entry_missing,
  incoming_entry_path_mismatch,
  transaction_backend_mismatch,
  transaction_observation_backend_mismatch,
  transaction_capability_mismatch,
  transaction_target_mismatch,
  transaction_lease_mismatch,
};

/*! \brief Invalid or stale authority universe supplied for application. */
class application_admission_error final : public std::invalid_argument {
public:
  application_admission_error(
      application_admission_error_code code,
      std::string message,
      std::vector<pkgplan::package_path> paths = {});

  [[nodiscard]] application_admission_error_code code() const noexcept;
  [[nodiscard]] const std::vector<pkgplan::package_path>& paths() const noexcept;

private:
  application_admission_error_code code_;
  std::vector<pkgplan::package_path> paths_;
};

/*! \brief Validate one installation authority universe before target mutation. */
void validate_application_admission(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Validate one upgrade authority universe before target mutation. */
void validate_application_admission(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Validate one removal authority universe without incoming archive facts. */
void validate_application_admission(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend);

/*! \brief Validate one backend transaction opened for an admitted request. */
void validate_backend_transaction(
    const application_target_context& target,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_backend_transaction& transaction);

} // namespace pkgapply
