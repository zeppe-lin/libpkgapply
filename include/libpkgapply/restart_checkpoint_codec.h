// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/restart.h>

namespace pkgapply {

inline constexpr std::uint16_t application_restart_checkpoint_encoding_version = 2;
inline constexpr std::size_t maximum_application_restart_checkpoint_encoding_size =
    256U * 1024U * 1024U;

/*! \brief Why durable checkpoint bytes could not be decoded. */
enum class application_restart_checkpoint_codec_error_code : std::uint8_t {
  invalid_magic = 1,
  unsupported_version = 2,
  truncated = 3,
  invalid_value = 4,
  limit_exceeded = 5,
  trailing_data = 6,
  identity_mismatch = 7,
  request_mismatch = 8,
};

/*! \brief Malformed, unsupported, or request-inconsistent checkpoint bytes. */
class application_restart_checkpoint_codec_error final
    : public std::invalid_argument {
public:
  application_restart_checkpoint_codec_error(
      application_restart_checkpoint_codec_error_code code,
      std::string message);

  [[nodiscard]] application_restart_checkpoint_codec_error_code
  code() const noexcept;

private:
  application_restart_checkpoint_codec_error_code code_;
};

using application_restart_checkpoint_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated durable restart checkpoint. */
[[nodiscard]] application_restart_checkpoint_encoding
encode_application_restart_checkpoint(
    const application_restart_checkpoint& checkpoint);

/*! \brief Decode one installation checkpoint against its immutable request. */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const installation_application_request& request);

/*! \brief Decode one upgrade checkpoint against its immutable request. */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const upgrade_application_request& request);

/*! \brief Decode one removal checkpoint against its immutable request. */
[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const removal_application_request& request);

[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const installation_application_request& request);

[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const upgrade_application_request& request);

[[nodiscard]] application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const removal_application_request& request);

} // namespace pkgapply
