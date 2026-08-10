// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file capture.h
 *  \brief Old-object capture requirements derived from recovery semantics.
 */
#pragma once

#include <libpkgapply/export.h>

#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgapply/execution_control.h>
#include <libpkgplan/install.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply {

/*! \brief Complete canonical old-object capture set before mutation. */
class PKGAPPLY_API old_object_capture_plan final {
public:
  /*! \brief Normalize and construct a capture plan.
   *  \param requests Path-scoped capture requests.
   *  \throws std::invalid_argument If a path occurs more than once.
   */
  explicit old_object_capture_plan(
      std::vector<old_object_capture_request> requests);

  /*!
   * \brief Return requests in canonical path order.
  *  \return Requests in canonical path order.
   */
  [[nodiscard]] const std::vector<old_object_capture_request>&
  requests() const noexcept;

  /*! \brief Find the request for one path.
   *  \param path Path to find.
   *  \return Pointer valid for this plan's lifetime, or `nullptr`.
   */
  [[nodiscard]] const old_object_capture_request*
  find(const pkgplan::package_path& path) const noexcept;

private:
  std::vector<old_object_capture_request> requests_;
};

/*! \brief Derive captures required by installation semantics.
 *  \param plan Accepted installation plan.
 *  \param control Required recovery guarantee.
 *  \return Canonical capture plan.
 *  \throws std::invalid_argument If a decision lacks its precondition or asks
 *          to stage an old object that planning observed as absent.
 */
[[nodiscard]] PKGAPPLY_API old_object_capture_plan prepare_old_object_captures(
    const pkgplan::installation_plan& plan,
    const application_execution_control& control);

/*! \brief Derive captures required by upgrade semantics.
 *  \param plan Accepted upgrade plan.
 *  \param control Required recovery guarantee.
 *  \return Canonical capture plan.
 *  \throws std::invalid_argument If a decision lacks its precondition or asks
 *          to stage an old object that planning observed as absent.
 */
[[nodiscard]] PKGAPPLY_API old_object_capture_plan prepare_old_object_captures(
    const pkgplan::upgrade_plan& plan,
    const application_execution_control& control);

/*! \brief Derive captures required by removal semantics.
 *  \param plan Accepted removal plan.
 *  \param control Required recovery guarantee.
 *  \return Canonical capture plan.
 *  \throws std::invalid_argument If a decision lacks its precondition or asks
 *          to stage an old object that planning observed as absent.
 */
[[nodiscard]] PKGAPPLY_API old_object_capture_plan prepare_old_object_captures(
    const pkgplan::removal_plan& plan,
    const application_execution_control& control);

} // namespace pkgapply
