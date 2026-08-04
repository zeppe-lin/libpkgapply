// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file journal_codec.h
 *  \brief Durable application-journal encoding and monotonic replacement.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/journal.h>

namespace pkgapply {

/*! \brief Wire-format version of the application journal. */
inline constexpr std::uint16_t application_journal_encoding_version = 1;

/*! \brief Maximum accepted byte length of one journal snapshot. */
inline constexpr std::size_t maximum_application_journal_encoding_size =
    256U * 1024U * 1024U;

/*! \brief Stable reason that durable journal bytes were refused. */
enum class application_journal_codec_error_code : std::uint8_t {
  invalid_magic = 1, /*!< Format magic does not identify this protocol. */
  unsupported_version = 2, /*!< Encoding version is unsupported. */
  truncated = 3, /*!< Byte stream ends before a required field. */
  invalid_value = 4, /*!< A decoded enum, count, or field is invalid. */
  limit_exceeded = 5, /*!< A declared size or count exceeds its ceiling. */
  trailing_data = 6, /*!< Bytes remain after the complete record. */
  identity_mismatch = 7, /*!< Decoded fields do not reproduce identities. */
};

/*! \brief Malformed, unsupported, or self-contradictory journal encoding. */
class application_journal_codec_error final : public std::invalid_argument {
public:
  /*! \brief Construct a journal codec refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  application_journal_codec_error(
      application_journal_codec_error_code code,
      std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~application_journal_codec_error() override;

  /*!
   * \brief Return the stable refusal category.
  *  \return The stable refusal category.
   */
  [[nodiscard]] application_journal_codec_error_code code() const noexcept;

private:
  application_journal_codec_error_code code_;
};

/*! \brief Stable reason that a journal successor was refused. */
enum class application_journal_transition_error_code : std::uint8_t {
  different_journal = 1, /*!< Snapshots belong to different attempts. */
  effect_graph_changed = 2, /*!< Planned effect graph was rewritten. */
  event_history_rewritten = 3, /*!< Durable event prefix was changed. */
  resolution_regressed = 4, /*!< A resolved effect became unresolved. */
  terminal_replaced = 5, /*!< Terminal evidence or receipt was replaced. */
  state_regressed = 6, /*!< Journal lifecycle state moved backward. */
};

/*! \brief Non-monotonic replacement of a durable journal snapshot. */
class application_journal_transition_error final : public std::invalid_argument {
public:
  /*! \brief Construct a journal-transition refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  application_journal_transition_error(
      application_journal_transition_error_code code,
      std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~application_journal_transition_error() override;

  /*!
   * \brief Return the stable refusal category.
  *  \return The stable refusal category.
   */
  [[nodiscard]] application_journal_transition_error_code code() const noexcept;

private:
  application_journal_transition_error_code code_;
};

/*! \brief Complete versioned application-journal byte stream. */
using application_journal_encoding = std::vector<std::uint8_t>;

/*! \brief Encode one validated journal snapshot.
 *  \param record Immutable journal snapshot.
 *  \return Canonical versioned bytes.
 *  \throws application_journal_codec_error If the encoded record exceeds the
 *          protocol ceiling.
 */
[[nodiscard]] application_journal_encoding
encode_application_journal(const application_journal_record& record);

/*! \brief Decode and revalidate one complete journal byte stream.
 *  \param data First byte of the candidate encoding.
 *  \param size Number of available bytes.
 *  \return Reconstructed invariant-checked journal snapshot.
 *  \throws application_journal_codec_error For malformed, unsupported,
 *          oversized, trailing, or identity-inconsistent bytes.
 */
[[nodiscard]] application_journal_record
decode_application_journal(const std::uint8_t* data, std::size_t size);

/*! \brief Decode and revalidate one complete journal vector.
 *  \param encoding Complete candidate encoding.
 *  \return Reconstructed invariant-checked journal snapshot.
 *  \throws application_journal_codec_error For malformed, unsupported,
 *          oversized, trailing, or identity-inconsistent bytes.
 */
[[nodiscard]] application_journal_record
decode_application_journal(const application_journal_encoding& encoding);

/*! \brief Require a snapshot to extend durable history monotonically.
 *  \param previous Previously durable journal snapshot.
 *  \param next Candidate replacement snapshot.
 *  \throws application_journal_transition_error If identity, effect graph,
 *          event prefix, resolution, terminal evidence, or lifecycle state
 *          regresses or is rewritten.
 */
void validate_application_journal_successor(
    const application_journal_record& previous,
    const application_journal_record& next);

} // namespace pkgapply
