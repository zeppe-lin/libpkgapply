// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/incoming_package.h>
#include <libpkgapply/request.h>

#include "plan_fixture.h"

#include <cassert>
#include <string>
#include <utility>

namespace {

template<typename Function>
void reject(Function&& function, pkgapply::incoming_package_error_code code)
{
  try {
    function();
    assert(false);
  } catch (const pkgapply::incoming_package_error& error) {
    assert(error.code() == code);
  }
}

pkgapply::application_target_context target()
{
  const auto id = [](std::uint8_t value) {
    std::string text = "v1:sha256:";
    static constexpr char hex[] = "0123456789abcdef";
    for (std::size_t index = 0; index < 32; ++index) {
      const auto byte = static_cast<std::uint8_t>(value + index);
      text.push_back(hex[byte >> 4]);
      text.push_back(hex[byte & 15]);
    }
    return text;
  };
  pkgplan::sha256_digest_bytes bytes{};
  bytes.fill(1);
  return pkgapply::application_target_context::make(
      pkgplan::target_system_context_identity::from_sha256(bytes),
      pkgapply::managed_target_identity::parse(id(2)),
      pkgapply::root_view_identity::parse(id(3)),
      pkgapply::observation_backend_identity::parse(id(4)),
      pkgapply::mutation_backend_identity::parse(id(5)),
      pkgapply::mutation_exclusion_domain_identity::parse(id(6)),
      pkgapply::active_object_namespace_identity::parse(id(7)),
      pkgapply::rejected_object_store_identity::parse(id(8)),
      pkgapply::staging_namespace_identity::parse(id(9)),
      pkgapply::journal_namespace_identity::parse(id(10)),
      pkgapply::execution_capability_profile_identity::parse(id(11)));
}

pkgapply::application_execution_control control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::best_effort,
      pkgapply::application_durability_requirement::all_application_domains,
      pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

} // namespace

int main()
{
  using namespace pkgapply::test::fixture;

  const auto first = ordinary_installation_incoming();
  const auto second = ordinary_installation_incoming();
  assert(first.identity() == second.identity());
  assert(first.build().outcome() == pkgbuild::build_outcome::succeeded);
  assert(first.image().image().entries().size() == 1);
  assert(first.candidate().release().name() == "tool");

  reject([] {
    const auto source = build_request("1.0");
    const auto failure = pkgbuild::build_result::failed(
        source,
        pkgbuild::execution_evidence_identity::from_sha256(
            std::string(64, '8')),
        pkgbuild::failure_evidence_identity::from_sha256(
            std::string(64, '9')));
    (void)pkgapply::incoming_package_authority::admit(
        failure,
        inspected_image({regular_entry("tool", 7)}));
  }, pkgapply::incoming_package_error_code::build_result);

  reject([] {
    const auto entries = std::vector<pkgimage::package_entry>{
        regular_entry("tool", 7)};
    auto build = pkgbuild::build_result::succeeded(
        build_request("1.0"), build_payload(entries),
        pkgbuild::sealed_artifact::make(
            pkgbuild::artifact_encoding::package_tar_v1,
            pkgbuild::artifact_compression::none, 4096,
            pkgbuild::sha256_digest(std::string(64, '1'))),
        pkgbuild::execution_evidence_identity::from_sha256(
            std::string(64, '8')));
    (void)pkgapply::incoming_package_authority::admit(
        std::move(build), inspected_image(entries, archive_digest(50)));
  }, pkgapply::incoming_package_error_code::artifact_binding);

  reject([] {
    const auto expected = std::vector<pkgimage::package_entry>{
        regular_entry("tool", 7)};
    const auto observed = std::vector<pkgimage::package_entry>{
        regular_entry("tool", 8)};
    const auto digest = archive_digest();
    auto build = pkgbuild::build_result::succeeded(
        build_request("1.0"), build_payload(expected),
        pkgbuild::sealed_artifact::make(
            pkgbuild::artifact_encoding::package_tar_v1,
            pkgbuild::artifact_compression::none, 4096,
            pkgbuild::sha256_digest(sha256_hex(digest.string()))),
        pkgbuild::execution_evidence_identity::from_sha256(
            std::string(64, '8')));
    (void)pkgapply::incoming_package_authority::admit(
        std::move(build), inspected_image(observed, digest));
  }, pkgapply::incoming_package_error_code::payload_mismatch);

  const auto context = target();
  const planning_authorities authorities(context.target());
  const auto install_plan = ordinary_installation(authorities);
  reject([&] {
    (void)pkgapply::installation_application_request::make(
        install_plan, ordinary_upgrade_incoming(), context, control());
  }, pkgapply::incoming_package_error_code::plan_binding);

  return 0;
}
