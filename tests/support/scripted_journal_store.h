// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <libpkgapply/journal_transport.h>

namespace pkgapply::test {

/*! \brief Deterministic append-only journal store for engine qualification. */
class scripted_journal_store final : public application_journal_store {
public:
  [[nodiscard]] application_journal_declaration publish_declaration(
      const application_journal_declaration& declaration) override;
  [[nodiscard]] application_journal_step publish_step(
      const application_journal_step& step) override;
  [[nodiscard]] application_journal_cursor compare_and_publish_cursor(
      const std::optional<application_journal_cursor_identity>& expected,
      const application_journal_cursor& cursor) override;
  [[nodiscard]] std::optional<application_journal_declaration> load_declaration(
      const application_journal_declaration_identity& identity) override;
  [[nodiscard]] std::optional<application_journal_cursor> load_cursor(
      const application_journal_declaration_identity& declaration) override;
  [[nodiscard]] std::optional<application_journal_step> load_step(
      const application_journal_declaration_identity& declaration,
      std::uint64_t sequence) override;

  [[nodiscard]] std::size_t declaration_publications() const noexcept;
  [[nodiscard]] std::size_t step_publications() const noexcept;
  [[nodiscard]] std::size_t cursor_publications() const noexcept;
  [[nodiscard]] const std::optional<application_journal_declaration_identity>&
  latest_declaration() const noexcept;
  [[nodiscard]] std::optional<application_journal_record> latest_snapshot() const;
  void clear_counts() noexcept;

private:
  struct journal_state final {
    application_journal_declaration declaration;
    std::map<std::uint64_t, application_journal_step> steps;
    std::optional<application_journal_cursor> cursor;
  };

  [[nodiscard]] static std::string key(
      const application_journal_declaration_identity& identity);

  std::map<std::string, journal_state> journals_;
  std::optional<application_journal_declaration_identity> latest_declaration_;
  std::size_t declaration_publications_ = 0;
  std::size_t step_publications_ = 0;
  std::size_t cursor_publications_ = 0;
};

} // namespace pkgapply::test
