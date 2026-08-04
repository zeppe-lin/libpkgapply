// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <libpkgapply/digest.h>
#include <libpkgapply/state_projection.h>
#include <libpkgapply/target_context.h>

namespace pkgapply {

inline constexpr std::uint16_t mutation_lease_acquisition_schema_version = 1;
inline constexpr std::size_t mutation_lease_nonce_size = 32;

/*! \brief Mechanism-issued nonce distinguishing one physical lease acquisition. */
class mutation_lease_nonce final {
public:
  using byte_array = std::array<std::uint8_t, mutation_lease_nonce_size>;

  [[nodiscard]] static mutation_lease_nonce from_bytes(byte_array bytes);
  [[nodiscard]] const byte_array& bytes() const noexcept;

  friend bool operator==(const mutation_lease_nonce& lhs,
                         const mutation_lease_nonce& rhs) noexcept;
  friend bool operator!=(const mutation_lease_nonce& lhs,
                         const mutation_lease_nonce& rhs) noexcept;
  friend bool operator<(const mutation_lease_nonce& lhs,
                        const mutation_lease_nonce& rhs) noexcept;

private:
  explicit mutation_lease_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief Canonical identity binding one target lease acquisition instance. */
class mutation_lease_acquisition final {
public:
  [[nodiscard]] static mutation_lease_acquisition make(
      application_target_context_identity target,
      mutation_exclusion_domain_identity exclusion_domain,
      mutation_lease_nonce nonce);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const mutation_lease_instance_identity& identity() const noexcept;
  [[nodiscard]] const application_target_context_identity& target() const noexcept;
  [[nodiscard]] const mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept;
  [[nodiscard]] const mutation_lease_nonce& nonce() const noexcept;

private:
  mutation_lease_acquisition(
      mutation_lease_instance_identity identity,
      application_target_context_identity target,
      mutation_exclusion_domain_identity exclusion_domain,
      mutation_lease_nonce nonce);

  std::uint16_t schema_version_ = mutation_lease_acquisition_schema_version;
  mutation_lease_instance_identity identity_;
  application_target_context_identity target_;
  mutation_exclusion_domain_identity exclusion_domain_;
  mutation_lease_nonce nonce_;
};

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

  ~mutation_lease_error() override;

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
  virtual ~target_mutation_lease();

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

/*! \brief Validate one live lease against its immutable target scope.
 *
 * This weaker validation is for recovery paths that only observe canonical
 * target state and therefore have no truthful pre-application state
 * projection.  It proves that the lease is held for the exact application
 * target and shared exclusion domain; it does not authorize filesystem
 * application or state publication.
 *
 * Success is only a point-in-time admission check. The caller remains
 * responsible for keeping the lease held for the complete observation or
 * finalization step.
 */
void validate_target_mutation_lease_scope(
    const application_target_context& target,
    const target_mutation_lease& lease);

/*! \brief Validate one live lease against immutable application authorities.
 *
 * This full validation additionally proves that the supplied installed-state
 * projection was established under the same acquisition instance.
 *
 * Success is only a point-in-time admission check. The caller remains
 * responsible for keeping the lease held for the complete outer transaction.
 */
void validate_target_mutation_lease(
    const application_target_context& target,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

} // namespace pkgapply
