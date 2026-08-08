// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fixtures/checkpoint.h"
#include "fixtures/plan.h"

#include <libpkgapply/completed_evidence_codec.h>

#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace pkgapply;
namespace fixture = pkgapply::test::fixture;
namespace checkpoint = pkgapply::test::checkpoint_fixture;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

completed_object_fact directory(const pkgplan::package_path& path,
                                std::uint32_t mode = 0755)
{
  return completed_object_fact(
      path,
      completed_object_kind::directory,
      qualified_fact<std::uint32_t>::known(mode),
      qualified_fact<std::uint64_t>::known(0),
      qualified_fact<std::uint64_t>::known(0),
      qualified_fact<std::uint64_t>::not_applicable(),
      qualified_fact<completed_object_timestamp>::unknown(),
      qualified_fact<completed_regular_content_identity>::not_applicable(),
      qualified_fact<std::string>::not_applicable(),
      qualified_fact<completed_device_number>::not_applicable(),
      qualified_fact<completed_hardlink_relation>::not_applicable(),
      object_fact_provenance::application_observation,
      object_fact_completeness::complete);
}

template<class Decision>
application_path_role role(const Decision& decision)
{
  if constexpr (std::is_same_v<Decision, pkgplan::removal_path_decision>) {
    return application_path_role::installed_owned_path;
  }
  else if constexpr (std::is_same_v<Decision, pkgplan::installation_path_decision>) {
    return decision.role() == pkgplan::installation_path_role::incoming_entry
        ? application_path_role::incoming_entry
        : application_path_role::structural_parent;
  }
  else {
    switch (decision.role()) {
      case pkgplan::upgrade_path_role::incoming_entry:
        return application_path_role::incoming_entry;
      case pkgplan::upgrade_path_role::obsolete_old_path:
        return application_path_role::obsolete_old_path;
      case pkgplan::upgrade_path_role::structural_parent:
        return application_path_role::structural_parent;
    }
    throw std::logic_error("invalid upgrade path role");
  }
}

template<class Request>
std::vector<application_path_consequence>
completed_paths(const Request& request)
{
  std::vector<application_path_consequence> result;
  result.reserve(request.plan().paths().size());
  for (const auto& decision : request.plan().paths()) {
    const auto before = [&] {
      const auto& preconditions = request.plan().preconditions().paths();
      const auto item = std::lower_bound(
          preconditions.begin(), preconditions.end(), decision.path(),
          [](const auto& fact, const auto& path) {
            return fact.path() < path;
          });
      if (item == preconditions.end() || item->path() != decision.path())
        throw std::logic_error("fixture decision lacks precondition");
      if (!item->observation().is_present())
        return application_path_observation::absent(decision.path());
      return application_path_observation::present(directory(decision.path()));
    }();

    application_path_observation after =
        decision.active() == pkgplan::planned_active_outcome::remove_observed ||
            decision.active() == pkgplan::planned_active_outcome::remain_absent ||
            decision.active() ==
                pkgplan::planned_active_outcome::remove_directory_if_empty
        ? application_path_observation::absent(decision.path())
        : application_path_observation::present(directory(decision.path()));

    result.emplace_back(
        decision.path(),
        role(decision),
        decision.active(),
        decision.rejected(),
        [&]() -> std::optional<pkgimage::entry_id> {
          if constexpr (std::is_same_v<
                            std::decay_t<decltype(decision)>,
                            pkgplan::removal_path_decision>)
            return std::nullopt;
          else
            return decision.incoming_entry();
        }(),
        decision.ownership(),
        application_effect_status::completed,
        decision.rejected() == pkgplan::planned_rejected_outcome::none
            ? application_effect_status::not_attempted
            : application_effect_status::completed,
        before,
        std::move(after),
        std::nullopt,
        ownership_publication_status::eligible);
  }
  return result;
}

template<class Request>
completed_application_evidence make_completed(const Request& request,
                                               std::uint8_t seed)
{
  const auto attempt =
      checkpoint::application_identity<application_attempt_identity>(seed);
  const auto state = checkpoint::application_identity<
      lease_bound_state_projection_identity>(seed + 1);
  const auto journal =
      checkpoint::application_identity<application_journal_identity>(seed + 2);
  const auto backend = checkpoint::application_identity<
      application_backend_evidence_identity>(seed + 3);

  if constexpr (std::is_same_v<Request, installation_application_request>) {
    return completed_application_evidence::installation(
        request, attempt, state, journal, completed_paths(request),
        checkpoint::durability(), {backend});
  }
  else if constexpr (std::is_same_v<Request, upgrade_application_request>) {
    return completed_application_evidence::upgrade(
        request, attempt, state, journal, completed_paths(request),
        checkpoint::durability(), {backend});
  }
  else {
    return completed_application_evidence::removal(
        request, attempt, state, journal, completed_paths(request),
        checkpoint::durability(), {backend});
  }
}

