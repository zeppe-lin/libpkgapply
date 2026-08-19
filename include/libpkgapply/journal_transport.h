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

namespace detail {
class application_journal_cursor_codec_access;
}

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
  /*! \brief Validate, identify, and construct fixed journal authority.
   *  \param header Immutable application-journal header.
   *  \param effects Complete deterministic effect graph in ordinal order.
   *  \param replay_seed Opaque owner-authored replay seed retained byte-for-byte.
   *  \return Immutable identified journal declaration.
   *  \throws std::invalid_argument If effect ordinals are not consecutive or an
   *          adjacent effect identity is repeated.
   */
  [[nodiscard]] static application_journal_declaration make(
      application_journal_header header,
      std::vector<application_journal_effect> effects,
      application_journal_replay_encoding replay_seed = {});

  /*! \brief Return the declaration schema version.
   *  \return The declaration schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*! \brief Return the canonical declaration identity.
   *  \return The canonical declaration identity.
   */
  [[nodiscard]] const application_journal_declaration_identity&
  identity() const noexcept;
  /*! \brief Return the immutable journal header.
   *  \return The immutable journal header.
   */
  [[nodiscard]] const application_journal_header& header() const noexcept;
  /*! \brief Return the complete declared effect graph.
   *  \return The complete effect graph in ordinal order.
   */
  [[nodiscard]] const std::vector<application_journal_effect>&
  effects() const noexcept;
  /*! \brief Return the opaque owner-authored replay seed.
   *  \return The replay seed retained byte-for-byte.
   */
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
  /*! \brief Validate, identify, and construct one append-only journal step.
   *  \param declaration Identity of the immutable journal declaration.
   *  \param sequence Zero-based step sequence.
   *  \param predecessor Exact predecessor identity; absent only at sequence zero.
   *  \param state Resulting application lifecycle state.
   *  \param event Optional effect-scoped journal event.
   *  \param replay_fact Opaque owner-authored replay fact for this transition.
   *  \param receipt Optional terminal receipt authority established or retained.
   *  \param completed_evidence Optional completed-evidence authority established
   *          or retained.
   *  \return Immutable identified journal step.
   *  \throws std::invalid_argument If sequence and predecessor disagree, the
   *          state is invalid, or completed evidence lacks receipt authority.
   */
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

  /*! \brief Return the step schema version.
   *  \return The step schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*! \brief Return the canonical step identity.
   *  \return The canonical step identity.
   */
  [[nodiscard]] const application_journal_step_identity& identity() const noexcept;
  /*! \brief Return the bound declaration identity.
   *  \return The declaration identity.
   */
  [[nodiscard]] const application_journal_declaration_identity&
  declaration() const noexcept;
  /*! \brief Return the zero-based step sequence.
   *  \return The zero-based step sequence.
   */
  [[nodiscard]] std::uint64_t sequence() const noexcept;
  /*! \brief Return the exact predecessor identity.
   *  \return The predecessor identity, absent only for sequence zero.
   */
  [[nodiscard]] const std::optional<application_journal_step_identity>&
  predecessor() const noexcept;
  /*! \brief Return the resulting application lifecycle state.
   *  \return The resulting application lifecycle state.
   */
  [[nodiscard]] application_journal_state state() const noexcept;
  /*! \brief Return the optional effect-scoped journal event.
   *  \return The journal event when this transition is effect-scoped.
   */
  [[nodiscard]] const std::optional<application_journal_event>& event() const noexcept;
  /*! \brief Return the opaque replay fact recorded by this step.
   *  \return The replay fact retained byte-for-byte.
   */
  [[nodiscard]] const application_journal_replay_encoding& replay_fact() const noexcept;
  /*! \brief Return terminal receipt authority established or retained by the step.
   *  \return The terminal receipt identity when present.
   */
  [[nodiscard]] const std::optional<application_receipt_identity>& receipt() const noexcept;
  /*! \brief Return completed-evidence authority established or retained by the step.
   *  \return The completed-application-evidence identity when present.
   */
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
  /*! \brief Construct the empty cursor for a journal declaration.
   *  \param declaration Immutable declaration whose history is being located.
   *  \param state Initial lifecycle state; only preparing is admitted.
   *  \return Empty identified cursor in preparing state.
   *  \throws std::invalid_argument If @p state is not preparing.
   */
  [[nodiscard]] static application_journal_cursor initial(
      const application_journal_declaration& declaration,
      application_journal_state state = application_journal_state::preparing);

  /*! \brief Advance a cursor by its exact next immutable step.
   *  \param current Current admitted journal head.
   *  \param step Candidate exact successor step.
   *  \return New cursor bound to @p step.
   *  \throws std::invalid_argument If declaration, sequence, predecessor,
   *          lifecycle transition, or retained terminal authority disagrees
   *          with @p current.
   */
  [[nodiscard]] static application_journal_cursor advance(
      const application_journal_cursor& current,
      const application_journal_step& step);

  /*! \brief Return the cursor schema version.
   *  \return The cursor schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*! \brief Return the canonical cursor identity.
   *  \return The canonical cursor identity.
   */
  [[nodiscard]] const application_journal_cursor_identity& identity() const noexcept;
  /*! \brief Return the bound declaration identity.
   *  \return The declaration identity.
   */
  [[nodiscard]] const application_journal_declaration_identity&
  declaration() const noexcept;
  /*! \brief Return the number of admitted immutable steps.
   *  \return The number of admitted steps.
   */
  [[nodiscard]] std::uint64_t step_count() const noexcept;
  /*! \brief Return the latest admitted step identity.
   *  \return The latest step identity, absent for an empty cursor.
   */
  [[nodiscard]] const std::optional<application_journal_step_identity>&
  latest_step() const noexcept;
  /*! \brief Return the current application lifecycle state.
   *  \return The current application lifecycle state.
   */
  [[nodiscard]] application_journal_state state() const noexcept;
  /*! \brief Return retained terminal receipt authority.
   *  \return The terminal receipt identity when present.
   */
  [[nodiscard]] const std::optional<application_receipt_identity>& receipt() const noexcept;
  /*! \brief Return retained completed-evidence authority.
   *  \return The completed-application-evidence identity when present.
   */
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  completed_evidence() const noexcept;

