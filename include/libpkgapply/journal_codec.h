// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/journal.h>

namespace pkgapply {

inline constexpr std::uint16_t application_journal_encoding_version = 1;
inline constexpr std::size_t maximum_application_journal_encoding_size =
    256U * 1024U * 1024U;

/*! \brief Why a durable journal byte stream could not be decoded. */
enum class application_journal_codec_error_code : std::uint8_t {
  invalid_magic = 1,
  unsupported_version = 2,
  truncated = 3,
  invalid_value = 4,
  limit_exceeded = 5,
  trailing_data = 6,
  identity_mismatch = 7,
};

/*! \brief Malformed, unsupported, or self-contradictory journal encoding. */
class application_journal_codec_error final : public std::invalid_argument {
public:
  application_journal_codec_error(
      application_journal_codec_error_code code,
      std::string message);

  [[nodiscard]] application_journal_codec_error_code code() const noexcept;

private:
  application_journal_codec_error_code code_;
};

/*! \brief Why one journal snapshot cannot replace another. */
enum class application_journal_transition_error_code : std::uint8_t {
  different_journal = 1,
  effect_graph_changed = 2,
  event_history_rewritten = 3,
  resolution_regressed = 4,
  terminal_replaced = 5,
  state_regressed = 6,
};

/*! \brief Non-monotonic replacement of a durable journal snapshot. */
class application_journal_transition_error final : public std::invalid_argument {
public:
  application_journal_transition_error(
      application_journal_transition_error_code code,
      std::string message);

  [[nodiscard]] application_journal_transition_error_code code() const noexcept;

private:
  application_journal_transition_error_code code_;
};

using application_journal_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated journal snapshot in the versioned wire format. */
[[nodiscard]] application_journal_encoding
encode_application_journal(const application_journal_record& record);

/*! \brief Decode and revalidate one complete versioned journal byte stream. */
[[nodiscard]] application_journal_record
decode_application_journal(const std::uint8_t* data, std::size_t size);

/*! \brief Decode and revalidate one complete versioned journal byte stream. */
[[nodiscard]] application_journal_record
decode_application_journal(const application_journal_encoding& encoding);

/*! \brief Require `next` to preserve and extend the durable history of `previous`. */
void validate_application_journal_successor(
    const application_journal_record& previous,
    const application_journal_record& next);

} // namespace pkgapply
