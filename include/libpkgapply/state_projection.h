// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file state_projection.h
 *  \brief Installed-state facts observed under a caller-held mutation lease.
 */
#pragma once

#include <cstdint>
#include <vector>

#include <libpkgapply/digest.h>
#include <libpkgplan/digest.h>
#include <libpkgplan/package_path.h>

namespace pkgapply {

/*! \brief Schema version of lease_bound_state_projection. */
inline constexpr std::uint16_t lease_bound_state_projection_schema_version = 1;

/*! \brief Completeness of the caller-supplied state projection universe. */
enum class state_projection_completeness : std::uint8_t {
  complete = 1, /*!< Every plan-operated path and current owner is present. */
  incomplete = 2, /*!< The projection is diagnostic and cannot admit mutation. */
};

/*! \brief Exact current installed owners for one operated path. */
class projected_path_owners final {
public:
  /*! \brief Construct one canonical path-to-owners fact.
   *  \param path Exact package path.
   *  \param owners Installed package identities, normalized canonically.
   *  \throws std::invalid_argument If owners are duplicated or not canonical.
   */
  projected_path_owners(
      pkgplan::package_path path,
      std::vector<pkgplan::installed_package_identity> owners);

  /*!
   * \brief Return the exact operated path.
  *  \return The exact operated path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;

  /*!
   * \brief Return current owners in canonical order.
  *  \return Current owners in canonical order.
   */
  [[nodiscard]] const std::vector<pkgplan::installed_package_identity>&
  owners() const noexcept;

  /*!
   * \brief Compare complete path-owner facts for equality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs are equal.
   */
  friend bool operator==(const projected_path_owners& lhs,
                         const projected_path_owners& rhs) noexcept;

  /*!
   * \brief Compare complete path-owner facts for inequality.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs and @p rhs differ.
   */
  friend bool operator!=(const projected_path_owners& lhs,
                         const projected_path_owners& rhs) noexcept;

  /*!
   * \brief Order facts by path and then canonical owner sequence.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend bool operator<(const projected_path_owners& lhs,
                        const projected_path_owners& rhs) noexcept;

private:
  pkgplan::package_path path_;
  std::vector<pkgplan::installed_package_identity> owners_;
};

/*! \brief Immutable installed-state facts established under one target lease.
 *
 *  This projection is an application admission input, not durable state
 *  authority. It binds planner-owned snapshot and ownership identities, the
 *  exact operated path universe, observation evidence, and completeness to
 *  one physical mutation-lease acquisition.
 */
class lease_bound_state_projection final {
public:
  /*! \brief Normalize, identify, and construct one state projection.
   *  \param lease Physical mutation-lease acquisition identity.
   *  \param snapshot Planner-owned installed snapshot identity.
   *  \param ownership_inventory Planner-owned ownership inventory identity.
   *  \param completeness Whether the path universe is complete.
   *  \param paths Exact projected path-owner facts.
   *  \param evidence Identity of the observation evidence.
   *  \return Immutable lease-bound projection.
   *  \throws std::invalid_argument If paths are duplicated or not canonical.
   */
  [[nodiscard]] static lease_bound_state_projection
  make(mutation_lease_instance_identity lease,
       pkgplan::installed_state_snapshot_identity snapshot,
       pkgplan::ownership_inventory_identity ownership_inventory,
       state_projection_completeness completeness,
       std::vector<projected_path_owners> paths,
       state_projection_evidence_identity evidence);

  /*!
   * \brief Return the projection schema version.
  *  \return The projection schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;

  /*!
   * \brief Return the canonical projection identity.
  *  \return The canonical projection identity.
   */
  [[nodiscard]] const lease_bound_state_projection_identity&
  identity() const noexcept;

  /*!
   * \brief Return the lease acquisition under which state was observed.
  *  \return The lease acquisition under which state was observed.
   */
  [[nodiscard]] const mutation_lease_instance_identity& lease() const noexcept;

  /*!
   * \brief Return the installed snapshot identity.
  *  \return The installed snapshot identity.
   */
  [[nodiscard]] const pkgplan::installed_state_snapshot_identity&
  snapshot() const noexcept;

  /*!
   * \brief Return the ownership inventory identity.
  *  \return The ownership inventory identity.
   */
  [[nodiscard]] const pkgplan::ownership_inventory_identity&
  ownership_inventory() const noexcept;

  /*!
   * \brief Return whether the projected universe is complete.
  *  \return Whether the projected universe is complete.
   */
  [[nodiscard]] state_projection_completeness completeness() const noexcept;

  /*!
   * \brief Return path-owner facts in canonical path order.
  *  \return Path-owner facts in canonical path order.
   */
  [[nodiscard]] const std::vector<projected_path_owners>&
  paths() const noexcept;

  /*!
   * \brief Return the observation-evidence identity.
  *  \return The observation-evidence identity.
   */
  [[nodiscard]] const state_projection_evidence_identity&
  evidence() const noexcept;

  /*! \brief Find projected owners for one exact path.
   *  \param path Path to find.
   *  \return Pointer valid for this projection's lifetime, or `nullptr`.
   */
  [[nodiscard]] const projected_path_owners*
  find(const pkgplan::package_path& path) const noexcept;

private:
  /*! \brief Construct normalized authority already identified by make(). */
  lease_bound_state_projection(
      lease_bound_state_projection_identity identity,
      mutation_lease_instance_identity lease,
      pkgplan::installed_state_snapshot_identity snapshot,
      pkgplan::ownership_inventory_identity ownership_inventory,
      state_projection_completeness completeness,
      std::vector<projected_path_owners> paths,
      state_projection_evidence_identity evidence);

  std::uint16_t schema_version_ = lease_bound_state_projection_schema_version;
  lease_bound_state_projection_identity identity_;
  mutation_lease_instance_identity lease_;
  pkgplan::installed_state_snapshot_identity snapshot_;
  pkgplan::ownership_inventory_identity ownership_inventory_;
  state_projection_completeness completeness_;
  std::vector<projected_path_owners> paths_;
  state_projection_evidence_identity evidence_;
};

} // namespace pkgapply
