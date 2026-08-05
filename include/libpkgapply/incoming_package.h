// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file incoming_package.h
 *  \brief Planner-ready native package authority admitted for application.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <libpkgapply/digest.h>
#include <libpkgbuild-plan/adapter.h>

namespace pkgapply {

/*! \brief Schema version of incoming_package_authority. */
inline constexpr std::uint16_t incoming_package_authority_schema_version = 1;

/*! \brief Stable reason that incoming package authority was refused. */
enum class incoming_package_error_code : std::uint8_t {
  plan_binding = 1, /*!< An accepted operation plan names other package facts. */
};

/*! \brief Invalid or cross-bound incoming package authority. */
class incoming_package_error final : public std::invalid_argument {
public:
  /*! \brief Construct an incoming-package refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  incoming_package_error(incoming_package_error_code code, std::string message);
  ~incoming_package_error() override;
  [[nodiscard]] incoming_package_error_code code() const noexcept;

private:
  incoming_package_error_code code_;
};

/*! \brief Application-owned projection of one planner-ready built package.
 *
 *  The value retains the exact `libpkgbuild-plan` artifact projection. Build
 *  and image agreement remains owned by `libpkgbuild-image`; source and
 *  artifact planner projection remains owned by `libpkgbuild-plan`.
 */
class incoming_package_authority final {
public:
  /*! \brief Admit one complete planner artifact projection.
   *  \param projection Planner-ready artifact authority.
   *  \return Immutable application input authority.
   */
  [[nodiscard]] static incoming_package_authority admit(
      pkgbuild::plan_adapter::artifact_projection projection);

  incoming_package_authority(const incoming_package_authority&) noexcept;
  incoming_package_authority(incoming_package_authority&&) noexcept;
  incoming_package_authority& operator=(
      const incoming_package_authority&) noexcept;
  incoming_package_authority& operator=(incoming_package_authority&&) noexcept;
  ~incoming_package_authority();

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const incoming_package_authority_identity&
  identity() const noexcept;
  [[nodiscard]] const pkgbuild::plan_adapter::artifact_projection&
  projection() const noexcept;
  [[nodiscard]] const pkgbuild::image_adapter::build_image_authority&
  authority() const noexcept;
  [[nodiscard]] const pkgbuild::build_result& build() const noexcept;
  [[nodiscard]] const pkgimage::inspected_package_image& image() const noexcept;
  [[nodiscard]] const pkgplan::candidate_package_fact& candidate() const noexcept;
  [[nodiscard]] const pkgplan::artifact_package_fact& artifact() const noexcept;

private:
  struct impl;
  explicit incoming_package_authority(std::shared_ptr<const impl> value);
  std::shared_ptr<const impl> impl_;
};

} // namespace pkgapply
