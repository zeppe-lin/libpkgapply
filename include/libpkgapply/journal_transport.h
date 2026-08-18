// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file journal_transport.h
 *  \brief Append-only application journal persistence authority.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <libpkgapply/digest.h>
#include <libpkgapply/journal.h>

namespace pkgapply {

/*! \brief Schema version of application_journal_declaration. */
inline constexpr std::uint16_t application_journal_declaration_schema_version = 1;
/*! \brief Schema version of application_journal_step. */
inline constexpr std::uint16_t application_journal_step_schema_version = 1;
/*! \brief Schema version of application_journal_cursor. */
inline constexpr std::uint16_t application_journal_cursor_schema_version = 1;

/*! \brief Opaque libpkgapply-owned replay bytes retained by journal storage. */
using application_journal_replay_encoding = std::vector<std::byte>;

/*! \brief Immutable once-per-attempt declaration of fixed journal authority.
 *
 * The declaration is the only durable record that carries the complete effect
 * graph. `replay_seed` is owned and interpreted exclusively by libpkgapply;
 * mechanism stores must retain it byte-for-byte without acquiring semantic
 * replay vocabulary.
 */
class PKGAPPLY_API application_journal_declaration final {
public:
  [[nodiscard]] static application_journal_declaration make(
      application_journal_header header,
      std::vector<application_journal_effect> effects,
      application_journal_replay_encoding replay_seed = {});

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_journal_declaration_identity&
  identity() const noexcept;
  [[nodiscard]] const application_journal_header& header() const noexcept;
  [[nodiscard]] const std::vector<application_journal_effect>&
  effects() const noexcept;
  [[nodiscard]] const application_journal_replay_encoding&
  replay_seed() const noexcept;

private:
  application_journal_declaration(
      application_journal_declaration_identity identity,
      application_journal_header header,
      std::vector<application_journal_effect> effects,
      application_journal_replay_encoding replay_seed);

  std::uint16_t schema_version_ = application_journal_declaration_schema_version;
  application_journal_declaration_identity identity_;
  application_journal_header header_;
  std::vector<application_journal_effect> effects_;
  application_journal_replay_encoding replay_seed_;
};

/*! \brief One immutable append-only transition on a declared application.
 *
 * A step carries only the newly-created historical fact. It never republishes
 * the declaration, older steps, or a reconstructed restart snapshot.
 */
class PKGAPPLY_API application_journal_step final {
public:
  [[nodiscard]] static application_journal_step make(
      application_journal_declaration_identity declaration,
      std::uint64_t sequence,
      std::optional<application_journal_step_identity> predecessor,
      application_journal_state state,
      std::optional<application_journal_event> event = std::nullopt,
      application_journal_replay_encoding replay_fact = {},
      std::optional<application_receipt_identity> receipt = std::nullopt,
      std::optional<completed_application_evidence_identity>
          completed_evidence = std::nullopt);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_journal_step_identity& identity() const noexcept;
  [[nodiscard]] const application_journal_declaration_identity&
  declaration() const noexcept;
  [[nodiscard]] std::uint64_t sequence() const noexcept;
  [[nodiscard]] const std::optional<application_journal_step_identity>&
  predecessor() const noexcept;
  [[nodiscard]] application_journal_state state() const noexcept;
  [[nodiscard]] const std::optional<application_journal_event>& event() const noexcept;
  [[nodiscard]] const application_journal_replay_encoding& replay_fact() const noexcept;
  [[nodiscard]] const std::optional<application_receipt_identity>& receipt() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  completed_evidence() const noexcept;

private:
  application_journal_step(
      application_journal_step_identity identity,
      application_journal_declaration_identity declaration,
      std::uint64_t sequence,
      std::optional<application_journal_step_identity> predecessor,
      application_journal_state state,
      std::optional<application_journal_event> event,
      application_journal_replay_encoding replay_fact,
      std::optional<application_receipt_identity> receipt,
      std::optional<completed_application_evidence_identity> completed_evidence);

