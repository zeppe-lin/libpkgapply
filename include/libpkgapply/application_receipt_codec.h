// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file application_receipt_codec.h
 *  \brief Canonical durable application-receipt codec.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/result.h>

namespace pkgapply {

/*! \brief Wire-format version of an application receipt. */
inline constexpr std::uint16_t application_receipt_encoding_version = 1;

/*! \brief Maximum accepted byte length of an application receipt. */
inline constexpr std::size_t maximum_application_receipt_encoding_size =
    512U * 1024U * 1024U;

/*! \brief Stable reason that application-receipt bytes were refused. */
enum class application_receipt_codec_error_code : std::uint8_t {
  invalid_magic = 1, /*!< The format magic does not identify this protocol. */
  unsupported_version = 2, /*!< The encoding version is unsupported. */
  truncated = 3, /*!< The byte stream ends before a required field. */
  invalid_value = 4, /*!< A decoded enum, count, or field value is invalid. */
  limit_exceeded = 5, /*!< A declared size or count exceeds its ceiling. */
  trailing_data = 6, /*!< Bytes remain after the complete canonical record. */
  checksum_mismatch = 7, /*!< Record-body checksum does not match. */
  identity_mismatch = 8, /*!< Fields do not reproduce the encoded identity. */
  request_mismatch = 9, /*!< The record names another immutable request. */
  completed_evidence_invalid = 10, /*!< Nested completed evidence is invalid. */
  noncanonical_encoding = 11, /*!< Decoded bytes are not canonical. */
};

/*! \brief Malformed, corrupt, or cross-bound application-receipt bytes. */
class PKGAPPLY_API application_receipt_codec_error final : public std::invalid_argument {
public:
  /*! \brief Construct a codec refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  application_receipt_codec_error(application_receipt_codec_error_code code,
                                  std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~application_receipt_codec_error() override;

  /*!
   * \brief Return the stable refusal category.
  *  \return The stable refusal category.
   */
  [[nodiscard]] application_receipt_codec_error_code code() const noexcept;

private:
  application_receipt_codec_error_code code_;
};

/*! \brief Complete versioned application-receipt byte stream. */
using application_receipt_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated application receipt.
 *  \param value Immutable value to encode.
 *  \return Canonical versioned bytes with integrity framing.
 *  \throws application_receipt_codec_error If bytes exceed the ceiling.
 */
[[nodiscard]] PKGAPPLY_API application_receipt_encoding
encode_application_receipt(const application_receipt& value);

/*! \brief Decode application-receipt bytes for an immutable installation request.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \param request Immutable installation request named by the record.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_receipt_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
decode_application_receipt(
    const std::uint8_t* data,
    std::size_t size,
    const installation_application_request& request);

/*! \brief Decode a complete application-receipt vector for an immutable installation request.
 *  \param encoding Complete candidate encoding.
 *  \param request Immutable installation request named by the record.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_receipt_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
decode_application_receipt(
    const application_receipt_encoding& encoding,
    const installation_application_request& request);

/*! \brief Decode application-receipt bytes for an immutable upgrade request.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \param request Immutable upgrade request named by the record.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_receipt_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
decode_application_receipt(
    const std::uint8_t* data,
    std::size_t size,
    const upgrade_application_request& request);

/*! \brief Decode a complete application-receipt vector for an immutable upgrade request.
 *  \param encoding Complete candidate encoding.
 *  \param request Immutable upgrade request named by the record.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_receipt_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
decode_application_receipt(
    const application_receipt_encoding& encoding,
    const upgrade_application_request& request);

/*! \brief Decode application-receipt bytes for an immutable removal request.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \param request Immutable removal request named by the record.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_receipt_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
decode_application_receipt(
    const std::uint8_t* data,
    std::size_t size,
    const removal_application_request& request);

/*! \brief Decode a complete application-receipt vector for an immutable removal request.
 *  \param encoding Complete candidate encoding.
 *  \param request Immutable removal request named by the record.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_receipt_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
decode_application_receipt(
    const application_receipt_encoding& encoding,
    const removal_application_request& request);

} // namespace pkgapply