private:
  friend class detail::application_journal_cursor_codec_access;

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
  /*! \brief Construct a journal-store mechanism interface. */
  application_journal_store() = default;
  /*! \brief Journal stores are not copy-constructible. */
  application_journal_store(const application_journal_store&) = delete;
  /*! \brief Journal stores are not copy-assignable. */
  application_journal_store& operator=(const application_journal_store&) = delete;
  /*! \brief Journal stores are not move-constructible. */
  application_journal_store(application_journal_store&&) = delete;
  /*! \brief Journal stores are not move-assignable. */
  application_journal_store& operator=(application_journal_store&&) = delete;
  /*! \brief Destroy the journal-store mechanism interface. */
  virtual ~application_journal_store();

  /*! \brief Durably publish one immutable journal declaration.
   *  \param declaration Owner-authored declaration to publish exactly.
   *  \return The exact retained declaration.
   */
  [[nodiscard]] virtual application_journal_declaration publish_declaration(
      const application_journal_declaration& declaration) = 0;

  /*! \brief Durably publish one immutable journal step.
   *  \param step Owner-authored step to publish exactly.
   *  \return The exact retained step.
   */
  [[nodiscard]] virtual application_journal_step publish_step(
      const application_journal_step& step) = 0;

  /*! \brief Compare and durably publish the bounded journal cursor.
   *  \param expected Expected current cursor identity, absent for initial publish.
   *  \param cursor New owner-authored cursor to publish exactly.
   *  \return The exact retained cursor.
   */
  [[nodiscard]] virtual application_journal_cursor compare_and_publish_cursor(
      const std::optional<application_journal_cursor_identity>& expected,
      const application_journal_cursor& cursor) = 0;

  /*! \brief Load one declaration by exact canonical identity.
   *  \param identity Declaration identity to locate directly.
   *  \return The retained declaration, or empty when absent.
   */
  [[nodiscard]] virtual std::optional<application_journal_declaration>
  load_declaration(const application_journal_declaration_identity& identity) = 0;

  /*! \brief Load the bounded cursor for one declaration by exact identity.
   *  \param declaration Declaration identity whose cursor is requested.
   *  \return The retained cursor, or empty when absent.
   */
  [[nodiscard]] virtual std::optional<application_journal_cursor>
  load_cursor(const application_journal_declaration_identity& declaration) = 0;

  /*! \brief Load one immutable step by declaration and exact sequence.
   *  \param declaration Declaration identity whose step is requested.
   *  \param sequence Exact zero-based step sequence.
   *  \return The retained step, or empty when absent.
   */
  [[nodiscard]] virtual std::optional<application_journal_step>
  load_step(const application_journal_declaration_identity& declaration,
            std::uint64_t sequence) = 0;
};

} // namespace pkgapply
