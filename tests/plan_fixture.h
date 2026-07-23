// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <libpkgplan/install.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply::test::fixture {

template<class Identity>
[[nodiscard]] inline Identity
planning_identity(std::uint8_t value)
{
  pkgplan::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return Identity::from_sha256(bytes);
}

[[nodiscard]] inline pkgimage::sha256_digest_bytes
image_bytes(std::uint8_t value)
{
  pkgimage::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return bytes;
}

[[nodiscard]] inline pkgimage::complete_archive_digest
archive_digest(std::uint8_t value = 50)
{
  return pkgimage::complete_archive_digest::from_sha256(image_bytes(value));
}

struct planning_authorities final {
  explicit planning_authorities(pkgplan::target_system_context_identity target)
      : target(std::move(target)),
        snapshot(planning_identity<
            pkgplan::installed_state_snapshot_identity>(20)),
        ownership_inventory(planning_identity<
            pkgplan::ownership_inventory_identity>(21)),
        observations(planning_identity<pkgplan::observation_set_identity>(31)),
        policy(planning_identity<pkgplan::policy_snapshot_identity>(40)),
        runtime_closure(planning_identity<
            pkgplan::runtime_dependency_closure_identity>(32)),
        installed_package(planning_identity<
            pkgplan::installed_package_identity>(10)),
        installed_control(planning_identity<
            pkgplan::installed_control_identity>(11))
  {
  }

  pkgplan::target_system_context_identity target;
  pkgplan::installed_state_snapshot_identity snapshot;
  pkgplan::ownership_inventory_identity ownership_inventory;
  pkgplan::observation_set_identity observations;
  pkgplan::policy_snapshot_identity policy;
  pkgplan::runtime_dependency_closure_identity runtime_closure;
  pkgplan::installed_package_identity installed_package;
  pkgplan::installed_control_identity installed_control;
};

[[nodiscard]] inline pkgplan::package_release
release(std::uint8_t identity_value,
        std::string version,
        std::string name = "tool")
{
  return pkgplan::package_release(
      planning_identity<pkgplan::package_release_identity>(identity_value),
      std::move(name),
      std::move(version),
      "1");
}

[[nodiscard]] inline pkgplan::filesystem_object_metadata
regular_object(std::uint8_t content,
               std::uint32_t mode = 0644,
               bool complete = true)
{
  return pkgplan::filesystem_object_metadata(
      pkgplan::filesystem_object_kind::regular,
      mode,
      0,
      0,
      complete ? std::optional<std::uint64_t>(4) : std::nullopt,
      pkgplan::object_timestamp(10, 0),
      complete
          ? std::optional<pkgplan::filesystem_regular_content_identity>(
                planning_identity<
                    pkgplan::filesystem_regular_content_identity>(content))
          : std::nullopt);
}

[[nodiscard]] inline pkgplan::filesystem_object_metadata
directory_object(std::uint32_t mode = 0755)
{
  return pkgplan::filesystem_object_metadata(
      pkgplan::filesystem_object_kind::directory, mode, 0, 0);
}

[[nodiscard]] inline pkgimage::package_entry
regular_entry(std::string path,
              std::uint8_t content,
              std::uint32_t mode = 0644)
{
  pkgimage::package_entry entry(
      pkgimage::package_path::parse(path), pkgimage::entry_type::regular);
  entry.mode = mode;
  entry.uid = 0;
  entry.gid = 0;
  entry.size = 4;
  entry.regular_content =
      pkgimage::regular_content_digest::from_sha256(image_bytes(content));
  return entry;
}

[[nodiscard]] inline pkgimage::package_entry
directory_entry(std::string path, std::uint32_t mode = 0755)
{
  pkgimage::package_entry entry(
      pkgimage::package_path::parse(path), pkgimage::entry_type::directory);
  entry.mode = mode;
  entry.uid = 0;
  entry.gid = 0;
  return entry;
}

