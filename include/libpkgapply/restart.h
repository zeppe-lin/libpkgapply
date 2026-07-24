// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include <libpkgapply/journal.h>

namespace pkgapply {

/*! \brief Required controller action for one validated durable journal. */
enum class application_restart_disposition : std::uint8_t {
  resume_forward = 1,
  resume_recovery = 2,
  terminal = 3,
  external_resolution_required = 4,
};

/*! \brief Pure assessment of one durable application journal snapshot. */
class application_restart_assessment final {
public:
  application_restart_assessment(
      application_journal_record_identity journal,
      application_journal_state state,
      application_restart_disposition disposition);

  [[nodiscard]] const application_journal_record_identity&
  journal() const noexcept;
  [[nodiscard]] application_journal_state state() const noexcept;
  [[nodiscard]] application_restart_disposition disposition() const noexcept;
  [[nodiscard]] bool resumable() const noexcept;

private:
  application_journal_record_identity journal_;
  application_journal_state state_;
  application_restart_disposition disposition_;
};

/*! \brief Classify restart handling without touching a backend or target. */
[[nodiscard]] application_restart_assessment
assess_application_restart(const application_journal_record& journal);

} // namespace pkgapply
