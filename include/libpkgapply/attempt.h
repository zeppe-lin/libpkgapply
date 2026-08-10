// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file attempt.h
 *  \brief Canonical identity of one physical application attempt.
 */
#pragma once

#include <libpkgapply/export.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <libpkgapply/digest.h>

namespace pkgapply {

/*! \brief Schema version of application_attempt. */
inline constexpr std::uint16_t application_attempt_schema_version = 1;

/*! \brief Number of bytes in a backend-issued attempt nonce. */
inline constexpr std::size_t application_attempt_nonce_size = 32;

/*! \brief Backend-issued durable nonce distinguishing physical attempts. */
class PKGAPPLY_API application_attempt_nonce final {
public:
  /*! \brief Fixed-size byte representation of an attempt nonce. */
  using byte_array =
      std::array<std::uint8_t, application_attempt_nonce_size>;

  /*! \brief Construct a nonce from exact backend-issued bytes.
   *  \param bytes Complete nonce bytes.
   *  \return Immutable nonce retaining those bytes.
   */
  [[nodiscard]] static application_attempt_nonce from_bytes(byte_array bytes);

  /*! \brief Return the exact nonce bytes.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const byte_array& bytes() const noexcept;

  /*! \brief Compare attempt nonces for equality.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when every nonce byte is equal.
   */
  friend PKGAPPLY_API bool operator==(const application_attempt_nonce& lhs,
                         const application_attempt_nonce& rhs) noexcept;

  /*! \brief Compare attempt nonces for inequality.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when any nonce byte differs.
   */
  friend PKGAPPLY_API bool operator!=(const application_attempt_nonce& lhs,
                         const application_attempt_nonce& rhs) noexcept;

  /*! \brief Order attempt nonces lexicographically by bytes.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when `lhs` precedes `rhs`.
   */
  friend PKGAPPLY_API bool operator<(const application_attempt_nonce& lhs,
                        const application_attempt_nonce& rhs) noexcept;

private:
  /*! \brief Construct a validated complete nonce. */
  explicit application_attempt_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief Canonical binding of one request to one physical backend attempt.
 *
 *  An application request is semantic and may be retried. This value binds
 *  that request to one exact target context, mutation backend, and
 *  backend-issued nonce so journal and recovery evidence cannot be replayed
 *  across physical attempts.
 */
class PKGAPPLY_API application_attempt final {
public:
  /*! \brief Identify one physical application attempt.
   *  \param request Complete semantic application-request identity.
   *  \param target Exact target-context identity.
   *  \param backend Exact mutation-backend identity.
   *  \param nonce Backend-issued durable attempt nonce.
   *  \return Immutable canonical attempt binding.
   */
  [[nodiscard]] static application_attempt make(
      application_request_identity request,
      application_target_context_identity target,
      mutation_backend_identity backend,
      application_attempt_nonce nonce);

  /*! \brief Return the attempt schema version.
   *  \return application_attempt_schema_version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  /*! \brief Return the canonical attempt identity.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const application_attempt_identity& identity() const noexcept;

  /*! \brief Return the bound application-request identity.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const application_request_identity& request() const noexcept;

  /*! \brief Return the bound target-context identity.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const application_target_context_identity&
  target() const noexcept;

  /*! \brief Return the bound mutation-backend identity.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const mutation_backend_identity& backend() const noexcept;

  /*! \brief Return the physical attempt nonce.
   *  \return Reference valid for the lifetime of this value.
   */
  [[nodiscard]] const application_attempt_nonce& nonce() const noexcept;

private:
  /*! \brief Construct authority already identified by make(). */
  application_attempt(
      application_attempt_identity identity,
      application_request_identity request,
      application_target_context_identity target,
      mutation_backend_identity backend,
      application_attempt_nonce nonce);

  std::uint16_t schema_version_ = application_attempt_schema_version;
  application_attempt_identity identity_;
  application_request_identity request_;
  application_target_context_identity target_;
  mutation_backend_identity backend_;
  application_attempt_nonce nonce_;
};

} // namespace pkgapply
