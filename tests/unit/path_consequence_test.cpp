// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/path_consequence.h>

#include "fixtures/plan.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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

template<class Identity>
Identity
identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hexadecimal[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const auto byte = static_cast<std::uint8_t>(value + index);
    text += hexadecimal[(byte >> 4) & 0x0fU];
    text += hexadecimal[byte & 0x0fU];
  }
  return Identity::parse(text);
}

pkgapply::completed_object_fact
directory(const pkgplan::package_path& path)
{
  return pkgapply::completed_object_fact(
      path,
      pkgapply::completed_object_kind::directory,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::unknown(),
      pkgapply::qualified_fact<
          pkgapply::completed_regular_content_identity>::not_applicable(),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_hardlink_relation>::not_applicable(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete);
}

} // namespace

int
main()
{
  const pkgplan::package_path path =
      pkgplan::package_path::parse("tool");

  const pkgapply::test::fixture::planning_authorities authorities(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(1));
  const auto active_plan = pkgapply::test::fixture::installation_plan(
      authorities,
      {pkgapply::test::fixture::directory_entry("tool")},
      {pkgplan::target_path_observation::absent(path)});
  const auto& active_decision = active_plan.paths().front();

  const auto retained_object =
      pkgapply::test::fixture::regular_object(9);
  const auto staged_policy = pkgapply::test::fixture::policy_snapshot(
      authorities,
      pkgapply::test::fixture::path_policy(),
      {pkgplan::path_policy_override(
          path,
          pkgapply::test::fixture::path_policy(
              pkgplan::incoming_path_policy::retain(
                  pkgplan::rejected_object_policy::stage,
                  pkgplan::retained_active_ownership_policy::
                      do_not_claim_operated_package)))});
  const auto staged_plan = pkgapply::test::fixture::installation_plan(
      authorities,
      {pkgapply::test::fixture::regular_entry("tool", 7)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(path, retained_object))},
      {},
      staged_policy);
  const auto& staged_decision = staged_plan.paths().front();

  const pkgapply::application_path_consequence consequence(
      path,
      pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      *active_decision.incoming_entry(),
      active_decision.ownership(),
      pkgapply::application_effect_status::completed,
      pkgapply::application_effect_status::not_attempted,
      pkgapply::application_path_observation::absent(path),
      pkgapply::application_path_observation::present(directory(path)),
      std::nullopt,
      pkgapply::ownership_publication_status::eligible);

  require(consequence.incoming_entry() == pkgimage::entry_id{0},
          "incoming entry binding changed");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::structural_parent,
        pkgplan::planned_active_outcome::retain_observed,
        pkgplan::planned_rejected_outcome::none,
        *active_decision.incoming_entry(),
        active_decision.ownership(),
        pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::present(directory(path)),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "non-incoming path accepted an incoming entry");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::incoming_entry,
        staged_decision.active(),
        staged_decision.rejected(),
        *staged_decision.incoming_entry(),
        staged_decision.ownership(),
        pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::completed,
        pkgapply::application_path_observation::absent(path),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "completed rejected consequence accepted no record");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::incoming_entry,
        pkgplan::planned_active_outcome::activate_incoming,
        pkgplan::planned_rejected_outcome::none,
        *active_decision.incoming_entry(),
        active_decision.ownership(),
        pkgapply::application_effect_status::conditional_retained,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::absent(path),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "non-directory effect accepted conditional retention");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::incoming_entry,
        pkgplan::planned_active_outcome::activate_incoming,
        pkgplan::planned_rejected_outcome::none,
        *active_decision.incoming_entry(),
        active_decision.ownership(),
        pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::absent(path),
        pkgapply::application_path_observation::unknown(path),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "publication accepted unknown resulting observation");

  const auto rejected_record =
      identity<pkgapply::rejected_object_record_identity>(80);
  const pkgapply::application_path_consequence staged(
      path,
      pkgapply::application_path_role::incoming_entry,
      staged_decision.active(),
      staged_decision.rejected(),
      *staged_decision.incoming_entry(),
      staged_decision.ownership(),
      pkgapply::application_effect_status::completed,
      pkgapply::application_effect_status::completed,
      pkgapply::application_path_observation::absent(path),
      pkgapply::application_path_observation::present(directory(path)),
      rejected_record,
      pkgapply::ownership_publication_status::eligible);
  require(staged.rejected_object() == rejected_record,
          "completed rejected record binding changed");

  const auto require_invalid_consequence_rejected =
      [&](pkgapply::application_path_role role,
          pkgplan::planned_active_outcome active,
          pkgplan::planned_rejected_outcome rejected_plan,
          pkgapply::application_effect_status active_status,
          pkgapply::ownership_publication_status publication,
          std::string_view message) {
        bool invalid_rejected = false;
        try {
          static_cast<void>(pkgapply::application_path_consequence(
              path, role, active, rejected_plan,
              *active_decision.incoming_entry(), active_decision.ownership(),
              active_status, pkgapply::application_effect_status::not_attempted,
              pkgapply::application_path_observation::absent(path),
              pkgapply::application_path_observation::present(directory(path)),
              std::nullopt, publication));
        } catch (const std::invalid_argument&) {
          invalid_rejected = true;
        }
        require(invalid_rejected, message);
      };

  require_invalid_consequence_rejected(
      static_cast<pkgapply::application_path_role>(0xff),
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      pkgapply::application_effect_status::completed,
      pkgapply::ownership_publication_status::eligible,
      "invalid path role was accepted");
  require_invalid_consequence_rejected(
      pkgapply::application_path_role::incoming_entry,
      static_cast<pkgplan::planned_active_outcome>(0xff),
      pkgplan::planned_rejected_outcome::none,
      pkgapply::application_effect_status::completed,
      pkgapply::ownership_publication_status::eligible,
      "invalid planned active outcome was accepted");
  require_invalid_consequence_rejected(
      pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      static_cast<pkgplan::planned_rejected_outcome>(0xff),
      pkgapply::application_effect_status::completed,
      pkgapply::ownership_publication_status::eligible,
      "invalid planned rejected outcome was accepted");
  require_invalid_consequence_rejected(
      pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      static_cast<pkgapply::application_effect_status>(0xff),
      pkgapply::ownership_publication_status::ineligible,
      "invalid active effect status was accepted");
  require_invalid_consequence_rejected(
      pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      pkgapply::application_effect_status::completed,
      static_cast<pkgapply::ownership_publication_status>(0xff),
      "invalid publication status was accepted");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path, pkgapply::application_path_role::incoming_entry,
        staged_decision.active(), staged_decision.rejected(),
        *staged_decision.incoming_entry(), staged_decision.ownership(),
        pkgapply::application_effect_status::not_attempted,
        static_cast<pkgapply::application_effect_status>(0xff),
        pkgapply::application_path_observation::present(directory(path)),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt, pkgapply::ownership_publication_status::ineligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "invalid rejected effect status was accepted");

  return 0;
}
