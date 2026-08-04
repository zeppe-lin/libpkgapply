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

inline constexpr std::uint16_t incoming_package_authority_schema_version = 1;

/*! \brief Machine-readable incoming build admission failure. */
enum class incoming_package_error_code : std::uint8_t {
  build_result = 1,
  source_projection = 2,
  artifact_binding = 3,
  payload_mismatch = 4,
  plan_binding = 5,
};

/*! \brief Invalid or cross-bound incoming build authority. */
class incoming_package_error final : public std::invalid_argument {
public:
  incoming_package_error(incoming_package_error_code code, std::string message);
  ~incoming_package_error() override;

  [[nodiscard]] incoming_package_error_code code() const noexcept;

private:
  incoming_package_error_code code_;
};

/*! \brief Successful native build bound to an independently inspected image.
 *
 * The value retains the exact libpkgbuild result, the normalized libpkgimage
 * evidence, and the source-to-planner candidate projection derived from the
 * build request's sealed source snapshot. Construction independently verifies
 * the successful build shape, complete artifact digest, and every ordered
 * payload entry. It does not retain an archive pathname or execute mutations.
 */
class incoming_package_authority final {
public:
  [[nodiscard]] static incoming_package_authority admit(
      pkgbuild::build_result build,
      pkgimage::inspected_package_image image);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const incoming_package_authority_identity&
  identity() const noexcept;
  [[nodiscard]] const pkgbuild::build_result& build() const noexcept;
  [[nodiscard]] const pkgimage::inspected_package_image& image() const noexcept;
  [[nodiscard]] const pkgplan::candidate_package_fact& candidate() const noexcept;

private:
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