[[nodiscard]] inline pkgimage::package_entry
hardlink_entry(std::string path, std::string target)
{
  pkgimage::package_entry entry(
      pkgimage::package_path::parse(path), pkgimage::entry_type::hardlink);
  entry.mode = 0644;
  entry.uid = 0;
  entry.gid = 0;
  entry.hardlink_target = pkgimage::package_path::parse(target);
  return entry;
}

[[nodiscard]] inline pkgimage::inspected_package_image
inspected_image(std::vector<pkgimage::package_entry> entries,
                pkgimage::complete_archive_digest digest = archive_digest(),
                std::string backend = "test/pkgimage-v1")
{
  pkgimage::package_image image(std::move(entries));
  pkgimage::archive_inspection_receipt receipt(
      pkgimage::archive_backend_identity::parse(backend),
      std::move(digest),
      image.identity(),
      image.size());
  return pkgimage::inspected_package_image(
      std::move(image), std::move(receipt));
}

[[nodiscard]] inline pkgplan::normalized_path_policy
path_policy(
    pkgplan::incoming_path_policy incoming =
        pkgplan::incoming_path_policy::activate(),
    pkgplan::obsolete_path_policy obsolete =
        pkgplan::obsolete_path_policy::remove(),
    pkgplan::shared_ownership_policy shared =
        pkgplan::shared_ownership_policy::forbid,
    pkgplan::directory_cleanup_policy cleanup =
        pkgplan::directory_cleanup_policy::remove_if_empty)
{
  return pkgplan::normalized_path_policy(
      std::move(incoming), std::move(obsolete), shared, cleanup);
}

[[nodiscard]] inline pkgplan::package_policy_snapshot
policy_snapshot(
    const planning_authorities& authorities,
    pkgplan::normalized_path_policy defaults = path_policy(),
    std::vector<pkgplan::path_policy_override> overrides = {})
{
  return pkgplan::package_policy_snapshot(
      authorities.policy, std::move(defaults), std::move(overrides));
}

[[nodiscard]] inline pkgplan::installed_ownership_inventory
ownership(
    const planning_authorities& authorities,
    std::vector<pkgplan::installed_ownership_claim> claims = {},
    pkgplan::fact_set_completeness completeness =
        pkgplan::fact_set_completeness::complete)
{
  return pkgplan::installed_ownership_inventory(
      authorities.ownership_inventory,
      authorities.snapshot,
      completeness,
      std::move(claims));
}

[[nodiscard]] inline pkgplan::target_observation_set
observations(
    const planning_authorities& authorities,
    std::vector<pkgplan::target_path_observation> facts,
    pkgplan::fact_set_completeness completeness =
        pkgplan::fact_set_completeness::complete)
{
  return pkgplan::target_observation_set(
      authorities.observations,
      authorities.target,
      completeness,
      std::move(facts));
}

[[nodiscard]] inline pkgplan::installed_package_fact
installed(const planning_authorities& authorities,
          pkgplan::package_release installed_release = release(1, "1.0"))
{
  return pkgplan::installed_package_fact(
      authorities.installed_package,
      authorities.installed_control,
      authorities.snapshot,
      std::move(installed_release));
}

[[nodiscard]] inline pkgplan::installation_plan
installation_plan(
    const planning_authorities& authorities,
    std::vector<pkgimage::package_entry> entries,
    std::vector<pkgplan::target_path_observation> observed,
    std::vector<pkgplan::installed_ownership_claim> claims = {},
    std::optional<pkgplan::package_policy_snapshot> selected_policy =
        std::nullopt,
    pkgimage::complete_archive_digest digest = archive_digest())
{
  const auto incoming_release = release(1, "1.0");
  pkgplan::installation_request request(
      pkgplan::candidate_package_fact(
          planning_identity<pkgplan::candidate_control_identity>(2),
          incoming_release),
      pkgplan::artifact_package_fact(
          planning_identity<pkgplan::artifact_identity>(3),
          planning_identity<pkgplan::artifact_manifest_identity>(4),
          incoming_release),
      digest,
      inspected_image(std::move(entries), digest),
      authorities.snapshot,
      ownership(authorities, std::move(claims)),
      authorities.target,
      observations(authorities, std::move(observed)),
      authorities.runtime_closure,
      selected_policy
          ? std::move(*selected_policy)
          : policy_snapshot(authorities));

  pkgplan::installation_result result = pkgplan::plan_install(request);
  if (!result.has_plan() || result.plan() == nullptr)
    throw std::runtime_error("installation fixture was refused");
  return *result.plan();
}

