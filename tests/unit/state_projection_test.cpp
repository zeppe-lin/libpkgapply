// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/state_projection.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, std::string_view message)
{
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

template<class Identity>
Identity app_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < 32; ++i) {
    const auto byte = static_cast<std::uint8_t>(value + i);
    text.push_back(hex[(byte >> 4) & 15]);
    text.push_back(hex[byte & 15]);
  }
  return Identity::parse(text);
}

template<class Identity>
Identity plan_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bytes[i] = static_cast<std::uint8_t>(value + i);
  return Identity::from_sha256(bytes);
}

pkgapply::lease_bound_state_projection projection(bool reverse = false)
{
  const auto owner_a = plan_identity<pkgplan::installed_package_identity>(20);
  const auto owner_b = plan_identity<pkgplan::installed_package_identity>(40);
  std::vector<pkgapply::projected_path_owners> paths;
  paths.emplace_back(pkgplan::package_path::parse("usr/bin/tool"),
                     std::vector{owner_b, owner_a});
  paths.emplace_back(pkgplan::package_path::parse("etc/tool.conf"),
                     std::vector<pkgplan::installed_package_identity>{});
  if (reverse)
    std::reverse(paths.begin(), paths.end());
  return pkgapply::lease_bound_state_projection::make(
      app_identity<pkgapply::mutation_lease_instance_identity>(1),
      plan_identity<pkgplan::installed_state_snapshot_identity>(2),
      plan_identity<pkgplan::ownership_inventory_identity>(3),
      pkgapply::state_projection_completeness::complete,
      std::move(paths),
      app_identity<pkgapply::state_projection_evidence_identity>(4));
}
}

int main()
{
  const auto first = projection();
  const auto reordered = projection(true);
  require(first.identity() == reordered.identity(),
          "projection identity must ignore caller path order");
  require(first.paths().front().path().string() == "etc/tool.conf",
          "projection paths must be canonical");
  const auto* owners = first.find(pkgplan::package_path::parse("usr/bin/tool"));
  require(owners != nullptr && owners->owners().size() == 2,
          "projected owners were not retained");
  require(owners->owners()[0] < owners->owners()[1],
          "owner identities must be canonical");
  require(first.identity().string() == "v1:sha256:e03ac257c25ef16a1559445f9d4461fda5cfef41eac9f80509eb15d784cd11f7",
          "state projection identity vector changed");

  bool rejected = false;
  try {
    const auto owner = plan_identity<pkgplan::installed_package_identity>(20);
    static_cast<void>(pkgapply::projected_path_owners(
        pkgplan::package_path::parse("usr/bin/tool"),
        std::vector{owner, owner}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "duplicate owners must be rejected");

  rejected = false;
  try {
    std::vector<pkgapply::projected_path_owners> paths;
    paths.emplace_back(pkgplan::package_path::parse("usr/bin/tool"),
                       std::vector<pkgplan::installed_package_identity>{});
    paths.emplace_back(pkgplan::package_path::parse("usr/bin/tool"),
                       std::vector<pkgplan::installed_package_identity>{});
    static_cast<void>(pkgapply::lease_bound_state_projection::make(
        app_identity<pkgapply::mutation_lease_instance_identity>(1),
        plan_identity<pkgplan::installed_state_snapshot_identity>(2),
        plan_identity<pkgplan::ownership_inventory_identity>(3),
        pkgapply::state_projection_completeness::complete,
        std::move(paths),
        app_identity<pkgapply::state_projection_evidence_identity>(4)));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "duplicate projection paths must be rejected");
  return 0;
}
