// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include <libpkgapply/digest.h>
#include <libpkgplan/digest.h>
#include <libpkgplan/package_path.h>

namespace pkgapply {

inline constexpr std::uint16_t lease_bound_state_projection_schema_version = 1;

/*! \brief Completeness of the caller-supplied state projection universe. */
enum class state_projection_completeness : std::uint8_t {
  complete = 1,
  incomplete = 2,
};

/*! \brief Exact current installed owners for one operated path. */
class projected_path_owners final {
public:
  projected_path_owners(
      pkgplan::package_path path,
      std::vector<pkgplan::installed_package_identity> owners);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] const std::vector<pkgplan::installed_package_identity>&
  owners() const noexcept;

  friend bool operator==(const projected_path_owners& lhs,
                         const projected_path_owners& rhs) noexcept;
  friend bool operator!=(const projected_path_owners& lhs,
                         const projected_path_owners& rhs) noexcept;
  friend bool operator<(const projected_path_owners& lhs,
                        const projected_path_owners& rhs) noexcept;

private:
  pkgplan::package_path path_;
  std::vector<pkgplan::installed_package_identity> owners_;
};

/*! \brief Immutable installed-state facts established under one target lease. */
class lease_bound_state_projection final {
public:
  [[nodiscard]] static lease_bound_state_projection
  make(mutation_lease_instance_identity lease,
       pkgplan::installed_state_snapshot_identity snapshot,
       pkgplan::ownership_inventory_identity ownership_inventory,
       state_projection_completeness completeness,
       std::vector<projected_path_owners> paths,
       state_projection_evidence_identity evidence);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const lease_bound_state_projection_identity&
  identity() const noexcept;
  [[nodiscard]] const mutation_lease_instance_identity& lease() const noexcept;
  [[nodiscard]] const pkgplan::installed_state_snapshot_identity&
  snapshot() const noexcept;
  [[nodiscard]] const pkgplan::ownership_inventory_identity&
  ownership_inventory() const noexcept;
  [[nodiscard]] state_projection_completeness completeness() const noexcept;
  [[nodiscard]] const std::vector<projected_path_owners>& paths() const noexcept;
  [[nodiscard]] const state_projection_evidence_identity& evidence() const noexcept;

  [[nodiscard]] const projected_path_owners*
  find(const pkgplan::package_path& path) const noexcept;

private:
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
