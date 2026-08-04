// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/result.h>

namespace pkgapply {

inline constexpr std::uint16_t
completed_application_evidence_encoding_version = 1;
inline constexpr std::size_t
maximum_completed_application_evidence_encoding_size =
    256U * 1024U * 1024U;

/*! \brief Why durable completed-evidence bytes could not be decoded. */
enum class completed_application_evidence_codec_error_code : std::uint8_t {
  invalid_magic = 1,
  unsupported_version = 2,
  truncated = 3,
  invalid_value = 4,
  limit_exceeded = 5,
  trailing_data = 6,
  checksum_mismatch = 7,
  identity_mismatch = 8,
  request_mismatch = 9,
};

/*! \brief Malformed, corrupt, or request-inconsistent evidence bytes. */
class completed_application_evidence_codec_error final
    : public std::invalid_argument {
public:
  completed_application_evidence_codec_error(
      completed_application_evidence_codec_error_code code,
      std::string message);

  ~completed_application_evidence_codec_error() override;

  [[nodiscard]] completed_application_evidence_codec_error_code
  code() const noexcept;

private:
  completed_application_evidence_codec_error_code code_;
};

using completed_application_evidence_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated completed application evidence record. */
[[nodiscard]] completed_application_evidence_encoding
encode_completed_application_evidence(
    const completed_application_evidence& evidence);

  [[nodiscard]] completed_application_evidence
decode_completed_application_evidence(
    const std::uint8_t* data,
    std::size_t size,
    const installation_application_request& request);

  [[nodiscard]] completed_application_evidence
decode_completed_application_evidence(
    const std::uint8_t* data,
    std::size_t size,
    const upgrade_application_request& request);

  [[nodiscard]] completed_application_evidence
decode_completed_application_evidence(
    const std::uint8_t* data,
    std::size_t size,
    const removal_application_request& request);

  [[nodiscard]] completed_application_evidence
decode_completed_application_evidence(
    const completed_application_evidence_encoding& encoding,
    const installation_application_request& request);

  [[nodiscard]] completed_application_evidence
decode_completed_application_evidence(
    const completed_application_evidence_encoding& encoding,
    const upgrade_application_request& request);

  [[nodiscard]] completed_application_evidence
decode_completed_application_evidence(
    const completed_application_evidence_encoding& encoding,
    const removal_application_request& request);

} // namespace pkgapply
