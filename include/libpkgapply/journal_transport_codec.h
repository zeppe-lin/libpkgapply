// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file journal_transport_codec.h
 *  \brief Canonical durable encoding of append-only journal transport values.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/journal_transport.h>

namespace pkgapply {

/*! \brief Wire-format version of append-only journal transport values. */
inline constexpr std::uint16_t application_journal_transport_encoding_version = 1;

/*! \brief Maximum accepted encoded size of one declaration, step, or cursor. */
inline constexpr std::size_t maximum_application_journal_transport_encoding_size =
    256U * 1024U * 1024U;

/*! \brief Stable reason that append-only journal transport bytes were refused. */
enum class application_journal_transport_codec_error_code : std::uint8_t {
  invalid_magic = 1, /*!< Format magic does not identify the requested value. */
  unsupported_version = 2, /*!< Encoding or represented schema is unsupported. */
  truncated = 3, /*!< Byte stream ends before a required field. */
  invalid_value = 4, /*!< Decoded field is outside the represented protocol. */
  limit_exceeded = 5, /*!< Encoded length or count exceeds its ceiling. */
  trailing_data = 6, /*!< Bytes remain after the complete value. */
  identity_mismatch = 7, /*!< Decoded fields do not reproduce the retained identity. */
};

/*! \brief Malformed or self-contradictory append-only journal bytes. */
class PKGAPPLY_API application_journal_transport_codec_error final
    : public std::invalid_argument {
public:
  /*! \brief Construct one transport-codec refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  application_journal_transport_codec_error(
      application_journal_transport_codec_error_code code,
      std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~application_journal_transport_codec_error() override;

  /*! \brief Return the stable refusal category.
   *  \return Stable refusal category.
   */
  [[nodiscard]] application_journal_transport_codec_error_code
  code() const noexcept;

private:
  application_journal_transport_codec_error_code code_;
};

/*! \brief Complete canonical byte stream for one append-only transport value. */
using application_journal_transport_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one immutable application-journal declaration.
 *  \param declaration Exact owner-authored declaration.
 *  \return Canonical versioned bytes.
 *  \throws application_journal_transport_codec_error If the value exceeds
 *          the transport size ceiling.
 */
[[nodiscard]] PKGAPPLY_API application_journal_transport_encoding
encode_application_journal_declaration(
    const application_journal_declaration& declaration);

/*! \brief Decode and revalidate one immutable declaration byte stream.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \return Reconstructed identity-checked declaration.
 *  \throws application_journal_transport_codec_error For malformed,
 *          unsupported, oversized, trailing, or identity-inconsistent bytes.
 */
[[nodiscard]] PKGAPPLY_API application_journal_declaration
decode_application_journal_declaration(const std::uint8_t* data,
                                       std::size_t size);

/*! \brief Decode and revalidate one immutable declaration vector.
 *  \param encoding Complete candidate encoding.
 *  \return Reconstructed identity-checked declaration.
 */
[[nodiscard]] PKGAPPLY_API application_journal_declaration
decode_application_journal_declaration(
    const application_journal_transport_encoding& encoding);

/*! \brief Encode one immutable application-journal step.
 *  \param step Exact owner-authored step.
 *  \return Canonical versioned bytes.
 */
[[nodiscard]] PKGAPPLY_API application_journal_transport_encoding
encode_application_journal_step(const application_journal_step& step);

/*! \brief Decode and revalidate one immutable journal step byte stream.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \return Reconstructed identity-checked step.
 */
[[nodiscard]] PKGAPPLY_API application_journal_step
decode_application_journal_step(const std::uint8_t* data, std::size_t size);

/*! \brief Decode and revalidate one immutable journal step vector.
 *  \param encoding Complete candidate encoding.
 *  \return Reconstructed identity-checked step.
 */
[[nodiscard]] PKGAPPLY_API application_journal_step
decode_application_journal_step(
    const application_journal_transport_encoding& encoding);

/*! \brief Encode one bounded application-journal cursor.
 *  \param cursor Exact owner-authored cursor.
 *  \return Canonical versioned bytes.
 */
[[nodiscard]] PKGAPPLY_API application_journal_transport_encoding
encode_application_journal_cursor(const application_journal_cursor& cursor);

/*! \brief Decode and revalidate one bounded journal cursor byte stream.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \return Reconstructed identity-checked cursor.
 */
[[nodiscard]] PKGAPPLY_API application_journal_cursor
decode_application_journal_cursor(const std::uint8_t* data, std::size_t size);

/*! \brief Decode and revalidate one bounded journal cursor vector.
 *  \param encoding Complete candidate encoding.
 *  \return Reconstructed identity-checked cursor.
 */
[[nodiscard]] PKGAPPLY_API application_journal_cursor
decode_application_journal_cursor(
    const application_journal_transport_encoding& encoding);

} // namespace pkgapply
