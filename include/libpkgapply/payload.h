// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <vector>

#include <libpkgimage/entry_selection.h>
#include <libpkgimage/package_entry.h>
#include <libpkgimage/package_image.h>
#include <libpkgplan/install.h>
#include <libpkgplan/package_path.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply {

/*! \brief One incoming image entry requiring active or rejected preparation. */
class incoming_payload_requirement final {
public:
  incoming_payload_requirement(
      pkgplan::package_path path,
      pkgimage::entry_id image_entry,
      std::optional<pkgimage::entry_id> regular_payload_entry,
      bool required_for_active,
      bool required_for_rejected);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] pkgimage::entry_id image_entry() const noexcept;
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  regular_payload_entry() const noexcept;
  [[nodiscard]] bool required_for_active() const noexcept;
  [[nodiscard]] bool required_for_rejected() const noexcept;

private:
  pkgplan::package_path path_;
  pkgimage::entry_id image_entry_;
  std::optional<pkgimage::entry_id> regular_payload_entry_;
  bool required_for_active_;
  bool required_for_rejected_;
};

/*! \brief Exact regular payload closure for one install or upgrade plan. */
class incoming_payload_plan final {
public:
  [[nodiscard]] const pkgimage::package_image_identity& image() const noexcept;
  [[nodiscard]] const pkgimage::entry_selection& selection() const noexcept;
  [[nodiscard]] const std::vector<incoming_payload_requirement>&
  requirements() const noexcept;

private:
  friend incoming_payload_plan prepare_incoming_payloads(
      const pkgplan::installation_plan&, const pkgimage::package_image&);
  friend incoming_payload_plan prepare_incoming_payloads(
      const pkgplan::upgrade_plan&, const pkgimage::package_image&);

  incoming_payload_plan(
      pkgimage::package_image_identity image,
      pkgimage::entry_selection selection,
      std::vector<incoming_payload_requirement> requirements);

  pkgimage::package_image_identity image_;
  pkgimage::entry_selection selection_;
  std::vector<incoming_payload_requirement> requirements_;
};

/*! \brief Derive exact regular payloads required by an installation plan. */
[[nodiscard]] incoming_payload_plan prepare_incoming_payloads(
    const pkgplan::installation_plan& plan,
    const pkgimage::package_image& image);

/*! \brief Derive exact regular payloads required by an upgrade plan. */
[[nodiscard]] incoming_payload_plan prepare_incoming_payloads(
    const pkgplan::upgrade_plan& plan,
    const pkgimage::package_image& image);

} // namespace pkgapply
