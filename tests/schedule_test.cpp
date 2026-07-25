// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/schedule.h>

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
incoming_image()
{
  return pkgimage::package_image({
      pkgapply::test::fixture::directory_entry("usr"),
      pkgapply::test::fixture::directory_entry("usr/bin"),
      pkgapply::test::fixture::regular_entry("usr/bin/base", 1),
      pkgapply::test::fixture::hardlink_entry(
          "usr/bin/tool", "usr/bin/base"),
      pkgapply::test::fixture::regular_entry("etc/tool.conf", 2),
  });
}

pkgplan::installation_plan
installation()
{
  const pkgapply::test::fixture::planning_authorities authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(2));
  const auto config = pkgplan::package_path::parse("etc/tool.conf");
  const auto policy = pkgapply::test::fixture::policy_snapshot(
      authorities,
      pkgapply::test::fixture::path_policy(),
      {pkgplan::path_policy_override(
          config,
          pkgapply::test::fixture::path_policy(
              pkgplan::incoming_path_policy::retain(
                  pkgplan::rejected_object_policy::stage,
                  pkgplan::retained_active_ownership_policy::
                      do_not_claim_operated_package)))});
  return pkgapply::test::fixture::installation_plan(
      authorities,
      {
          pkgapply::test::fixture::directory_entry("usr"),
          pkgapply::test::fixture::directory_entry("usr/bin"),
          pkgapply::test::fixture::regular_entry("usr/bin/base", 1),
          pkgapply::test::fixture::hardlink_entry(
              "usr/bin/tool", "usr/bin/base"),
          pkgapply::test::fixture::regular_entry("etc/tool.conf", 2),
      },
      {
          pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(
                  pkgplan::package_path::parse("etc"),
                  pkgapply::test::fixture::directory_object())),
          pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(
                  config,
                  pkgapply::test::fixture::regular_object(9))),
          pkgplan::target_path_observation::absent(
              pkgplan::package_path::parse("usr")),
          pkgplan::target_path_observation::absent(
              pkgplan::package_path::parse("usr/bin")),
          pkgplan::target_path_observation::absent(
              pkgplan::package_path::parse("usr/bin/base")),
          pkgplan::target_path_observation::absent(
              pkgplan::package_path::parse("usr/bin/tool")),
      },
      {},
      policy);
}

std::size_t
find_step(const pkgapply::application_effect_schedule& schedule,
          pkgapply::application_effect_step_kind kind,
          std::string_view path)
{
  for (std::size_t index = 0; index < schedule.steps().size(); ++index) {
    const auto& step = schedule.steps()[index];
    if (step.kind() == kind && step.path().string() == path)
      return index;
  }
  throw std::runtime_error("application effect step not found");
}

} // namespace

