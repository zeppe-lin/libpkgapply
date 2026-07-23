// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/capture.h>

#include "plan_fixture.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void
require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
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

} // namespace

int
main()
{
  const auto existing = pkgplan::package_path::parse("existing");
  const auto incoming = pkgplan::package_path::parse("new");
  const auto active = pkgapply::test::fixture::regular_object(9);

  const pkgapply::test::fixture::planning_authorities install_authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(1));
  const auto install = pkgapply::test::fixture::installation_plan(
      install_authorities,
      {
          pkgapply::test::fixture::regular_entry("existing", 1),
          pkgapply::test::fixture::regular_entry("new", 2),
      },
      {
          pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(existing, active)),
          pkgplan::target_path_observation::absent(incoming),
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

  const pkgapply::test::fixture::planning_authorities upgrade_authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(2));
  const auto upgrade_policy = pkgapply::test::fixture::policy_snapshot(
      upgrade_authorities,
      pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::remove(
              pkgplan::rejected_object_policy::stage)));
  const auto upgrade = pkgapply::test::fixture::upgrade_plan(
      upgrade_authorities,
      {},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(existing, active))},
      {pkgplan::installed_ownership_claim(
          existing, upgrade_authorities.installed_package, active)},
      upgrade_policy);
  const auto upgrade_captures = pkgapply::prepare_old_object_captures(
      upgrade,
      control(pkgapply::application_recovery_requirement::best_effort));
  require(upgrade_captures.requests().size() == 1 &&
              upgrade_captures.requests()[0].for_recovery() &&
              upgrade_captures.requests()[0].for_rejected_object(),
          "upgrade capture did not merge recovery and rejected consumers");

  const pkgapply::test::fixture::planning_authorities removal_authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(3));
  const auto removal_policy = pkgapply::test::fixture::policy_snapshot(
      removal_authorities,
      pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::remove(
              pkgplan::rejected_object_policy::stage)));
  const auto removal = pkgapply::test::fixture::removal_plan(
      removal_authorities,
      {pkgplan::installed_ownership_claim(
          existing, removal_authorities.installed_package, active)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(existing, active))},
      removal_policy);
  const auto removal_captures = pkgapply::prepare_old_object_captures(
      removal,
      control(pkgapply::application_recovery_requirement::none));
  require(removal_captures.requests().size() == 1 &&
              removal_captures.requests()[0].for_rejected_object() &&
              !removal_captures.requests()[0].for_recovery(),
          "stage-old removal capture incorrectly acquired recovery policy");

  const auto retained_policy = pkgapply::test::fixture::policy_snapshot(
      removal_authorities,
      pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::retain_relic()));
  const auto retained = pkgapply::test::fixture::removal_plan(
      removal_authorities,
      {pkgplan::installed_ownership_claim(
          existing, removal_authorities.installed_package, active)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(existing, active))},
      retained_policy);
  const auto no_captures = pkgapply::prepare_old_object_captures(
      retained,
      control(pkgapply::application_recovery_requirement::exact_prior_state));
  require(no_captures.requests().empty(),
          "retained object entered old-object capture set");

  return 0;
}
