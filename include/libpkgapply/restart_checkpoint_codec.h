// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file restart_checkpoint_codec.h
 *  \brief Canonical durable restart-checkpoint codec.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/restart.h>

namespace pkgapply {

/*! \brief Wire-format version of an application restart checkpoint. */
inline constexpr std::uint16_t application_restart_checkpoint_encoding_version = 1;

/*! \brief Maximum accepted byte length of an application restart checkpoint. */
inline constexpr std::size_t maximum_application_restart_checkpoint_encoding_size =
    256U * 1024U * 1024U;

/*! \brief Stable reason that restart-checkpoint bytes were refused. */
enum class application_restart_checkpoint_codec_error_code : std::uint8_t {
  invalid_magic = 1, /*!< The format magic does not identify this protocol. */
  unsupported_version = 2, /*!< The encoding version is unsupported. */
  truncated = 3, /*!< The byte stream ends before a required field. */
  invalid_value = 4, /*!< A decoded enum, count, or field value is invalid. */
  limit_exceeded = 5, /*!< A declared size or count exceeds its ceiling. */
  trailing_data = 6, /*!< Bytes remain after the complete canonical record. */
  identity_mismatch = 7, /*!< Fields do not reproduce encoded authority. */
  request_mismatch = 8, /*!< The checkpoint names another request or journal. */
};

/*! \brief Malformed, corrupt, or cross-bound restart-checkpoint bytes. */
class application_restart_checkpoint_codec_error final : public std::invalid_argument {
public:
  /*! \brief Construct a codec refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  application_restart_checkpoint_codec_error(application_restart_checkpoint_codec_error_code code,
                                               std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~application_restart_checkpoint_codec_error() override;

  /*!
   * \brief Return the stable refusal category.
  *  \return The stable refusal category.
   */
  [[nodiscard]] application_restart_checkpoint_codec_error_code code() const noexcept;

private:
  application_restart_checkpoint_codec_error_code code_;
};

/*! \brief Complete versioned restart-checkpoint byte stream. */
using application_restart_checkpoint_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated restart checkpoint.
 *  \param value Immutable value to encode.
 *  \return Canonical versioned bytes with integrity framing.
 *  \throws application_restart_checkpoint_codec_error If bytes exceed the ceiling.
 */
[[nodiscard]] application_restart_checkpoint_encoding
encode_application_restart_checkpoint(const application_restart_checkpoint& value);

/*! \brief Decode restart-checkpoint bytes for an immutable installation request.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \param journal Exact journal authority named by the checkpoint.
 *  \param request Immutable installation request named by the checkpoint.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_restart_checkpoint_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const installation_application_request& request);

/*! \brief Decode a complete restart-checkpoint vector for an immutable installation request.
 *  \param encoding Complete candidate encoding.
 *  \param journal Exact journal authority named by the checkpoint.
 *  \param request Immutable installation request named by the checkpoint.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_restart_checkpoint_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const installation_application_request& request);

/*! \brief Decode restart-checkpoint bytes for an immutable upgrade request.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \param journal Exact journal authority named by the checkpoint.
 *  \param request Immutable upgrade request named by the checkpoint.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_restart_checkpoint_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const upgrade_application_request& request);

/*! \brief Decode a complete restart-checkpoint vector for an immutable upgrade request.
 *  \param encoding Complete candidate encoding.
 *  \param journal Exact journal authority named by the checkpoint.
 *  \param request Immutable upgrade request named by the checkpoint.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_restart_checkpoint_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const upgrade_application_request& request);

/*! \brief Decode restart-checkpoint bytes for an immutable removal request.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \param journal Exact journal authority named by the checkpoint.
 *  \param request Immutable removal request named by the checkpoint.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_restart_checkpoint_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const removal_application_request& request);

/*! \brief Decode a complete restart-checkpoint vector for an immutable removal request.
 *  \param encoding Complete candidate encoding.
 *  \param journal Exact journal authority named by the checkpoint.
 *  \param request Immutable removal request named by the checkpoint.
 *  \return Reconstructed invariant-checked semantic value.
 *  \throws application_restart_checkpoint_codec_error For malformed, corrupt, noncanonical,
 *          oversized, identity-mismatching, or request-mismatching bytes.
 */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const removal_application_request& request);

} // namespace pkgapply
