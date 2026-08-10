// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file precondition.h
 *  \brief Fresh observation comparison against planning-time preconditions.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstdint>
#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgplan/precondition.h>

namespace pkgapply {

/*! \brief Semantic field whose planning precondition was not satisfied. */
enum class application_precondition_field : std::uint8_t {
  presence = 1, /*!< Object presence or absence. */
  object_kind = 2, /*!< Semantic object class. */
  mode = 3, /*!< Permission and type bits. */
  uid = 4, /*!< Numeric user owner. */
  gid = 5, /*!< Numeric group owner. */
  size = 6, /*!< Regular-file byte size. */
  mtime = 7, /*!< Modification timestamp. */
  regular_content = 8, /*!< Decoded regular-content identity. */
  symlink_target = 9, /*!< Symbolic-link target. */
  device_number = 10, /*!< Special-device major and minor numbers. */
};

/*! \brief Whether a required current fact was unknown or contradicted. */
enum class application_precondition_failure_kind : std::uint8_t {
  unknown = 1, /*!< Backend could not establish the required field. */
  mismatch = 2, /*!< Backend established a different field value. */
};

/*! \brief One deterministic failed filesystem precondition. */
class PKGAPPLY_API application_precondition_failure final {
public:
  /*! \brief Construct one field-level failure.
   *  \param path Exact path whose precondition failed.
   *  \param field Semantic field compared.
   *  \param kind Unknown or mismatching current fact.
   */
  application_precondition_failure(
      pkgplan::package_path path,
      application_precondition_field field,
      application_precondition_failure_kind kind);

  /*!
   * \brief Return the exact failed path.
  *  \return The exact failed path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the semantic field compared.
  *  \return The semantic field compared.
   */
  [[nodiscard]] application_precondition_field field() const noexcept;
  /*!
   * \brief Return unknown or mismatch classification.
  *  \return Unknown or mismatch classification.
   */
  [[nodiscard]] application_precondition_failure_kind kind() const noexcept;

  /*!
   * \brief Order failures by path, field, and failure kind.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const application_precondition_failure& lhs,
                        const application_precondition_failure& rhs) noexcept;

private:
  pkgplan::package_path path_;
  application_precondition_field field_;
  application_precondition_failure_kind kind_;
};

/*! \brief Fresh observations and every planning-precondition failure. */
class PKGAPPLY_API application_precondition_check final {
public:
  /*! \brief Compare fresh backend observations with accepted preconditions.
   *  \param expected Planner-owned operation preconditions.
   *  \param observed Fresh exact observation batch for the same path universe.
   *  \return Immutable observations and canonical failure list.
   *  \throws std::invalid_argument If observed paths do not match the expected
   *          universe or contain invalid object-kind values.
   */
  [[nodiscard]] static application_precondition_check make(
      const pkgplan::operation_preconditions& expected,
      backend_observation_batch observed);

  /*!
   * \brief Return whether no precondition failed.
  *  \return Whether no precondition failed.
   */
  [[nodiscard]] bool satisfied() const noexcept;
  /*!
   * \brief Return the retained fresh observation batch.
  *  \return The retained fresh observation batch.
   */
  [[nodiscard]] const backend_observation_batch&
  observations() const noexcept;
  /*!
   * \brief Return all failures in deterministic order.
  *  \return All failures in deterministic order.
   */
  [[nodiscard]] const std::vector<application_precondition_failure>&
  failures() const noexcept;

private:
  /*! \brief Construct a completed comparison. */
  application_precondition_check(
      backend_observation_batch observations,
      std::vector<application_precondition_failure> failures);

  backend_observation_batch observations_;
  std::vector<application_precondition_failure> failures_;
};

} // namespace pkgapply
