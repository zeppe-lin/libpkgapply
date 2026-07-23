// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/backend.h>

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

  const auto rejected_incoming =
      pkgapply::backend_rejected_effect_request::stage_incoming(
          second, pkgimage::entry_id{9});
  require(rejected_incoming.outcome() ==
              pkgplan::planned_rejected_outcome::stage_incoming &&
          rejected_incoming.incoming_entry() == pkgimage::entry_id{9},
          "incoming rejected-object command changed");

  const auto rejected_old =
      pkgapply::backend_rejected_effect_request::stage_old(first);
  require(rejected_old.outcome() ==
              pkgplan::planned_rejected_outcome::stage_old &&
          !rejected_old.incoming_entry().has_value(),
          "old rejected-object command gained incoming authority");

  return 0;
}
