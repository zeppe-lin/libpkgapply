// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/state_projection.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace pkgapply {
namespace {

std::uint8_t
canonical_completeness(state_projection_completeness value)
{
  switch (value) {
    case state_projection_completeness::complete:
      return 1;
    case state_projection_completeness::incomplete:
      return 2;
  }
  throw std::invalid_argument("invalid state projection completeness");
}

lease_bound_state_projection_identity
identify_projection(
    const mutation_lease_instance_identity& lease,
    const pkgplan::installed_state_snapshot_identity& snapshot,
    const pkgplan::ownership_inventory_identity& ownership_inventory,
    state_projection_completeness completeness,
    const std::vector<projected_path_owners>& paths,
    const state_projection_evidence_identity& evidence)
{
  detail::canonical_record record(
      lease_bound_state_projection_identity::canonical_domain());
  record.append_u16(lease_bound_state_projection_schema_version);
  record.append_digest(lease);
  record.append_bytes(snapshot.string());
  record.append_bytes(ownership_inventory.string());
  record.append_u8(canonical_completeness(completeness));
  record.append_u64(static_cast<std::uint64_t>(paths.size()));
  for (const projected_path_owners& path : paths) {
    record.append_bytes(path.path().string());
    record.append_u64(static_cast<std::uint64_t>(path.owners().size()));
    for (const auto& owner : path.owners())
      record.append_bytes(owner.string());
  }
  record.append_digest(evidence);
  return detail::identity_factory::from_sha256<
      lease_bound_state_projection_identity>(record.sha256());
}

} // namespace

projected_path_owners::projected_path_owners(
    pkgplan::package_path path,
    std::vector<pkgplan::installed_package_identity> owners)
    : path_(std::move(path)), owners_(std::move(owners))
{
  std::sort(owners_.begin(), owners_.end());
  const auto duplicate = std::adjacent_find(owners_.begin(), owners_.end());
  if (duplicate != owners_.end())
    throw std::invalid_argument("duplicate installed owner in state projection");
}

const pkgplan::package_path&
projected_path_owners::path() const noexcept
{
  return path_;
}

const std::vector<pkgplan::installed_package_identity>&
projected_path_owners::owners() const noexcept
{
  return owners_;
}

bool
operator==(const projected_path_owners& lhs,
           const projected_path_owners& rhs) noexcept
{
  return lhs.path_ == rhs.path_ && lhs.owners_ == rhs.owners_;
}

bool
operator!=(const projected_path_owners& lhs,
           const projected_path_owners& rhs) noexcept
{
  return !(lhs == rhs);
}

bool
operator<(const projected_path_owners& lhs,
          const projected_path_owners& rhs) noexcept
{
  return std::tie(lhs.path_, lhs.owners_) < std::tie(rhs.path_, rhs.owners_);
}

lease_bound_state_projection
lease_bound_state_projection::make(
    mutation_lease_instance_identity lease,
    pkgplan::installed_state_snapshot_identity snapshot,
    pkgplan::ownership_inventory_identity ownership_inventory,
    state_projection_completeness completeness,
    std::vector<projected_path_owners> paths,
    state_projection_evidence_identity evidence)
{
  std::sort(paths.begin(), paths.end(),
            [](const projected_path_owners& lhs,
               const projected_path_owners& rhs) {
              return lhs.path() < rhs.path();
            });
  const auto duplicate = std::adjacent_find(
      paths.begin(), paths.end(),
      [](const projected_path_owners& lhs,
         const projected_path_owners& rhs) {
        return lhs.path() == rhs.path();
      });
  if (duplicate != paths.end())
    throw std::invalid_argument("duplicate path in state projection");

  lease_bound_state_projection_identity identity = identify_projection(
      lease, snapshot, ownership_inventory, completeness, paths, evidence);
  return lease_bound_state_projection(
      std::move(identity),
      std::move(lease),
      std::move(snapshot),
      std::move(ownership_inventory),
      completeness,
      std::move(paths),
      std::move(evidence));
}

lease_bound_state_projection::lease_bound_state_projection(
    lease_bound_state_projection_identity identity,
    mutation_lease_instance_identity lease,
    pkgplan::installed_state_snapshot_identity snapshot,
    pkgplan::ownership_inventory_identity ownership_inventory,
    state_projection_completeness completeness,
    std::vector<projected_path_owners> paths,
    state_projection_evidence_identity evidence)
    : identity_(std::move(identity)),
      lease_(std::move(lease)),
      snapshot_(std::move(snapshot)),
      ownership_inventory_(std::move(ownership_inventory)),
      completeness_(completeness),
      paths_(std::move(paths)),
      evidence_(std::move(evidence))
{
}

std::uint16_t
lease_bound_state_projection::schema_version() const noexcept
{
  return schema_version_;
}

const lease_bound_state_projection_identity&
lease_bound_state_projection::identity() const noexcept
{
  return identity_;
}

const mutation_lease_instance_identity&
lease_bound_state_projection::lease() const noexcept
{
  return lease_;
}

const pkgplan::installed_state_snapshot_identity&
lease_bound_state_projection::snapshot() const noexcept
{
  return snapshot_;
}

const pkgplan::ownership_inventory_identity&
lease_bound_state_projection::ownership_inventory() const noexcept
{
  return ownership_inventory_;
}

state_projection_completeness
lease_bound_state_projection::completeness() const noexcept
{
  return completeness_;
}

const std::vector<projected_path_owners>&
lease_bound_state_projection::paths() const noexcept
{
  return paths_;
}

const state_projection_evidence_identity&
lease_bound_state_projection::evidence() const noexcept
{
  return evidence_;
}

const projected_path_owners*
lease_bound_state_projection::find(
    const pkgplan::package_path& path) const noexcept
{
  const auto found = std::lower_bound(
      paths_.begin(), paths_.end(), path,
      [](const projected_path_owners& item,
         const pkgplan::package_path& expected) {
        return item.path() < expected;
      });
  if (found == paths_.end() || found->path() != path)
    return nullptr;
  return &*found;
}

} // namespace pkgapply