template<class Request>
void check_round_trip(const Request& request, std::uint8_t seed)
{
  const auto value = make_completed(request, seed);
  const auto bytes = encode_completed_application_evidence(value);
  const auto decoded = decode_completed_application_evidence(bytes, request);
  require(decoded.identity() == value.identity() &&
              decoded.kind() == value.kind() &&
              decoded.request() == value.request() &&
              decoded.plan() == value.plan() &&
              decoded.attempt() == value.attempt() &&
              decoded.target() == value.target() &&
              decoded.control() == value.control() &&
              decoded.state_projection() == value.state_projection() &&
              decoded.journal() == value.journal() &&
              decoded.paths().size() == value.paths().size() &&
              decoded.durability() == value.durability() &&
              decoded.backend_evidence() == value.backend_evidence(),
          "completed-evidence codec changed semantic authority");
  require(encode_completed_application_evidence(decoded) == bytes,
          "completed-evidence encoding is not canonical");
}

template<class Request>
void check_corruption(const Request& request, std::uint8_t seed)
{
  auto bytes = encode_completed_application_evidence(make_completed(request, seed));
  require(bytes.size() > 48, "completed-evidence encoding is unexpectedly short");

  auto corrupted = bytes;
  corrupted[corrupted.size() / 2] ^= 0x80U;
  bool rejected = false;
  try {
    static_cast<void>(decode_completed_application_evidence(corrupted, request));
  }
  catch (const completed_application_evidence_codec_error& error) {
    rejected = error.code() ==
        completed_application_evidence_codec_error_code::checksum_mismatch ||
        error.code() ==
        completed_application_evidence_codec_error_code::identity_mismatch;
  }
  require(rejected, "completed-evidence codec accepted corruption");

  auto truncated = bytes;
  truncated.pop_back();
  rejected = false;
  try {
    static_cast<void>(decode_completed_application_evidence(truncated, request));
  }
  catch (const completed_application_evidence_codec_error& error) {
    rejected = error.code() ==
        completed_application_evidence_codec_error_code::truncated;
  }
  require(rejected, "completed-evidence codec accepted truncation");

  auto trailing = bytes;
  trailing.push_back(0);
  rejected = false;
  try {
    static_cast<void>(decode_completed_application_evidence(trailing, request));
  }
  catch (const completed_application_evidence_codec_error& error) {
    rejected = error.code() ==
        completed_application_evidence_codec_error_code::trailing_data;
  }
  require(rejected, "completed-evidence codec accepted trailing data");
}

} // namespace

int main()
{
  const auto target = checkpoint::target();
  const fixture::planning_authorities authorities(target.target());
  const auto path = pkgplan::package_path::parse("tool");

  const auto install = installation_application_request::make(
      fixture::installation_plan(
          authorities,
          {fixture::directory_entry("tool")},
          {pkgplan::target_path_observation::absent(path)}),
      fixture::incoming_package({fixture::directory_entry("tool")}),
      target,
      checkpoint::control());

  const auto old_directory = fixture::directory_object();
  const auto upgrade = upgrade_application_request::make(
      fixture::upgrade_plan(
          authorities,
          {fixture::directory_entry("tool")},
          {pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(path, old_directory))},
          {pkgplan::installed_ownership_claim(
              path, authorities.installed_package, old_directory)}),
      fixture::incoming_package(
          {fixture::directory_entry("tool")}, fixture::archive_digest(), "2.0"),
      target,
      checkpoint::control());

  const auto remove = removal_application_request::make(
      fixture::removal_plan(
          authorities,
          {pkgplan::installed_ownership_claim(
              path, authorities.installed_package, old_directory)},
          {pkgplan::target_path_observation::present(
              pkgplan::filesystem_object_fact(path, old_directory))}),
      target,
      checkpoint::control());

  check_round_trip(install, 30);
  check_round_trip(upgrade, 40);
  check_round_trip(remove, 50);
  check_corruption(install, 60);

  const auto foreign = checkpoint::request("other");
  const auto bytes = encode_completed_application_evidence(
      make_completed(install, 70));
  bool rejected = false;
  try {
    static_cast<void>(decode_completed_application_evidence(bytes, foreign));
  }
  catch (const completed_application_evidence_codec_error& error) {
    rejected = error.code() ==
        completed_application_evidence_codec_error_code::request_mismatch;
  }
  require(rejected, "completed-evidence codec accepted another request");

  return 0;
}
