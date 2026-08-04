// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file incoming_package.h
 *  \brief Verified native build authority admitted for application.
 */
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <libpkgapply/digest.h>
#include <libpkgbuild/result.h>
#include <libpkgimage/inspection_receipt.h>
#include <libpkgplan/package_fact.h>

namespace pkgapply {

/*! \brief Schema version of incoming_package_authority. */
inline constexpr std::uint16_t incoming_package_authority_schema_version = 1;

/*! \brief Stable reason that incoming build authority was refused. */
enum class incoming_package_error_code : std::uint8_t {
  build_result = 1, /*!< Build evidence is not complete and successful. */
  source_projection = 2, /*!< Sealed source cannot project into planner facts. */
  artifact_binding = 3, /*!< Inspection observed different archive bytes. */
  payload_mismatch = 4, /*!< Inspected image differs from the build manifest. */
  plan_binding = 5, /*!< A later request plan differs from this authority. */
};

/*! \brief Invalid or cross-bound incoming build authority. */
class incoming_package_error final : public std::invalid_argument {
public:
  /*! \brief Construct an incoming-package refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  incoming_package_error(incoming_package_error_code code, std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~incoming_package_error() override;

  /*! \brief Return the stable refusal category. */
  [[nodiscard]] incoming_package_error_code code() const noexcept;

private:
  incoming_package_error_code code_;
};

/*! \brief Successful native build bound to independently inspected bytes.
 *
 *  The value retains exact libpkgbuild authority, normalized libpkgimage
 *  evidence, and the source-to-planner candidate projection derived from the
 *  build request's sealed source snapshot. It carries no archive pathname and
 *  performs no target mutation.
 */
class incoming_package_authority final {
public:
  /*! \brief Admit one successful build and inspected package image.
   *  \param build Complete successful native build result.
   *  \param image Independently inspected exact archive and normalized image.
   *  \return Immutable incoming package authority.
   *  \throws incoming_package_error If build evidence is incomplete, archive
   *          bytes differ, payload metadata differs, or source projection
   *          fails.
   */
  [[nodiscard]] static incoming_package_authority admit(
      pkgbuild::build_result build,
      pkgimage::inspected_package_image image);

  /*! \brief Return the incoming authority schema version. */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  /*! \brief Return the canonical incoming authority identity. */
  [[nodiscard]] const incoming_package_authority_identity&
  identity() const noexcept;

  /*! \brief Return retained complete build authority. */
  [[nodiscard]] const pkgbuild::build_result& build() const noexcept;

  /*! \brief Return retained inspection evidence and normalized image. */
  [[nodiscard]] const pkgimage::inspected_package_image&
  image() const noexcept;

  /*! \brief Return the planner candidate derived from sealed source. */
  [[nodiscard]] const pkgplan::candidate_package_fact&
  candidate() const noexcept;

private:
  /*! \brief Construct admitted authority already identified by admit(). */
  incoming_package_authority(
      incoming_package_authority_identity identity,
      pkgbuild::build_result build,
      pkgimage::inspected_package_image image,
      pkgplan::candidate_package_fact candidate);

  std::uint16_t schema_version_ = incoming_package_authority_schema_version;
  incoming_package_authority_identity identity_;
  pkgbuild::build_result build_;
  pkgimage::inspected_package_image image_;
  pkgplan::candidate_package_fact candidate_;
};

} // namespace pkgapply
