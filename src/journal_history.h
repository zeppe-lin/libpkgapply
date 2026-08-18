// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <libpkgapply/journal_transport.h>

namespace pkgapply::detail {

/*! \brief Owner-side projection of one append-only application history.
 *
 * The history validates steps incrementally and retains only the semantic
 * material needed by libpkgapply. Physical stores remain byte persistence
 * mechanisms and never reconstruct event progress themselves.
 */
class application_journal_history final {
public:
  [[nodiscard]] static application_journal_history
  initial(application_journal_declaration declaration);

  /*! \brief Rehydrate one exact committed history from owner-authored storage.
   *
   * Reads are exact-name by declaration identity and sequence. If the bounded
   * cursor lags one already-durable successor, that one successor may be
   * adopted only after complete declaration, sequence, predecessor, event, and
   * lifecycle validation. No enumeration or target observation is performed.
   */
  [[nodiscard]] static application_journal_history
  load(application_journal_store& store,
       const application_journal_declaration_identity& declaration);

  /*! \brief Validate one exact immutable successor without advancing memory.
   *  \return The bounded cursor that would result from committing @p step.
   */
  [[nodiscard]] application_journal_cursor
  validate(const application_journal_step& step) const;

  /*! \brief Admit one exact immutable successor into the semantic history. */
  void append(const application_journal_step& step);

  [[nodiscard]] const application_journal_declaration&
  declaration() const noexcept;
  [[nodiscard]] const application_journal_header& header() const noexcept;
  [[nodiscard]] const std::vector<application_journal_effect>&
  effects() const noexcept;
  [[nodiscard]] const application_journal_effect&
  effect(const application_journal_effect_identity& identity) const;
  [[nodiscard]] const application_journal_cursor& cursor() const noexcept;
  [[nodiscard]] application_journal_state state() const noexcept;
  [[nodiscard]] const std::vector<application_journal_event>&
  events() const noexcept;
  [[nodiscard]] const std::vector<application_journal_step>&
  steps() const noexcept;
  [[nodiscard]] const std::optional<application_receipt_identity>&
  receipt() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  completed_evidence() const noexcept;

  /*! \brief Materialize the legacy snapshot only as a derived in-memory view.
   *
   * This helper exists only while the pre-release engine is migrated away from
   * complete snapshots. It performs no persistence and is not journal truth.
   */
  [[nodiscard]] application_journal_record snapshot() const;

private:
  struct digest_hash final {
    [[nodiscard]] std::size_t operator()(
        const application_journal_effect_identity& identity) const noexcept;
  };

  struct effect_progress final {
    bool intended = false;
    std::optional<application_journal_event_kind> terminal;
  };

  struct step_validation final {
    application_journal_cursor candidate;
    std::optional<std::size_t> ordinal;
    std::size_t completed_success_effects;
  };

  application_journal_history(
      application_journal_declaration declaration,
      application_journal_cursor cursor);

  [[nodiscard]] std::size_t validate_event(
      const application_journal_event& event) const;
  [[nodiscard]] step_validation
  validate_step(const application_journal_step& step) const;
  void validate_resolution(
      const application_journal_cursor& candidate,
      std::size_t completed_success_effects) const;

  application_journal_declaration declaration_;
  application_journal_cursor cursor_;
  std::vector<application_journal_event> events_;
  std::vector<application_journal_step> steps_;
  std::unordered_map<application_journal_effect_identity,
                     std::size_t,
                     digest_hash>
      effect_ordinals_;
  std::vector<effect_progress> progress_;
  std::size_t required_success_effects_ = 0;
  std::size_t completed_success_effects_ = 0;
};

} // namespace pkgapply::detail
