// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <libpkgapply/capture.h>
#include <libpkgapply/payload.h>
#include <libpkgimage/package_image.h>
#include <libpkgplan/install.h>
#include <libpkgplan/package_path.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply {

/*! \brief Phase in one deterministic package-application schedule. */
enum class application_effect_step_kind : std::uint8_t {
  capture_old_object = 1,
  stage_regular_payload = 2,
  publish_rejected_object = 3,
  publish_active_object = 4,
  observe_result = 5,
};

/*! \brief One fully ordered mechanism step derived from accepted semantics. */
class application_effect_step final {
public:
  application_effect_step(
      std::uint64_t ordinal,
      application_effect_step_kind kind,
      pkgplan::package_path path,
      std::optional<pkgimage::entry_id> incoming_entry = std::nullopt);

  [[nodiscard]] std::uint64_t ordinal() const noexcept;
  [[nodiscard]] application_effect_step_kind kind() const noexcept;
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;

private:
  std::uint64_t ordinal_;
  application_effect_step_kind kind_;
  pkgplan::package_path path_;
  std::optional<pkgimage::entry_id> incoming_entry_;
};

/*! \brief Canonical safe order of all path-scoped application mechanisms. */
class application_effect_schedule final {
public:
  explicit application_effect_schedule(
      std::vector<application_effect_step> steps);

  [[nodiscard]] const std::vector<application_effect_step>&
  steps() const noexcept;

private:
  std::vector<application_effect_step> steps_;
};

/*! \brief Derive installation mechanism order from sealed archive truth. */
[[nodiscard]] application_effect_schedule prepare_application_schedule(
    const pkgplan::installation_plan& plan,
    const pkgimage::package_image& image,
    const incoming_payload_plan& payloads,
    const old_object_capture_plan& captures);

/*! \brief Derive upgrade mechanism order from sealed archive truth. */
[[nodiscard]] application_effect_schedule prepare_application_schedule(
    const pkgplan::upgrade_plan& plan,
    const pkgimage::package_image& image,
    const incoming_payload_plan& payloads,
    const old_object_capture_plan& captures);

/*! \brief Derive removal order without incoming archive authority. */
[[nodiscard]] application_effect_schedule prepare_application_schedule(
    const pkgplan::removal_plan& plan,
    const old_object_capture_plan& captures);

} // namespace pkgapply