[[nodiscard]] inline pkgplan::upgrade_plan
upgrade_plan(
    const planning_authorities& authorities,
    std::vector<pkgimage::package_entry> entries,
    std::vector<pkgplan::target_path_observation> observed,
    std::vector<pkgplan::installed_ownership_claim> claims,
    std::optional<pkgplan::package_policy_snapshot> selected_policy =
        std::nullopt,
    pkgimage::complete_archive_digest digest = archive_digest())
{
  const auto incoming_release = release(2, "2.0");
  pkgplan::upgrade_request request(
      installed(authorities),
      pkgplan::candidate_package_fact(
          planning_identity<pkgplan::candidate_control_identity>(3),
          incoming_release),
      pkgplan::artifact_package_fact(
          planning_identity<pkgplan::artifact_identity>(4),
          planning_identity<pkgplan::artifact_manifest_identity>(5),
          incoming_release),
      digest,
      inspected_image(std::move(entries), digest),
      authorities.snapshot,
      ownership(authorities, std::move(claims)),
      authorities.target,
      observations(authorities, std::move(observed)),
      authorities.runtime_closure,
      selected_policy
          ? std::move(*selected_policy)
          : policy_snapshot(authorities));

  pkgplan::upgrade_result result = pkgplan::plan_upgrade(request);
  if (!result.has_plan() || result.plan() == nullptr)
    throw std::runtime_error("upgrade fixture was refused");
  return *result.plan();
}

[[nodiscard]] inline pkgplan::removal_plan
removal_plan(
    const planning_authorities& authorities,
    std::vector<pkgplan::installed_ownership_claim> claims,
    std::vector<pkgplan::target_path_observation> observed,
    std::optional<pkgplan::package_policy_snapshot> selected_policy =
        std::nullopt)
{
  pkgplan::removal_request request(
      installed(authorities),
      authorities.snapshot,
      ownership(authorities, std::move(claims)),
      authorities.target,
      observations(authorities, std::move(observed)),
      selected_policy
          ? std::move(*selected_policy)
          : policy_snapshot(authorities));

  pkgplan::removal_result result = pkgplan::plan_removal(request);
  if (!result.has_plan() || result.plan() == nullptr)
    throw std::runtime_error("removal fixture was refused");
  return *result.plan();
}

[[nodiscard]] inline pkgplan::installation_plan
ordinary_installation(const planning_authorities& authorities,
                      std::string path = "tool")
{
  const auto logical = pkgplan::package_path::parse(path);
  return installation_plan(
      authorities,
      {regular_entry(path, 7)},
      {pkgplan::target_path_observation::absent(logical)});
}

[[nodiscard]] inline pkgplan::upgrade_plan
ordinary_upgrade(const planning_authorities& authorities,
                 std::string path = "tool")
{
  const auto logical = pkgplan::package_path::parse(path);
  const auto active = regular_object(1, 0755);
  return upgrade_plan(
      authorities,
      {regular_entry(path, 2, 0755)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(logical, active))},
      {pkgplan::installed_ownership_claim(
          logical, authorities.installed_package, active)});
}

[[nodiscard]] inline pkgplan::removal_plan
ordinary_removal(const planning_authorities& authorities,
                 std::string path = "tool")
{
  const auto logical = pkgplan::package_path::parse(path);
  const auto active = regular_object(1, 0755);
  return removal_plan(
      authorities,
      {pkgplan::installed_ownership_claim(
          logical, authorities.installed_package, active)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(logical, active))});
}

} // namespace pkgapply::test::fixture
