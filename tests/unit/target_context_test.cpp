// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/target_context.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
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
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const std::uint8_t byte = static_cast<std::uint8_t>(value + index);
    text.push_back(hex[(byte >> 4) & 0x0f]);
    text.push_back(hex[byte & 0x0f]);
  }
  return Identity::parse(text);
}

pkgapply::application_target_context
make_context(bool executor = false)
{
  std::array<std::uint8_t, 32> target_bytes{};
  for (std::size_t index = 0; index < target_bytes.size(); ++index)
    target_bytes[index] = static_cast<std::uint8_t>(index + 1);

  return pkgapply::application_target_context::make(
      pkgplan::target_system_context_identity::from_sha256(target_bytes),
      identity<pkgapply::managed_target_identity>(2),
      identity<pkgapply::root_view_identity>(3),
      identity<pkgapply::observation_backend_identity>(4),
      identity<pkgapply::mutation_backend_identity>(5),
      identity<pkgapply::mutation_exclusion_domain_identity>(6),
      identity<pkgapply::active_object_namespace_identity>(7),
      identity<pkgapply::rejected_object_store_identity>(8),
      identity<pkgapply::staging_namespace_identity>(9),
      identity<pkgapply::journal_namespace_identity>(10),
      identity<pkgapply::execution_capability_profile_identity>(11),
      executor
          ? std::optional<pkgapply::lifecycle_executor_identity>(
                identity<pkgapply::lifecycle_executor_identity>(12))
          : std::nullopt);
}

} // namespace

int
main()
{
  const auto context = make_context();
  const auto equal = make_context();
  const auto with_executor = make_context(true);

  require(context == equal, "equal target contexts must compare equal");
  require(context != with_executor,
          "executor presence must change target context identity");
  require(context.schema_version() == 1,
          "target context schema version changed");
  require(!context.lifecycle_executor().has_value(),
          "schema 1 must represent absent lifecycle executor explicitly");
  require(context.identity().string() ==
              "v1:sha256:2835165126dcbe26d2c8be6e2c60e2169efea4a5ca1bd7df67d36b5585211b77",
          "target context identity vector changed");

  bool rejected = false;
  try {
    const auto same = identity<pkgapply::active_object_namespace_identity>(7);
    static_cast<void>(pkgapply::application_target_context::make(
        context.target(),
        context.managed_target(),
        context.root_view(),
        context.observation_backend(),
        context.mutation_backend(),
        context.mutation_exclusion_domain(),
        same,
        pkgapply::rejected_object_store_identity::parse(same.string()),
        context.staging_namespace(),
        context.journal_namespace(),
        context.capabilities()));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "aliased logical namespaces must be rejected");

  return 0;
}
