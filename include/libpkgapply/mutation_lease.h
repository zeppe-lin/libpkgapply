// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file mutation_lease.h
 *  \brief Caller-held target mutation authority and admission checks.
 */
#pragma once

#include <libpkgapply/export.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <libpkgapply/digest.h>
#include <libpkgapply/state_projection.h>
#include <libpkgapply/target_context.h>

namespace pkgapply {

/*! \brief Schema version of mutation_lease_acquisition. */
inline constexpr std::uint16_t mutation_lease_acquisition_schema_version = 1;

/*! \brief Number of bytes in a mechanism-issued lease nonce. */
inline constexpr std::size_t mutation_lease_nonce_size = 32;

/*! \brief Mechanism-issued nonce distinguishing a physical lease acquisition. */
class PKGAPPLY_API mutation_lease_nonce final {
public:
  /*! \brief Fixed-size byte representation of a lease nonce. */
  using byte_array = std::array<std::uint8_t, mutation_lease_nonce_size>;

  /*! \brief Construct a nonce from exact mechanism-issued bytes.
   *  \param bytes Complete nonce bytes.
   *  \return Immutable nonce retaining those bytes.
   */
  [[nodiscard]] static mutation_lease_nonce from_bytes(byte_array bytes);

  /*!
   * \brief Return the exact nonce bytes.
  *  \return The exact nonce bytes.
   */
  [[nodiscard]] const byte_array& bytes() const noexcept;

  /*!
   * \brief Compare lease nonces for equality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs are equal.
   */
  friend PKGAPPLY_API bool operator==(const mutation_lease_nonce& lhs,
                         const mutation_lease_nonce& rhs) noexcept;

  /*!
   * \brief Compare lease nonces for inequality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs differ.
   */
  friend PKGAPPLY_API bool operator!=(const mutation_lease_nonce& lhs,
                         const mutation_lease_nonce& rhs) noexcept;

  /*!
   * \brief Order lease nonces lexicographically by bytes.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const mutation_lease_nonce& lhs,
                        const mutation_lease_nonce& rhs) noexcept;

private:
  /*! \brief Construct a complete mechanism-issued nonce. */
  explicit mutation_lease_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief Canonical identity of one target lease acquisition instance. */
class PKGAPPLY_API mutation_lease_acquisition final {
public:
  /*! \brief Identify one physical lease acquisition.
   *  \param target Exact application target context protected by the lease.
   *  \param exclusion_domain Shared lock-ordering domain.
   *  \param nonce Mechanism-issued durable nonce.
   *  \return Immutable acquisition authority.
   */
  [[nodiscard]] static mutation_lease_acquisition make(
      application_target_context_identity target,
      mutation_exclusion_domain_identity exclusion_domain,
      mutation_lease_nonce nonce);

  /*!
   * \brief Return the acquisition schema version.
  *  \return The acquisition schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  /*!
   * \brief Return the unique acquisition identity.
  *  \return The unique acquisition identity.
   */
  [[nodiscard]] const mutation_lease_instance_identity&
  identity() const noexcept;

  /*!
   * \brief Return the protected target-context identity.
  *  \return The protected target-context identity.
   */
  [[nodiscard]] const application_target_context_identity&
  target() const noexcept;

  /*!
   * \brief Return the shared mutation-exclusion domain.
  *  \return The shared mutation-exclusion domain.
   */
  [[nodiscard]] const mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept;

  /*!
   * \brief Return the physical acquisition nonce.
  *  \return The physical acquisition nonce.
   */
  [[nodiscard]] const mutation_lease_nonce& nonce() const noexcept;

private:
  /*! \brief Construct authority already identified by make(). */
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

/*! \brief Stable reason that a supplied mutation lease was refused. */
enum class mutation_lease_error_code {
  not_held, /*!< The caller no longer holds the lease. */
  target_context_mismatch, /*!< The lease protects another target context. */
  exclusion_domain_mismatch, /*!< The lease belongs to another lock domain. */
  state_projection_mismatch, /*!< State was observed under another acquisition. */
};

/*! \brief Invalid or stale caller-held target mutation authority. */
class PKGAPPLY_API mutation_lease_error final : public std::invalid_argument {
public:
  /*! \brief Construct a lease refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  mutation_lease_error(mutation_lease_error_code code, std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~mutation_lease_error() override;

  /*!
   * \brief Return the stable refusal category.
  *  \return The stable refusal category.
   */
  [[nodiscard]] mutation_lease_error_code code() const noexcept;

private:
  mutation_lease_error_code code_;
};

/*! \brief Caller-owned live authority excluding package target mutation.
 *
 *  Application borrows this object. It never acquires, releases, transfers, or
 *  extends the outer transaction lease. The caller must keep the same lease
 *  held through application, installed-state publication, and the selected
 *  finalization or recovery decision.
 */
class PKGAPPLY_API target_mutation_lease {
public:
  /*! \brief Construct an interface base. */
  target_mutation_lease() = default;
  /*! \brief Lease interfaces are not copy-constructible. */
  target_mutation_lease(const target_mutation_lease&) = delete;
  /*! \brief Lease interfaces are not copy-assignable. */
  target_mutation_lease& operator=(const target_mutation_lease&) = delete;
  /*! \brief Lease interfaces are not move-constructible. */
  target_mutation_lease(target_mutation_lease&&) = delete;
  /*! \brief Lease interfaces are not move-assignable. */
  target_mutation_lease& operator=(target_mutation_lease&&) = delete;

  /*! \brief Destroy the live lease interface. */
  virtual ~target_mutation_lease();

  /*!
   * \brief Return the unique acquisition identity.
  *  \return The unique acquisition identity.
   */
  [[nodiscard]] virtual const mutation_lease_instance_identity&
  identity() const noexcept = 0;

  /*!
   * \brief Return the exact target context protected.
  *  \return The exact target context protected.
   */
  [[nodiscard]] virtual const application_target_context_identity&
  target() const noexcept = 0;

  /*!
   * \brief Return the shared exclusion and lock-ordering domain.
  *  \return The shared exclusion and lock-ordering domain.
   */
  [[nodiscard]] virtual const mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept = 0;

  /*!
   * \brief Report whether the caller still holds this acquisition.
  *  \return Whether the caller still holds this acquisition.
   */
  [[nodiscard]] virtual bool held() const noexcept = 0;
};

/*! \brief Validate live lease scope without a state projection.
 *  \param target Immutable target scope expected by the caller.
 *  \param lease Borrowed caller-held lease to validate.
 *  \throws mutation_lease_error If the lease is not held or protects another
 *          target context or exclusion domain.
 *
 *  This weaker check is for recovery paths that have no truthful
 *  pre-application state projection. Success is point-in-time admission only;
 *  the caller remains responsible for retaining the lease.
 */
PKGAPPLY_API void validate_target_mutation_lease_scope(
    const application_target_context& target,
    const target_mutation_lease& lease);

/*! \brief Validate live lease scope and state-observation binding.
 *  \param target Immutable target scope expected by the request.
 *  \param state Installed-state projection established under a lease.
 *  \param lease Borrowed caller-held lease to validate.
 *  \throws mutation_lease_error If scope is invalid or the projection was
 *          established under another acquisition instance.
 */
PKGAPPLY_API void validate_target_mutation_lease(
    const application_target_context& target,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

} // namespace pkgapply
