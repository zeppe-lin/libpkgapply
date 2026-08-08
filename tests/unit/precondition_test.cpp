// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/precondition.h>

#include "fixtures/plan.h"

#include <array>
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
application_identity(std::uint8_t value)
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

template<class Identity>
Identity
planning_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return Identity::from_sha256(bytes);
}

pkgplan::operation_preconditions
present_preconditions(const pkgplan::package_path& path)
{
  const auto metadata = pkgplan::filesystem_object_metadata(
      pkgplan::filesystem_object_kind::regular,
      0644,
      0,
      0,
      4,
      std::nullopt,
      planning_identity<pkgplan::filesystem_regular_content_identity>(40));
  const pkgapply::test::fixture::planning_authorities authorities(
      planning_identity<pkgplan::target_system_context_identity>(1));
  const auto plan = pkgapply::test::fixture::removal_plan(
      authorities,
      {pkgplan::installed_ownership_claim(
          path, authorities.installed_package, metadata)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(path, metadata))});
  return plan.preconditions();
}

pkgplan::operation_preconditions
absent_preconditions(const pkgplan::package_path& path)
{
  const pkgapply::test::fixture::planning_authorities authorities(
      planning_identity<pkgplan::target_system_context_identity>(1));
  const auto plan = pkgapply::test::fixture::installation_plan(
      authorities,
      {pkgapply::test::fixture::regular_entry(path.string(), 7)},
      {pkgplan::target_path_observation::absent(path)});
  return plan.preconditions();
}

pkgapply::completed_object_fact
regular(const pkgplan::package_path& path,
        std::uint32_t mode,
        pkgapply::qualified_fact<
            pkgapply::completed_regular_content_identity> content)
{
  return pkgapply::completed_object_fact(
      path,
      pkgapply::completed_object_kind::regular,
      pkgapply::qualified_fact<std::uint32_t>::known(mode),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(4),
      pkgapply::qualified_fact<
          pkgapply::completed_object_timestamp>::known({1234, 99}),
      std::move(content),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_hardlink_relation>::unknown(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::partial);
}

pkgapply::backend_observation_batch
batch(const pkgplan::package_path& path,
      pkgapply::application_path_observation observation)
{
  return pkgapply::backend_observation_batch::make(
      {path},
      {std::move(observation)},
      {application_identity<
          pkgapply::application_backend_evidence_identity>(50)});
}

} // namespace

int
main()
{
  const auto path = pkgplan::package_path::parse("tool");
  const auto expected = present_preconditions(path);
  const auto matching_content =
      application_identity<pkgapply::completed_regular_content_identity>(40);

  const auto satisfied = pkgapply::application_precondition_check::make(
      expected,
      batch(path, pkgapply::application_path_observation::present(
          regular(
              path,
              0644,
              pkgapply::qualified_fact<
                  pkgapply::completed_regular_content_identity>::known(
                      matching_content)))));
  require(satisfied.satisfied(),
          "richer matching current observation was rejected");
  require(satisfied.observations().evidence().size() == 1,
          "fresh observation evidence was not retained");

  const auto mode_mismatch = pkgapply::application_precondition_check::make(
      expected,
      batch(path, pkgapply::application_path_observation::present(
          regular(
              path,
              0600,
              pkgapply::qualified_fact<
                  pkgapply::completed_regular_content_identity>::known(
                      matching_content)))));
  require(!mode_mismatch.satisfied() &&
              mode_mismatch.failures().size() == 1 &&
              mode_mismatch.failures()[0].field() ==
                  pkgapply::application_precondition_field::mode &&
              mode_mismatch.failures()[0].kind() ==
                  pkgapply::application_precondition_failure_kind::mismatch,
          "mode mismatch was not classified precisely");

  const auto content_unknown = pkgapply::application_precondition_check::make(
      expected,
      batch(path, pkgapply::application_path_observation::present(
          regular(
              path,
              0644,
              pkgapply::qualified_fact<
                  pkgapply::completed_regular_content_identity>::unknown()))));
  require(!content_unknown.satisfied() &&
              content_unknown.failures().size() == 1 &&
              content_unknown.failures()[0].field() ==
                  pkgapply::application_precondition_field::regular_content &&
              content_unknown.failures()[0].kind() ==
                  pkgapply::application_precondition_failure_kind::unknown,
          "unknown required content identity was not classified");

  const auto expected_absent = absent_preconditions(path);
  const auto unexpected_present =
      pkgapply::application_precondition_check::make(
          expected_absent,
          batch(path, pkgapply::application_path_observation::present(
              regular(
                  path,
                  0644,
                  pkgapply::qualified_fact<
                      pkgapply::completed_regular_content_identity>::known(
                          matching_content)))));
  require(!unexpected_present.satisfied() &&
              unexpected_present.failures()[0].field() ==
                  pkgapply::application_precondition_field::presence &&
              unexpected_present.failures()[0].kind() ==
                  pkgapply::application_precondition_failure_kind::mismatch,
          "unexpected present object was not classified as stale");

  const auto unknown_presence = pkgapply::application_precondition_check::make(
      expected,
      batch(path, pkgapply::application_path_observation::unknown(path)));
  require(!unknown_presence.satisfied() &&
              unknown_presence.failures()[0].field() ==
                  pkgapply::application_precondition_field::presence &&
              unknown_presence.failures()[0].kind() ==
                  pkgapply::application_precondition_failure_kind::unknown,
          "unknown path presence was not classified");

  bool rejected = false;
  try {
    const auto another = pkgplan::package_path::parse("other");
    static_cast<void>(pkgapply::application_precondition_check::make(
        expected,
        batch(another, pkgapply::application_path_observation::absent(another))));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected,
          "fresh observation request for another path universe was accepted");

  return 0;
}
