// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>

#include <libpkgapply/digest.h>

namespace pkgapply {

inline constexpr std::uint16_t application_attempt_schema_version = 1;
inline constexpr std::size_t application_attempt_nonce_size = 32;

/*! \brief Backend-issued durable nonce distinguishing physical attempts. */
class application_attempt_nonce final {
public:
  using byte_array =
      std::array<std::uint8_t, application_attempt_nonce_size>;

  [[nodiscard]] static application_attempt_nonce from_bytes(byte_array bytes);
  [[nodiscard]] const byte_array& bytes() const noexcept;

  friend bool operator==(const application_attempt_nonce& lhs,
                         const application_attempt_nonce& rhs) noexcept;
  friend bool operator!=(const application_attempt_nonce& lhs,
                         const application_attempt_nonce& rhs) noexcept;
  friend bool operator<(const application_attempt_nonce& lhs,
                        const application_attempt_nonce& rhs) noexcept;

private:
  explicit application_attempt_nonce(byte_array bytes);
  byte_array bytes_;
};

/*! \brief Canonical binding of one request to one physical backend attempt. */
class application_attempt final {
public:
  [[nodiscard]] static application_attempt make(
      application_request_identity request,
      application_target_context_identity target,
      mutation_backend_identity backend,
      application_attempt_nonce nonce);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_attempt_identity& identity() const noexcept;
  [[nodiscard]] const application_request_identity& request() const noexcept;
  [[nodiscard]] const application_target_context_identity& target() const noexcept;
  [[nodiscard]] const mutation_backend_identity& backend() const noexcept;
  [[nodiscard]] const application_attempt_nonce& nonce() const noexcept;

private:
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
