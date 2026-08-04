// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file schedule.h
 *  \brief Deterministic mechanism order derived from accepted plan semantics.
 */
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
  capture_old_object = 1, /*!< Capture current object before destructive effect. */
  stage_regular_payload = 2, /*!< Stage exact decoded regular data. */
  publish_rejected_object = 3, /*!< Publish requested rejected-object evidence. */
  publish_active_object = 4, /*!< Apply the planned active namespace effect. */
  observe_result = 5, /*!< Observe the resulting logical path. */
};

/*! \brief One fully ordered mechanism step derived from accepted semantics. */
class application_effect_step final {
public:
  /*! \brief Validate and construct one schedule step.
   *  \param ordinal Zero-based consecutive schedule position.
   *  \param kind Mechanism phase.
   *  \param path Logical path governed by the step.
   *  \param incoming_entry Incoming image entry when applicable.
   *  \throws std::invalid_argument For an unknown kind, an inapplicable entry,
   *          or a payload-staging step without an entry.
   */
  application_effect_step(
      std::uint64_t ordinal,
      application_effect_step_kind kind,
      pkgplan::package_path path,
      std::optional<pkgimage::entry_id> incoming_entry = std::nullopt);

  /*! \brief Return the zero-based schedule position. */
  [[nodiscard]] std::uint64_t ordinal() const noexcept;
  /*! \brief Return the mechanism phase. */
  [[nodiscard]] application_effect_step_kind kind() const noexcept;
  /*! \brief Return the governed logical path. */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*! \brief Return the incoming image entry when applicable. */
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;

private:
  std::uint64_t ordinal_;
  application_effect_step_kind kind_;
  pkgplan::package_path path_;
  std::optional<pkgimage::entry_id> incoming_entry_;
};

/*! \brief Canonical safe order of path-scoped application mechanisms. */
class application_effect_schedule final {
public:
  /*! \brief Construct and validate a complete schedule.
   *  \param steps Mechanism steps in execution order.
   *  \throws std::invalid_argument If ordinals are not zero-based consecutive.
   */
  explicit application_effect_schedule(
      std::vector<application_effect_step> steps);

  /*! \brief Return all steps in canonical execution order. */
  [[nodiscard]] const std::vector<application_effect_step>&
  steps() const noexcept;

private:
  std::vector<application_effect_step> steps_;
};

/*! \brief Derive installation mechanism order from sealed archive truth.
 *  \param plan Accepted installation plan.
 *  \param image Exact admitted normalized package image.
 *  \param payloads Image-bound incoming payload closure.
 *  \param captures Required old-object captures.
 *  \return Complete deterministic effect schedule.
 *  \throws std::invalid_argument For cross-bound image facts, malformed link
 *          dependencies, missing entries, or cyclic effect ordering.
 */
[[nodiscard]] application_effect_schedule prepare_application_schedule(
    const pkgplan::installation_plan& plan,
    const pkgimage::package_image& image,
    const incoming_payload_plan& payloads,
    const old_object_capture_plan& captures);

/*! \brief Derive upgrade mechanism order from sealed archive truth.
 *  \param plan Accepted upgrade plan.
 *  \param image Exact admitted normalized package image.
 *  \param payloads Image-bound incoming payload closure.
 *  \param captures Required old-object captures.
 *  \return Complete deterministic effect schedule.
 *  \throws std::invalid_argument For cross-bound image facts, malformed link
 *          dependencies, missing entries, or cyclic effect ordering.
 */
[[nodiscard]] application_effect_schedule prepare_application_schedule(
    const pkgplan::upgrade_plan& plan,
    const pkgimage::package_image& image,
    const incoming_payload_plan& payloads,
    const old_object_capture_plan& captures);

/*! \brief Derive removal order without incoming archive authority.
 *  \param plan Accepted removal plan.
 *  \param captures Required old-object captures.
 *  \return Complete deterministic effect schedule.
 *  \throws std::invalid_argument For malformed or cyclic effect ordering.
 */
[[nodiscard]] application_effect_schedule prepare_application_schedule(
    const pkgplan::removal_plan& plan,
    const old_object_capture_plan& captures);

} // namespace pkgapply