int
main()
{
  const auto image = incoming_image();
  const auto plan = installation();
  const auto payloads = pkgapply::prepare_incoming_payloads(plan, image);
  const pkgapply::old_object_capture_plan captures({
      pkgapply::old_object_capture_request(
          pkgplan::package_path::parse("etc/tool.conf"), true, true),
  });

  const auto schedule = pkgapply::prepare_application_schedule(
      plan, image, payloads, captures);

  require(!schedule.steps().empty(), "application schedule is empty");
  for (std::size_t index = 0; index < schedule.steps().size(); ++index)
    require(schedule.steps()[index].ordinal() == index,
            "application schedule ordinals changed");

  require(schedule.steps()[0].kind() ==
              pkgapply::application_effect_step_kind::capture_old_object &&
              schedule.steps()[0].path().string() == "etc/tool.conf",
          "old-object capture did not lead the schedule");

  const std::size_t anchor_payload = find_step(
      schedule,
      pkgapply::application_effect_step_kind::stage_regular_payload,
      "usr/bin/base");
  require(schedule.steps()[anchor_payload].incoming_entry() ==
              pkgimage::entry_id{2},
          "regular payload step lost its image entry");

  std::size_t anchor_payload_count = 0;
  for (const auto& step : schedule.steps())
    if (step.kind() ==
            pkgapply::application_effect_step_kind::stage_regular_payload &&
        step.incoming_entry() == pkgimage::entry_id{2})
      ++anchor_payload_count;
  require(anchor_payload_count == 1,
          "hard-link anchor duplicated regular payload staging");

  const std::size_t config_payload = find_step(
      schedule,
      pkgapply::application_effect_step_kind::stage_regular_payload,
      "etc/tool.conf");
  const std::size_t rejected = find_step(
      schedule,
      pkgapply::application_effect_step_kind::publish_rejected_object,
      "etc/tool.conf");
  require(config_payload < rejected,
          "rejected publication preceded incoming payload staging");

  const std::size_t usr = find_step(
      schedule,
      pkgapply::application_effect_step_kind::publish_active_object,
      "usr");
  const std::size_t bin = find_step(
      schedule,
      pkgapply::application_effect_step_kind::publish_active_object,
      "usr/bin");
  const std::size_t base = find_step(
      schedule,
      pkgapply::application_effect_step_kind::publish_active_object,
      "usr/bin/base");
  const std::size_t tool = find_step(
      schedule,
      pkgapply::application_effect_step_kind::publish_active_object,
      "usr/bin/tool");
  require(rejected < usr && usr < bin && bin < base && base < tool,
          "incoming directory or hard-link dependency order changed");

  const std::size_t first_observation = find_step(
      schedule,
      pkgapply::application_effect_step_kind::observe_result,
      "etc");
  require(tool < first_observation,
          "result observation preceded active publication");
  require(schedule.steps().back().kind() ==
              pkgapply::application_effect_step_kind::observe_result,
          "application schedule did not end with observations");

  const auto child = pkgplan::package_path::parse("var/lib/tool/file");
  const auto parent = pkgplan::package_path::parse("var/lib/tool");
  const auto child_object = pkgapply::test::fixture::regular_object(1);
  const auto parent_object = pkgapply::test::fixture::directory_object();
  const pkgapply::test::fixture::planning_authorities removal_authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(4));
  const auto removal = pkgapply::test::fixture::removal_plan(
      removal_authorities,
      {
          pkgplan::installed_ownership_claim(
              parent,
              removal_authorities.installed_package,
              parent_object),
          pkgplan::installed_ownership_claim(
              child,
              removal_authorities.installed_package,
              child_object),
      },
      {
          pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(parent, parent_object)),
          pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(child, child_object)),
      });
  const auto removal_schedule = pkgapply::prepare_application_schedule(
      removal, pkgapply::old_object_capture_plan({}));
  require(find_step(
              removal_schedule,
              pkgapply::application_effect_step_kind::publish_active_object,
              "var/lib/tool/file") <
              find_step(
                  removal_schedule,
                  pkgapply::application_effect_step_kind::
                      publish_active_object,
                  "var/lib/tool"),
          "directory cleanup preceded descendant removal");

  bool rejected_ordinal = false;
  try {
    static_cast<void>(pkgapply::application_effect_schedule({
        pkgapply::application_effect_step(
            1,
            pkgapply::application_effect_step_kind::observe_result,
            pkgplan::package_path::parse("usr")),
    }));
  } catch (const std::invalid_argument&) {
    rejected_ordinal = true;
  }
  require(rejected_ordinal,
          "non-consecutive application schedule ordinals were accepted");

  bool missing_payload_entry = false;
  try {
    static_cast<void>(pkgapply::application_effect_step(
        0,
        pkgapply::application_effect_step_kind::stage_regular_payload,
        pkgplan::package_path::parse("usr/bin/base")));
  } catch (const std::invalid_argument&) {
    missing_payload_entry = true;
  }
  require(missing_payload_entry,
          "payload staging step without an incoming entry was accepted");

  return 0;
}
