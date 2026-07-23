// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>
#include <string>

#include <libpkgapply/digest.h>
#include <libpkgapply/state_projection.h>
#include <libpkgapply/target_context.h>

namespace pkgapply {

/*! \brief Structured reason that a supplied target lease was refused. */
enum class mutation_lease_error_code {
  not_held,
  target_context_mismatch,
  exclusion_domain_mismatch,
  state_projection_mismatch,
};

/*! \brief Invalid or stale caller-held target mutation authority. */
class mutation_lease_error final : public std::invalid_argument {
public:
  mutation_lease_error(mutation_lease_error_code code, std::string message);

  [[nodiscard]] mutation_lease_error_code code() const noexcept;

private:
  mutation_lease_error_code code_;
};

/*! \brief Caller-owned live authority excluding package target mutation.
 *
 * Application borrows this object. It never acquires, releases, transfers, or
 * extends the outer transaction lease. The caller must keep the same lease
 * held through application, installed-state publication, and the selected
 * finalization or recovery decision.
 */
class target_mutation_lease {
public:
  target_mutation_lease() = default;
  target_mutation_lease(const target_mutation_lease&) = delete;
  target_mutation_lease& operator=(const target_mutation_lease&) = delete;
  target_mutation_lease(target_mutation_lease&&) = delete;
  target_mutation_lease& operator=(target_mutation_lease&&) = delete;
  virtual ~target_mutation_lease() = default;

  /*! \brief Return the unique identity of this acquisition instance. */
  [[nodiscard]] virtual const mutation_lease_instance_identity&
  identity() const noexcept = 0;

  /*! \brief Return the exact application target context protected. */
  [[nodiscard]] virtual const application_target_context_identity&
  target() const noexcept = 0;

  /*! \brief Return the shared exclusion and lock-ordering domain. */
  [[nodiscard]] virtual const mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept = 0;

  /*! \brief Return whether the caller still holds this acquisition. */
  [[nodiscard]] virtual bool held() const noexcept = 0;
};

/*! \brief Validate one live lease against immutable application authorities.
 *
 * Success is only a point-in-time admission check. The caller remains
 * responsible for keeping the lease held for the complete outer transaction.
 */
void validate_target_mutation_lease(
    const application_target_context& target,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

} // namespace pkgapply
