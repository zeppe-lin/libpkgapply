// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/capture.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void
require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template<class Identity>
Identity
planning_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return Identity::from_sha256(bytes);
}

pkgapply::application_execution_control
control(pkgapply::application_recovery_requirement recovery)
{
  return pkgapply::application_execution_control::make(
      recovery,
      recovery == pkgapply::application_recovery_requirement::exact_prior_state
          ? pkgapply::application_durability_requirement::journal_and_recovery
          : pkgapply::application_durability_requirement::visibility_only,
      recovery == pkgapply::application_recovery_requirement::none
          ? pkgapply::application_cancellation_policy::before_target_mutation_only
          : pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

pkgplan::target_path_observation
present(const pkgplan::package_path& path)
{
  return pkgplan::target_path_observation::present(
      pkgplan::filesystem_object_fact(
          path,
          pkgplan::filesystem_object_metadata(
              pkgplan::filesystem_object_kind::regular,
              0644,
              0,
              0)));
}

pkgplan::operation_preconditions
preconditions(std::vector<pkgplan::path_precondition> paths)
{
  return pkgplan::operation_preconditions(
      planning_identity<pkgplan::target_system_context_identity>(1),
      planning_identity<pkgplan::installed_state_snapshot_identity>(2),
      planning_identity<pkgplan::ownership_inventory_identity>(3),
      std::nullopt,
      std::move(paths));
}

pkgplan::path_ownership_transition
ownership()
{
  return pkgplan::path_ownership_transition({}, {}, false);
}

} // namespace

int
main()
{
  const auto existing = pkgplan::package_path::parse("usr/bin/existing");
  const auto incoming = pkgplan::package_path::parse("usr/bin/new");

  const auto install = pkgplan::installation_plan(
      planning_identity<pkgplan::operation_plan_identity>(4),
      preconditions({
          pkgplan::path_precondition(present(existing), {}),
          pkgplan::path_precondition(
              pkgplan::target_path_observation::absent(incoming), {}),
      }),
      {
          pkgplan::installation_path_decision(
              existing,
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{0},
              ownership()),
          pkgplan::installation_path_decision(
              incoming,
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{1},
              ownership()),
      });

  const auto install_captures = pkgapply::prepare_old_object_captures(
      install,
      control(pkgapply::application_recovery_requirement::exact_prior_state));
  require(install_captures.requests().size() == 1,
          "recovery capture included an absent incoming path");
  require(install_captures.find(existing) != nullptr &&
              install_captures.find(existing)->for_recovery() &&
              !install_captures.find(existing)->for_rejected_object(),
          "replacement recovery capture was not derived");

  const auto upgrade = pkgplan::upgrade_plan(
      planning_identity<pkgplan::operation_plan_identity>(5),
      preconditions({pkgplan::path_precondition(present(existing), {})}),
      {pkgplan::upgrade_path_decision(
          existing,
          pkgplan::upgrade_path_role::obsolete_old_path,
          pkgplan::planned_active_outcome::remove_observed,
          pkgplan::planned_rejected_outcome::stage_old,
          std::nullopt,
          ownership())});
  const auto upgrade_captures = pkgapply::prepare_old_object_captures(
      upgrade,
      control(pkgapply::application_recovery_requirement::best_effort));
  require(upgrade_captures.requests().size() == 1 &&
              upgrade_captures.requests()[0].for_recovery() &&
              upgrade_captures.requests()[0].for_rejected_object(),
          "upgrade capture did not merge recovery and rejected consumers");

  const auto removal = pkgplan::removal_plan(
      planning_identity<pkgplan::operation_plan_identity>(6),
      preconditions({pkgplan::path_precondition(present(existing), {})}),
      {pkgplan::removal_path_decision(
          existing,
          pkgplan::planned_active_outcome::remove_observed,
          pkgplan::planned_rejected_outcome::stage_old,
          ownership())});
  const auto removal_captures = pkgapply::prepare_old_object_captures(
      removal,
      control(pkgapply::application_recovery_requirement::none));
  require(removal_captures.requests().size() == 1 &&
              removal_captures.requests()[0].for_rejected_object() &&
              !removal_captures.requests()[0].for_recovery(),
          "stage-old removal capture incorrectly acquired recovery policy");

  const auto retained = pkgplan::removal_plan(
      planning_identity<pkgplan::operation_plan_identity>(7),
      preconditions({pkgplan::path_precondition(present(existing), {})}),
      {pkgplan::removal_path_decision(
          existing,
          pkgplan::planned_active_outcome::retain_observed,
          pkgplan::planned_rejected_outcome::none,
          ownership())});
  const auto no_captures = pkgapply::prepare_old_object_captures(
      retained,
      control(pkgapply::application_recovery_requirement::exact_prior_state));
  require(no_captures.requests().empty(),
          "retained object entered old-object capture set");

  return 0;
}
