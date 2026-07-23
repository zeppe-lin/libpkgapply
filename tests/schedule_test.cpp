// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/schedule.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
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

pkgplan::path_ownership_transition
ownership()
{
  return pkgplan::path_ownership_transition({}, {}, true);
}

pkgimage::package_image
incoming_image()
{
  pkgimage::package_entry usr(
      pkgimage::package_path::parse("usr"),
      pkgimage::entry_type::directory);
  pkgimage::package_entry bin(
      pkgimage::package_path::parse("usr/bin"),
      pkgimage::entry_type::directory);
  pkgimage::package_entry base(
      pkgimage::package_path::parse("usr/bin/base"),
      pkgimage::entry_type::regular);
  pkgimage::package_entry tool(
      pkgimage::package_path::parse("usr/bin/tool"),
      pkgimage::entry_type::hardlink);
  tool.hardlink_target = pkgimage::package_path::parse("usr/bin/base");
  return pkgimage::package_image(
      {std::move(usr), std::move(bin), std::move(base), std::move(tool)});
}

pkgplan::installation_plan
installation()
{
  return pkgplan::installation_plan(
      planning_identity<pkgplan::operation_plan_identity>(1),
      planning_identity<pkgplan::target_system_context_identity>(2),
      {
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{0},
              ownership()),
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/bin"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{1},
              ownership()),
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/bin/base"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::stage_incoming,
              pkgimage::entry_id{2},
              ownership()),
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/bin/tool"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{3},
              ownership()),
      });
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

  const std::size_t payload = find_step(
      schedule,
      pkgapply::application_effect_step_kind::stage_regular_payload,
      "usr/bin/base");
  require(schedule.steps()[payload].incoming_entry() == pkgimage::entry_id{2},
          "regular payload step lost its image entry");

  std::size_t payload_count = 0;
  for (const auto& step : schedule.steps())
    if (step.kind() ==
        pkgapply::application_effect_step_kind::stage_regular_payload)
      ++payload_count;
  require(payload_count == 1,
          "hard-link anchor duplicated regular payload staging");

  const std::size_t rejected = find_step(
      schedule,
      pkgapply::application_effect_step_kind::publish_rejected_object,
      "usr/bin/base");
  require(payload < rejected,
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
      "usr");
  require(tool < first_observation,
          "result observation preceded active publication");
  require(schedule.steps().back().kind() ==
              pkgapply::application_effect_step_kind::observe_result,
          "application schedule did not end with observations");

  const auto child = pkgplan::package_path::parse("var/lib/tool/file");
  const auto parent = pkgplan::package_path::parse("var/lib/tool");
  const pkgplan::removal_plan removal(
      planning_identity<pkgplan::operation_plan_identity>(3),
      planning_identity<pkgplan::target_system_context_identity>(4),
      {
          pkgplan::removal_path_decision(
              parent,
              pkgplan::planned_active_outcome::remove_directory_if_empty,
              pkgplan::planned_rejected_outcome::none,
              ownership()),
          pkgplan::removal_path_decision(
              child,
              pkgplan::planned_active_outcome::remove_observed,
              pkgplan::planned_rejected_outcome::none,
              ownership()),
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
