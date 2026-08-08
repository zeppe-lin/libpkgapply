// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/backend.h>

#include "fixtures/plan.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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

template<class Function>
void
require_invalid(Function&& function, std::string_view message)
{
  bool rejected = false;
  try {
    function();
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, message);
}

} // namespace

int
main()
{
  const auto first = pkgplan::package_path::parse("etc/tool");
  const auto second = pkgplan::package_path::parse("usr/bin/tool");
  const auto evidence_a =
      identity<pkgapply::application_backend_evidence_identity>(10);
  const auto evidence_b =
      identity<pkgapply::application_backend_evidence_identity>(20);

  const auto observations = pkgapply::backend_observation_batch::make(
      {second, first},
      {
          pkgapply::application_path_observation::present(directory(first)),
          pkgapply::application_path_observation::absent(second),
      },
      {evidence_b, evidence_a});
  require(observations.requested().front() == first,
          "observation request paths were not normalized");
  require(observations.observations().front().path() == first,
          "observations were not normalized with the request closure");
  require(observations.find(second) != nullptr,
          "exact observation lookup failed");
  require(observations.evidence().front() == evidence_a,
          "backend evidence was not normalized");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_observation_batch::make(
            {first, first},
            {pkgapply::application_path_observation::absent(first),
             pkgapply::application_path_observation::absent(first)}));
      },
      "duplicate observation request paths were accepted");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_observation_batch::make(
            {first, second},
            {pkgapply::application_path_observation::absent(first)}));
      },
      "incomplete observation closure was accepted");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_observation_batch::make(
            {first},
            {pkgapply::application_path_observation::absent(second)}));
      },
      "mismatched observation closure was accepted");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_operation_result(
            pkgapply::backend_operation_outcome::completed,
            {evidence_a, evidence_a}));
      },
      "duplicate mechanism evidence was accepted");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_operation_result(
            static_cast<pkgapply::backend_operation_outcome>(255)));
      },
      "invalid mechanism outcome was accepted");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::old_object_capture_request(
            first, false, false));
      },
      "purposeless old-object capture was accepted");

  const pkgapply::old_object_capture_request capture(first, true, true);
  require(capture.for_rejected_object() && capture.for_recovery(),
          "old-object capture purposes were not retained");

  const pkgapply::old_object_capture_result captured(
      pkgapply::backend_operation_outcome::completed,
      pkgapply::application_path_observation::present(directory(first)),
      true,
      {evidence_a});
  require(captured.exact_recovery_possible(),
          "successful exact recovery capture was not retained");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::old_object_capture_result(
            pkgapply::backend_operation_outcome::completed,
            pkgapply::application_path_observation::absent(first),
            false));
      },
      "completed capture accepted an absent object");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::old_object_capture_result(
            pkgapply::backend_operation_outcome::failed,
            pkgapply::application_path_observation::unknown(first),
            true));
      },
      "failed capture claimed exact recoverability");

  const auto activate = pkgapply::backend_active_effect_request::make(
      second,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgimage::entry_id{7});
  require(activate.incoming_entry() == pkgimage::entry_id{7},
          "active incoming entry was not retained");

  const auto retain = pkgapply::backend_active_effect_request::make(
      first, pkgplan::planned_active_outcome::retain_observed);
  require(!retain.incoming_entry().has_value(),
          "retained active object gained an incoming entry");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_active_effect_request::make(
            second, pkgplan::planned_active_outcome::activate_incoming));
      },
      "incoming activation without an entry was accepted");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_active_effect_request::make(
            first,
            pkgplan::planned_active_outcome::remove_observed,
            pkgimage::entry_id{3}));
      },
      "non-incoming active effect accepted an entry");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::backend_active_effect_request::make(
            first,
            static_cast<pkgplan::planned_active_outcome>(255)));
      },
      "invalid planned active outcome was accepted");

  const pkgapply::test::fixture::planning_authorities incoming_authorities(
      identity<pkgplan::target_system_context_identity>(50));
  const auto incoming_path = pkgplan::package_path::parse("incoming.conf");
  const auto incoming_policy = pkgapply::test::fixture::policy_snapshot(
      incoming_authorities,
      pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::retain(
              pkgplan::rejected_object_policy::stage,
              pkgplan::retained_active_ownership_policy::
                  do_not_claim_operated_package)));
  const auto incoming_plan = pkgapply::test::fixture::installation_plan(
      incoming_authorities,
      {pkgapply::test::fixture::regular_entry(incoming_path.string(), 9)},
      {pkgplan::target_path_observation::absent(incoming_path)},
      {}, incoming_policy);
  const auto rejected_incoming =
      pkgapply::test::fixture::rejected_request(incoming_plan, incoming_path);
  require(rejected_incoming.outcome() ==
              pkgplan::planned_rejected_outcome::stage_incoming &&
          rejected_incoming.source_side() ==
              pkgplan::rejected_object_source_side::incoming &&
          rejected_incoming.reason() ==
              pkgplan::rejected_object_reason::install_policy_exclusion &&
          rejected_incoming.incoming_entry().has_value() &&
          rejected_incoming.artifact().has_value() &&
          rejected_incoming.artifact_manifest().has_value() &&
          rejected_incoming.image().has_value() &&
          !rejected_incoming.installed_package().has_value(),
          "incoming rejected-object command lost structured provenance");

  const pkgapply::test::fixture::planning_authorities old_authorities(
      identity<pkgplan::target_system_context_identity>(60));
  const auto old_object = pkgapply::test::fixture::regular_object(1);
  const auto old_policy = pkgapply::test::fixture::policy_snapshot(
      old_authorities,
      pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::activate(),
          pkgplan::obsolete_path_policy::remove(
              pkgplan::rejected_object_policy::stage)));
  const auto old_plan = pkgapply::test::fixture::removal_plan(
      old_authorities,
      {pkgplan::installed_ownership_claim(
          first, old_authorities.installed_package, old_object)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(first, old_object))},
      old_policy);
  const auto rejected_old =
      pkgapply::test::fixture::rejected_request(old_plan, first);
  require(rejected_old.outcome() ==
              pkgplan::planned_rejected_outcome::stage_old &&
          rejected_old.source_side() ==
              pkgplan::rejected_object_source_side::old_installed &&
          rejected_old.reason() ==
              pkgplan::rejected_object_reason::removal_old_preservation &&
          !rejected_old.incoming_entry().has_value() &&
          rejected_old.installed_package().has_value() &&
          rejected_old.installed_control().has_value() &&
          !rejected_old.artifact().has_value(),
          "old rejected-object command lost structured provenance");

  const auto rejected_record =
      identity<pkgapply::rejected_object_record_identity>(30);
  const pkgapply::rejected_object_publication_result rejected_completed(
      pkgapply::backend_operation_outcome::completed, rejected_record,
      {evidence_a});
  require(rejected_completed.record() == rejected_record,
          "completed rejected publication lost its immutable record");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::rejected_object_publication_result(
            pkgapply::backend_operation_outcome::completed, std::nullopt));
      },
      "completed rejected publication omitted its record");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::rejected_object_publication_result(
            pkgapply::backend_operation_outcome::failed, rejected_record));
      },
      "failed rejected publication retained a completed record");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::rejected_object_publication_result(
            pkgapply::backend_operation_outcome::conditional_retained,
            std::nullopt));
      },
      "rejected publication accepted a conditional outcome");

  const auto completed_record =
      identity<pkgapply::completed_application_evidence_identity>(40);
  const pkgapply::completed_evidence_publication_result evidence_completed(
      pkgapply::backend_operation_outcome::completed, completed_record,
      {evidence_a});
  require(evidence_completed.record() == completed_record,
          "completed evidence publication lost its exact record");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::completed_evidence_publication_result(
            pkgapply::backend_operation_outcome::completed, std::nullopt));
      },
      "completed evidence publication omitted its record");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::completed_evidence_publication_result(
            pkgapply::backend_operation_outcome::failed, completed_record));
      },
      "failed evidence publication retained a completed record");

  require_invalid(
      [&] {
        static_cast<void>(pkgapply::completed_evidence_publication_result(
            pkgapply::backend_operation_outcome::conditional_retained,
            std::nullopt));
      },
      "completed evidence publication accepted a conditional outcome");

  return 0;
}
