// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/object_fact.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool condition, std::string_view message)
{
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main()
{
  const auto path = pkgplan::package_path::parse("usr/bin/tool");
  const auto content = pkgapply::completed_regular_content_identity::parse(
      "v1:sha256:0000000000000000000000000000000000000000000000000000000000000001");
  const pkgapply::completed_object_fact regular(
      path,
      pkgapply::completed_object_kind::regular,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(4),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::unknown(),
      pkgapply::qualified_fact<pkgapply::completed_regular_content_identity>::known(content),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_hardlink_relation>::unknown(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::partial);
  require(regular.kind() == pkgapply::completed_object_kind::regular,
          "regular object kind was not retained");
  require(regular.hardlink().state() == pkgapply::fact_state::unknown,
          "unknown hard-link relation was promoted");

  const pkgapply::completed_object_fact complete_regular(
      path,
      pkgapply::completed_object_kind::regular,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(4),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::known(
          {10, 0}),
      pkgapply::qualified_fact<
          pkgapply::completed_regular_content_identity>::known(content),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_hardlink_relation>::unknown(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete);
  require(complete_regular.hardlink().state() == pkgapply::fact_state::unknown,
          "complete regular fact invented a hard-link peer");
  require(complete_regular.completeness() ==
              pkgapply::object_fact_completeness::complete,
          "unknown regular hard-link relation forced partial completeness");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::completed_object_fact(
        path,
        pkgapply::completed_object_kind::directory,
        pkgapply::qualified_fact<std::uint32_t>::known(0755),
        pkgapply::qualified_fact<std::uint64_t>::known(0),
        pkgapply::qualified_fact<std::uint64_t>::known(0),
        pkgapply::qualified_fact<std::uint64_t>::known(4),
        pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::unknown(),
        pkgapply::qualified_fact<pkgapply::completed_regular_content_identity>::known(content),
        pkgapply::qualified_fact<std::string>::not_applicable(),
        pkgapply::qualified_fact<pkgapply::completed_device_number>::not_applicable(),
        pkgapply::qualified_fact<pkgapply::completed_hardlink_relation>::not_applicable(),
        pkgapply::object_fact_provenance::application_observation,
        pkgapply::object_fact_completeness::partial));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "directory with regular payload facts must be rejected");

  rejected = false;
  try {
    static_cast<void>(pkgapply::completed_object_fact(
        path,
        pkgapply::completed_object_kind::symlink,
        pkgapply::qualified_fact<std::uint32_t>::known(0777),
        pkgapply::qualified_fact<std::uint64_t>::known(0),
        pkgapply::qualified_fact<std::uint64_t>::known(0),
        pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
        pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::known({0, 1000000000U}),
        pkgapply::qualified_fact<pkgapply::completed_regular_content_identity>::not_applicable(),
        pkgapply::qualified_fact<std::string>::known("target"),
        pkgapply::qualified_fact<pkgapply::completed_device_number>::not_applicable(),
        pkgapply::qualified_fact<pkgapply::completed_hardlink_relation>::not_applicable(),
        pkgapply::object_fact_provenance::application_observation,
        pkgapply::object_fact_completeness::complete));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "invalid timestamp nanoseconds must be rejected");

  const auto require_invalid_enumeration_rejected =
      [&](pkgapply::completed_object_kind kind,
          pkgapply::object_fact_provenance provenance,
          pkgapply::object_fact_completeness completeness,
          std::string_view message) {
        bool invalid_rejected = false;
        try {
          static_cast<void>(pkgapply::completed_object_fact(
              path, kind,
              pkgapply::qualified_fact<std::uint32_t>::known(0755),
              pkgapply::qualified_fact<std::uint64_t>::known(0),
              pkgapply::qualified_fact<std::uint64_t>::known(0),
              pkgapply::qualified_fact<std::uint64_t>::known(4),
              pkgapply::qualified_fact<
                  pkgapply::completed_object_timestamp>::known({10, 0}),
              pkgapply::qualified_fact<
                  pkgapply::completed_regular_content_identity>::known(content),
              pkgapply::qualified_fact<std::string>::not_applicable(),
              pkgapply::qualified_fact<
                  pkgapply::completed_device_number>::not_applicable(),
              pkgapply::qualified_fact<
                  pkgapply::completed_hardlink_relation>::unknown(),
              provenance, completeness));
        } catch (const std::invalid_argument&) {
          invalid_rejected = true;
        }
        require(invalid_rejected, message);
      };

  require_invalid_enumeration_rejected(
      static_cast<pkgapply::completed_object_kind>(0xff),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete,
      "invalid completed object kind was accepted");
  require_invalid_enumeration_rejected(
      pkgapply::completed_object_kind::regular,
      static_cast<pkgapply::object_fact_provenance>(0xff),
      pkgapply::object_fact_completeness::complete,
      "invalid object fact provenance was accepted");
  require_invalid_enumeration_rejected(
      pkgapply::completed_object_kind::regular,
      pkgapply::object_fact_provenance::application_observation,
      static_cast<pkgapply::object_fact_completeness>(0xff),
      "invalid object fact completeness was accepted");
  return 0;
}