  std::uint16_t schema_version_ = application_journal_step_schema_version;
  application_journal_step_identity identity_;
  application_journal_declaration_identity declaration_;
  std::uint64_t sequence_;
  std::optional<application_journal_step_identity> predecessor_;
  application_journal_state state_;
  std::optional<application_journal_event> event_;
  application_journal_replay_encoding replay_fact_;
  std::optional<application_receipt_identity> receipt_;
  std::optional<completed_application_evidence_identity> completed_evidence_;
};

/*! \brief Bounded mutable locator for the latest admitted journal step.
 *
 * Controllers may retain this value. Its size is independent of package path
 * count and historical step count.
 */
class PKGAPPLY_API application_journal_cursor final {
public:
  [[nodiscard]] static application_journal_cursor initial(
      const application_journal_declaration& declaration,
      application_journal_state state = application_journal_state::preparing);

  [[nodiscard]] static application_journal_cursor advance(
      const application_journal_cursor& current,
      const application_journal_step& step);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_journal_cursor_identity& identity() const noexcept;
  [[nodiscard]] const application_journal_declaration_identity&
  declaration() const noexcept;
  [[nodiscard]] std::uint64_t step_count() const noexcept;
  [[nodiscard]] const std::optional<application_journal_step_identity>&
  latest_step() const noexcept;
  [[nodiscard]] application_journal_state state() const noexcept;
  [[nodiscard]] const std::optional<application_receipt_identity>& receipt() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  completed_evidence() const noexcept;

private:
  application_journal_cursor(
      application_journal_cursor_identity identity,
      application_journal_declaration_identity declaration,
      std::uint64_t step_count,
      std::optional<application_journal_step_identity> latest_step,
      application_journal_state state,
      std::optional<application_receipt_identity> receipt,
      std::optional<completed_application_evidence_identity> completed_evidence);

  std::uint16_t schema_version_ = application_journal_cursor_schema_version;
  application_journal_cursor_identity identity_;
  application_journal_declaration_identity declaration_;
  std::uint64_t step_count_;
  std::optional<application_journal_step_identity> latest_step_;
  application_journal_state state_;
  std::optional<application_receipt_identity> receipt_;
  std::optional<completed_application_evidence_identity> completed_evidence_;
};

/*! \brief Physical persistence interface for one append-only journal spine.
 *
 * Successful publication must mean the supplied bytes are durably retained.
 * Implementations must be immutable for declarations/steps, compare-and-swap
 * cursors exactly, and load steps by exact sequence. They must never discover
 * semantic history by scanning a directory or inspecting the managed target.
 */
class PKGAPPLY_API application_journal_store {
public:
  application_journal_store() = default;
  application_journal_store(const application_journal_store&) = delete;
  application_journal_store& operator=(const application_journal_store&) = delete;
  application_journal_store(application_journal_store&&) = delete;
  application_journal_store& operator=(application_journal_store&&) = delete;
  virtual ~application_journal_store();

  [[nodiscard]] virtual application_journal_declaration publish_declaration(
      const application_journal_declaration& declaration) = 0;

  [[nodiscard]] virtual application_journal_step publish_step(
      const application_journal_step& step) = 0;

  [[nodiscard]] virtual application_journal_cursor compare_and_publish_cursor(
      const std::optional<application_journal_cursor_identity>& expected,
      const application_journal_cursor& cursor) = 0;

  [[nodiscard]] virtual std::optional<application_journal_declaration>
  load_declaration(const application_journal_declaration_identity& identity) = 0;

  [[nodiscard]] virtual std::optional<application_journal_cursor>
  load_cursor(const application_journal_declaration_identity& declaration) = 0;

  [[nodiscard]] virtual std::optional<application_journal_step>
  load_step(const application_journal_declaration_identity& declaration,
            std::uint64_t sequence) = 0;
};

} // namespace pkgapply
