// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/request.h>

#include "plan_fixture.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message)
{
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

template<class Identity>
Identity identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const std::uint8_t byte = static_cast<std::uint8_t>(value + index);
    text.push_back(hex[(byte >> 4) & 15]);
    text.push_back(hex[byte & 15]);
  }
  return Identity::parse(text);
}

pkgplan::target_system_context_identity plan_target(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return pkgplan::target_system_context_identity::from_sha256(bytes);
}

pkgapply::application_target_context context(std::uint8_t target_value = 1)
{
  return pkgapply::application_target_context::make(
      plan_target(target_value),
      identity<pkgapply::managed_target_identity>(2),
      identity<pkgapply::root_view_identity>(3),
      identity<pkgapply::observation_backend_identity>(4),
      identity<pkgapply::mutation_backend_identity>(5),
      identity<pkgapply::mutation_exclusion_domain_identity>(6),
      identity<pkgapply::active_object_namespace_identity>(7),
      identity<pkgapply::rejected_object_store_identity>(8),
      identity<pkgapply::staging_namespace_identity>(9),
      identity<pkgapply::journal_namespace_identity>(10),
      identity<pkgapply::execution_capability_profile_identity>(11));
}

pkgapply::application_execution_control control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::exact_prior_state,
      pkgapply::application_durability_requirement::all_application_domains,
      pkgapply::application_cancellation_policy::recover_after_target_mutation,
      4096,
      8192);
}

} // namespace

int main()
{
  const auto target = context();
  const auto execution = control();
  const pkgapply::test::fixture::planning_authorities authorities(
      target.target());

  const auto install_plan =
      pkgapply::test::fixture::ordinary_installation(authorities);
  const auto upgrade_plan =
      pkgapply::test::fixture::ordinary_upgrade(authorities);
  const auto removal_plan =
      pkgapply::test::fixture::ordinary_removal(authorities);

  const auto install = pkgapply::installation_application_request::make(
      install_plan, target, execution);
  const auto upgrade = pkgapply::upgrade_application_request::make(
      upgrade_plan, target, execution);
  const auto removal = pkgapply::removal_application_request::make(
      removal_plan, target, execution);

  require(install.identity() != upgrade.identity(),
          "operation kind must participate in request identity");
  require(upgrade.identity() != removal.identity(),
          "operation kinds must remain distinct");
  require(install.plan().identity() == install_plan.identity(),
          "request must retain the exact accepted plan");
  require(execution.identity().string() == "v1:sha256:087ad9450a5d6fdfb3aa4e09c86ee69a6057f956ee99fab52c94157ea6cde056",
          "execution control identity vector changed");

  const auto repeated = pkgapply::installation_application_request::make(
      install_plan, target, execution);
  require(repeated.identity() == install.identity(),
          "identical application requests must have identical identities");

  bool rejected = false;
  try {
    const pkgapply::test::fixture::planning_authorities foreign_authorities(
        plan_target(99));
    static_cast<void>(pkgapply::removal_application_request::make(
        pkgapply::test::fixture::ordinary_removal(foreign_authorities),
        target,
        execution));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "request must reject a target-mismatched plan");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_execution_control::make(
        pkgapply::application_recovery_requirement::exact_prior_state,
        pkgapply::application_durability_requirement::visibility_only,
        pkgapply::application_cancellation_policy::before_target_mutation_only));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "incoherent exact-recovery control must be rejected");

  return 0;
}
