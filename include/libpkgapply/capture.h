// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgapply/execution_control.h>
#include <libpkgplan/install.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply {

/*! \brief Complete canonical old-object capture set before mutation. */
class old_object_capture_plan final {
public:
  explicit old_object_capture_plan(
      std::vector<old_object_capture_request> requests);

  [[nodiscard]] const std::vector<old_object_capture_request>&
  requests() const noexcept;

  [[nodiscard]] const old_object_capture_request*
  find(const pkgplan::package_path& path) const noexcept;

private:
  std::vector<old_object_capture_request> requests_;
};

/*! \brief Derive old-object captures required by installation semantics. */
[[nodiscard]] old_object_capture_plan prepare_old_object_captures(
    const pkgplan::installation_plan& plan,
    const application_execution_control& control);

/*! \brief Derive old-object captures required by upgrade semantics. */
[[nodiscard]] old_object_capture_plan prepare_old_object_captures(
    const pkgplan::upgrade_plan& plan,
    const application_execution_control& control);

/*! \brief Derive old-object captures required by removal semantics. */
[[nodiscard]] old_object_capture_plan prepare_old_object_captures(
    const pkgplan::removal_plan& plan,
    const application_execution_control& control);

} // namespace pkgapply
