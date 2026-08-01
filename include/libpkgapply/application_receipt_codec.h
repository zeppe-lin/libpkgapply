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

inline constexpr std::uint16_t application_receipt_encoding_version = 1;
inline constexpr std::size_t maximum_application_receipt_encoding_size =
    512U * 1024U * 1024U;

/*! \brief Why durable application-receipt bytes could not be decoded. */
enum class application_receipt_codec_error_code : std::uint8_t {
  invalid_magic = 1,
  unsupported_version = 2,
  truncated = 3,
  invalid_value = 4,
  limit_exceeded = 5,
  trailing_data = 6,
  checksum_mismatch = 7,
  identity_mismatch = 8,
  request_mismatch = 9,
  completed_evidence_invalid = 10,
  noncanonical_encoding = 11,
};

/*! \brief Malformed, corrupt, or request-inconsistent receipt bytes. */
class application_receipt_codec_error final : public std::invalid_argument {
public:
  application_receipt_codec_error(
      application_receipt_codec_error_code code,
      std::string message);

  [[nodiscard]] application_receipt_codec_error_code code() const noexcept;

private:
  application_receipt_codec_error_code code_;
};

using application_receipt_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated terminal application receipt. */
[[nodiscard]] application_receipt_encoding
encode_application_receipt(const application_receipt& receipt);

[[nodiscard]] application_receipt decode_application_receipt(
    const std::uint8_t* data,
    std::size_t size,
    const installation_application_request& request);

[[nodiscard]] application_receipt decode_application_receipt(
    const std::uint8_t* data,
    std::size_t size,
    const upgrade_application_request& request);

[[nodiscard]] application_receipt decode_application_receipt(
    const std::uint8_t* data,
    std::size_t size,
    const removal_application_request& request);

[[nodiscard]] application_receipt decode_application_receipt(
    const application_receipt_encoding& encoding,
    const installation_application_request& request);

[[nodiscard]] application_receipt decode_application_receipt(
    const application_receipt_encoding& encoding,
    const upgrade_application_request& request);

[[nodiscard]] application_receipt decode_application_receipt(
    const application_receipt_encoding& encoding,
    const removal_application_request& request);

} // namespace pkgapply
