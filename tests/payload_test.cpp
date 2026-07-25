// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/payload.h>

#include "plan_fixture.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
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

pkgimage::package_image
image()
{
  return pkgimage::package_image({
      pkgapply::test::fixture::regular_entry("base", 1),
      pkgapply::test::fixture::hardlink_entry("tool", "base"),
      pkgapply::test::fixture::directory_entry("share"),
  });
}

pkgplan::installation_plan
installation()
{
  const pkgapply::test::fixture::planning_authorities authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(2));
  const auto base = pkgplan::package_path::parse("base");
  const auto tool = pkgplan::package_path::parse("tool");
  const auto share = pkgplan::package_path::parse("share");
  const auto retained = pkgapply::test::fixture::regular_object(1);
  const auto policy = pkgapply::test::fixture::policy_snapshot(
      authorities,
      pkgapply::test::fixture::path_policy(),
      {pkgplan::path_policy_override(
          base,
          pkgapply::test::fixture::path_policy(
              pkgplan::incoming_path_policy::retain(
                  pkgplan::rejected_object_policy::stage,
                  pkgplan::retained_active_ownership_policy::
                      do_not_claim_operated_package)))});
  return pkgapply::test::fixture::installation_plan(
      authorities,
      {
          pkgapply::test::fixture::regular_entry("base", 1),
          pkgapply::test::fixture::hardlink_entry("tool", "base"),
          pkgapply::test::fixture::directory_entry("share"),
      },
      {
          pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(base, retained)),
          pkgplan::target_path_observation::absent(tool),
          pkgplan::target_path_observation::absent(share),
      },
      {},
      policy);
}

} // namespace

int
main()
{
  const auto package_image = image();
  const auto payload = pkgapply::prepare_incoming_payloads(
      installation(), package_image);

  require(payload.image() == package_image.identity(),
          "payload plan changed package-image identity");
  require(payload.selection().size() == 1 &&
              payload.selection().contains(pkgimage::entry_id{0}),
          "payload closure did not deduplicate the regular anchor");
  require(payload.requirements().size() == 3,
          "payload consumers were lost");

  require(payload.requirements()[0].path().string() == "base" &&
              payload.requirements()[0].image_entry() == 0 &&
              payload.requirements()[0].regular_payload_entry() ==
                  pkgimage::entry_id{0} &&
              !payload.requirements()[0].required_for_active() &&
              payload.requirements()[0].required_for_rejected(),
          "rejected regular payload requirement changed");

  require(payload.requirements()[1].path().string() == "share" &&
              payload.requirements()[1].image_entry() == 2 &&
              !payload.requirements()[1].regular_payload_entry().has_value(),
          "non-regular incoming object incorrectly acquired payload bytes");

  require(payload.requirements()[2].path().string() == "tool" &&
              payload.requirements()[2].image_entry() == 1 &&
              payload.requirements()[2].regular_payload_entry() ==
                  pkgimage::entry_id{0} &&
              payload.requirements()[2].required_for_active() &&
              !payload.requirements()[2].required_for_rejected(),
          "hard-link payload anchor was not retained");

  const pkgapply::test::fixture::planning_authorities no_payload_authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(4));
  const auto base = pkgplan::package_path::parse("base");
  const auto retained = pkgapply::test::fixture::regular_object(1);
  const auto retain_policy = pkgapply::test::fixture::policy_snapshot(
      no_payload_authorities,
      pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::retain(
              pkgplan::rejected_object_policy::none,
              pkgplan::retained_active_ownership_policy::
                  add_operated_owner)));
  const auto no_payload_plan = pkgapply::test::fixture::installation_plan(
      no_payload_authorities,
      {pkgapply::test::fixture::regular_entry("base", 1)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(base, retained))},
      {},
      retain_policy);
  const auto no_payload = pkgapply::prepare_incoming_payloads(
      no_payload_plan, package_image);
  require(no_payload.selection().size() == 0 &&
              no_payload.requirements().empty(),
          "unused incoming entry entered the replay closure");

  bool rejected = false;
  try {
    const pkgimage::package_image mismatched({
        pkgapply::test::fixture::regular_entry("base", 1),
        pkgapply::test::fixture::hardlink_entry("other", "base"),
        pkgapply::test::fixture::directory_entry("share"),
    });
    static_cast<void>(pkgapply::prepare_incoming_payloads(
        installation(), mismatched));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected,
          "incoming entry bound to another path entered payload closure");

  return 0;
}
