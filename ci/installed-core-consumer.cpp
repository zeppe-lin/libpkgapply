// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/libpkgapply.h>

#include <cstdint>
#include <string>

namespace {
template<class Identity>
Identity identity(std::uint8_t byte)
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string text = "v1:sha256:";
  for (std::size_t index = 0; index < 32; ++index) {
    const auto value = static_cast<std::uint8_t>(byte + index);
    text += hex[(value >> 4U) & 0x0fU];
    text += hex[value & 0x0fU];
  }
  return Identity::parse(text);
}
} // namespace

int main()
{
  const auto target = identity<pkgplan::target_system_context_identity>(1);
  const auto managed = identity<pkgapply::managed_target_identity>(2);
  const bool identity_api =
      pkgapply::managed_target_identity::canonical_domain() ==
          "pkgapply/managed-target-reference/v1" &&
      managed.algorithm() == pkgapply::digest_algorithm::sha256 &&
      managed.string().size() == 74 && managed.bytes()[0] == 2;

  const auto context = pkgapply::application_target_context::make(
      target,
      managed,
      identity<pkgapply::root_view_identity>(3),
      identity<pkgapply::observation_backend_identity>(4),
      identity<pkgapply::mutation_backend_identity>(5),
      identity<pkgapply::mutation_exclusion_domain_identity>(6),
      identity<pkgapply::active_object_namespace_identity>(7),
      identity<pkgapply::rejected_object_store_identity>(8),
      identity<pkgapply::staging_namespace_identity>(9),
      identity<pkgapply::journal_namespace_identity>(10),
      identity<pkgapply::execution_capability_profile_identity>(11));

  const pkgbuild::plan_adapter::projection_error dependency_probe(
      pkgbuild::plan_adapter::projection_error_code::planner_fact,
      "installed consumer dependency probe");

  return identity_api && context.target() == target &&
                 dependency_probe.code() ==
                     pkgbuild::plan_adapter::projection_error_code::planner_fact &&
                 pkgapply::version() == "4.0.0" &&
                 pkgapply::api_version == 4
             ? 0
             : 1;
}
