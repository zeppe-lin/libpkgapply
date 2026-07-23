// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/payload.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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
image()
{
  pkgimage::package_entry regular(
      pkgimage::package_path::parse("usr/lib/base"),
      pkgimage::entry_type::regular);
  pkgimage::package_entry hardlink(
      pkgimage::package_path::parse("usr/bin/tool"),
      pkgimage::entry_type::hardlink);
  hardlink.hardlink_target = pkgimage::package_path::parse("usr/lib/base");
  pkgimage::package_entry directory(
      pkgimage::package_path::parse("usr/share/tool"),
      pkgimage::entry_type::directory);
  return pkgimage::package_image(
      {std::move(regular), std::move(hardlink), std::move(directory)});
}

pkgplan::installation_plan
installation()
{
  return pkgplan::installation_plan(
      planning_identity<pkgplan::operation_plan_identity>(1),
      planning_identity<pkgplan::target_system_context_identity>(2),
      {
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/bin/tool"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{1},
              ownership()),
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/lib/base"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::retain_observed,
              pkgplan::planned_rejected_outcome::stage_incoming,
              pkgimage::entry_id{0},
              ownership()),
          pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/share/tool"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::activate_incoming,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{2},
              ownership()),
      });
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

  require(payload.requirements()[0].path().string() == "usr/bin/tool" &&
              payload.requirements()[0].image_entry() == 1 &&
              payload.requirements()[0].regular_payload_entry() ==
                  pkgimage::entry_id{0} &&
              payload.requirements()[0].required_for_active() &&
              !payload.requirements()[0].required_for_rejected(),
          "hard-link payload anchor was not retained");

  require(payload.requirements()[1].image_entry() == 0 &&
              payload.requirements()[1].regular_payload_entry() ==
                  pkgimage::entry_id{0} &&
              !payload.requirements()[1].required_for_active() &&
              payload.requirements()[1].required_for_rejected(),
          "rejected regular payload requirement changed");

  require(payload.requirements()[2].image_entry() == 2 &&
              !payload.requirements()[2].regular_payload_entry().has_value(),
          "non-regular incoming object incorrectly acquired payload bytes");

  const auto no_payload = pkgapply::prepare_incoming_payloads(
      pkgplan::installation_plan(
          planning_identity<pkgplan::operation_plan_identity>(3),
          planning_identity<pkgplan::target_system_context_identity>(4),
          {pkgplan::installation_path_decision(
              pkgplan::package_path::parse("usr/lib/base"),
              pkgplan::installation_path_role::incoming_entry,
              pkgplan::planned_active_outcome::retain_observed,
              pkgplan::planned_rejected_outcome::none,
              pkgimage::entry_id{0},
              ownership())}),
      package_image);
  require(no_payload.selection().size() == 0 &&
              no_payload.requirements().empty(),
          "unused incoming entry entered the replay closure");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::prepare_incoming_payloads(
        pkgplan::installation_plan(
            planning_identity<pkgplan::operation_plan_identity>(5),
            planning_identity<pkgplan::target_system_context_identity>(6),
            {pkgplan::installation_path_decision(
                pkgplan::package_path::parse("usr/bin/other"),
                pkgplan::installation_path_role::incoming_entry,
                pkgplan::planned_active_outcome::activate_incoming,
                pkgplan::planned_rejected_outcome::none,
                pkgimage::entry_id{1},
                ownership())}),
        package_image));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected,
          "incoming entry bound to another path entered payload closure");

  return 0;
}
