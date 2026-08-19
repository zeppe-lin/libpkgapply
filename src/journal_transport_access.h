// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

#include <libpkgapply/journal_transport.h>

namespace pkgapply::detail {

class application_journal_cursor_codec_access final {
public:
  [[nodiscard]] static application_journal_cursor restore(
      application_journal_declaration_identity declaration,
      std::uint64_t step_count,
      std::optional<application_journal_step_identity> latest_step,
      application_journal_state state,
      std::optional<application_receipt_identity> receipt,
      std::optional<completed_application_evidence_identity> completed_evidence);
};

} // namespace pkgapply::detail
