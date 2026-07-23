// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgplan/precondition.h>

namespace pkgapply {

/*! \brief Semantic field whose planning-time precondition was not satisfied. */
enum class application_precondition_field : std::uint8_t {
  presence = 1,
  object_kind = 2,
  mode = 3,
  uid = 4,
  gid = 5,
  size = 6,
  mtime = 7,
  regular_content = 8,
  symlink_target = 9,
  device_number = 10,
};

/*! \brief Whether a required current fact was unknown or contradicted. */
enum class application_precondition_failure_kind : std::uint8_t {
  unknown = 1,
  mismatch = 2,
};

/*! \brief One deterministic failed planning-time filesystem precondition. */
class application_precondition_failure final {
public:
  application_precondition_failure(
      pkgplan::package_path path,
      application_precondition_field field,
      application_precondition_failure_kind kind);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] application_precondition_field field() const noexcept;
  [[nodiscard]] application_precondition_failure_kind kind() const noexcept;

  friend bool operator<(const application_precondition_failure& lhs,
                        const application_precondition_failure& rhs) noexcept;

private:
  pkgplan::package_path path_;
  application_precondition_field field_;
  application_precondition_failure_kind kind_;
};

/*! \brief Fresh exact observations plus all planning-precondition failures. */
class application_precondition_check final {
public:
  [[nodiscard]] static application_precondition_check make(
      const pkgplan::operation_preconditions& expected,
      backend_observation_batch observed);

  [[nodiscard]] bool satisfied() const noexcept;
  [[nodiscard]] const backend_observation_batch& observations() const noexcept;
  [[nodiscard]] const std::vector<application_precondition_failure>&
  failures() const noexcept;

private:
  application_precondition_check(
      backend_observation_batch observations,
      std::vector<application_precondition_failure> failures);

  backend_observation_batch observations_;
  std::vector<application_precondition_failure> failures_;
};

} // namespace pkgapply
