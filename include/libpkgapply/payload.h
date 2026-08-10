// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file payload.h
 *  \brief Exact incoming regular-payload closure derived from accepted plans.
 */
#pragma once

#include <libpkgapply/export.h>

#include <optional>
#include <vector>

#include <libpkgimage/entry_selection.h>
#include <libpkgimage/package_entry.h>
#include <libpkgimage/package_image.h>
#include <libpkgplan/install.h>
#include <libpkgplan/package_path.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply {

/*! \brief One incoming entry requiring active or rejected preparation. */
class PKGAPPLY_API incoming_payload_requirement final {
public:
  /*! \brief Validate and construct one payload requirement.
   *  \param path Logical path governed by the plan decision.
   *  \param image_entry Incoming image entry for that path.
   *  \param regular_payload_entry Regular data entry to stage, when any.
   *  \param required_for_active Whether active publication consumes it.
   *  \param required_for_rejected Whether rejected publication consumes it.
   *  \throws std::invalid_argument If neither mechanism consumes the entry.
   */
  incoming_payload_requirement(
      pkgplan::package_path path,
      pkgimage::entry_id image_entry,
      std::optional<pkgimage::entry_id> regular_payload_entry,
      bool required_for_active,
      bool required_for_rejected);

  /*!
   * \brief Return the governed logical path.
  *  \return The governed logical path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the incoming image entry.
  *  \return The incoming image entry.
   */
  [[nodiscard]] pkgimage::entry_id image_entry() const noexcept;
  /*!
   * \brief Return the regular data entry to stage, when any.
  *  \return The regular data entry to stage, when any.
   */
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  regular_payload_entry() const noexcept;
  /*!
   * \brief Return whether active publication consumes this requirement.
  *  \return Whether active publication consumes this requirement.
   */
  [[nodiscard]] bool required_for_active() const noexcept;
  /*!
   * \brief Return whether rejected publication consumes this requirement.
  *  \return Whether rejected publication consumes this requirement.
   */
  [[nodiscard]] bool required_for_rejected() const noexcept;

private:
  pkgplan::package_path path_;
  pkgimage::entry_id image_entry_;
  std::optional<pkgimage::entry_id> regular_payload_entry_;
  bool required_for_active_;
  bool required_for_rejected_;
};

/*! \brief Exact regular payload closure for one install or upgrade plan. */
class PKGAPPLY_API incoming_payload_plan final {
public:
  /*!
   * \brief Return the package image identity governing the closure.
  *  \return The package image identity governing the closure.
   */
  [[nodiscard]] const pkgimage::package_image_identity& image() const noexcept;
  /*!
   * \brief Return the deduplicated regular entry selection.
  *  \return The deduplicated regular entry selection.
   */
  [[nodiscard]] const pkgimage::entry_selection& selection() const noexcept;
  /*!
   * \brief Return path requirements in accepted plan order.
  *  \return Path requirements in accepted plan order.
   */
  [[nodiscard]] const std::vector<incoming_payload_requirement>&
  requirements() const noexcept;

private:
  /*! \brief Allow installation preparation to construct the closed value. */
  friend incoming_payload_plan prepare_incoming_payloads(
      const pkgplan::installation_plan&, const pkgimage::package_image&);
  /*! \brief Allow upgrade preparation to construct the closed value. */
  friend incoming_payload_plan prepare_incoming_payloads(
      const pkgplan::upgrade_plan&, const pkgimage::package_image&);

  /*! \brief Construct a validated image-bound payload closure. */
  incoming_payload_plan(
      pkgimage::package_image_identity image,
      pkgimage::entry_selection selection,
      std::vector<incoming_payload_requirement> requirements);

  pkgimage::package_image_identity image_;
  pkgimage::entry_selection selection_;
  std::vector<incoming_payload_requirement> requirements_;
};

/*! \brief Derive payloads required by accepted installation semantics.
 *  \param plan Accepted installation plan.
 *  \param image Exact admitted normalized package image.
 *  \return Image-bound, deduplicated regular-payload closure.
 *  \throws std::invalid_argument If a planned entry is absent, cross-bound,
 *          malformed, or has an invalid hard-link payload anchor.
 */
[[nodiscard]] PKGAPPLY_API incoming_payload_plan prepare_incoming_payloads(
    const pkgplan::installation_plan& plan,
    const pkgimage::package_image& image);

/*! \brief Derive payloads required by accepted upgrade semantics.
 *  \param plan Accepted upgrade plan.
 *  \param image Exact admitted normalized package image.
 *  \return Image-bound, deduplicated regular-payload closure.
 *  \throws std::invalid_argument If a planned entry is absent, cross-bound,
 *          malformed, or has an invalid hard-link payload anchor.
 */
[[nodiscard]] PKGAPPLY_API incoming_payload_plan prepare_incoming_payloads(
    const pkgplan::upgrade_plan& plan,
    const pkgimage::package_image& image);

} // namespace pkgapply
