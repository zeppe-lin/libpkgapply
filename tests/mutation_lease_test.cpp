// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/mutation_lease.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

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

pkgapply::application_target_context
target()
{
  return pkgapply::application_target_context::make(
      planning_identity<pkgplan::target_system_context_identity>(1),
      application_identity<pkgapply::managed_target_identity>(2),
      application_identity<pkgapply::root_view_identity>(3),
      application_identity<pkgapply::observation_backend_identity>(4),
      application_identity<pkgapply::mutation_backend_identity>(5),
      application_identity<pkgapply::mutation_exclusion_domain_identity>(6),
      application_identity<pkgapply::active_object_namespace_identity>(7),
      application_identity<pkgapply::rejected_object_store_identity>(8),
      application_identity<pkgapply::staging_namespace_identity>(9),
      application_identity<pkgapply::journal_namespace_identity>(10),
      application_identity<
          pkgapply::execution_capability_profile_identity>(11));
}

pkgapply::lease_bound_state_projection
state(const pkgapply::mutation_lease_instance_identity& lease)
{
  return pkgapply::lease_bound_state_projection::make(
      lease,
      planning_identity<pkgplan::installed_state_snapshot_identity>(20),
      planning_identity<pkgplan::ownership_inventory_identity>(21),
      pkgapply::state_projection_completeness::complete,
      {},
      application_identity<pkgapply::state_projection_evidence_identity>(22));
}

class fake_lease final : public pkgapply::target_mutation_lease {
public:
  fake_lease(pkgapply::mutation_lease_instance_identity identity,
             pkgapply::application_target_context_identity target,
             pkgapply::mutation_exclusion_domain_identity domain,
             bool held)
      : identity_(std::move(identity)), target_(std::move(target)),
        domain_(std::move(domain)), held_(held)
  {
  }

  const pkgapply::mutation_lease_instance_identity&
  identity() const noexcept override
  {
    return identity_;
  }

  const pkgapply::application_target_context_identity&
  target() const noexcept override
  {
    return target_;
  }

  const pkgapply::mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept override
  {
    return domain_;
  }

  bool held() const noexcept override
  {
    return held_;
  }

private:
  pkgapply::mutation_lease_instance_identity identity_;
  pkgapply::application_target_context_identity target_;
  pkgapply::mutation_exclusion_domain_identity domain_;
  bool held_;
};

} // namespace

int
main()
{
  const auto context = target();
  const auto lease_identity =
      application_identity<pkgapply::mutation_lease_instance_identity>(30);
  const auto projection = state(lease_identity);
  const fake_lease lease(
      lease_identity,
      context.identity(),
      context.mutation_exclusion_domain(),
      true);

  pkgapply::validate_target_mutation_lease(context, projection, lease);

  bool rejected = false;
  try {
    const fake_lease released(
        lease_identity,
        context.identity(),
        context.mutation_exclusion_domain(),
        false);
    pkgapply::validate_target_mutation_lease(context, projection, released);
  } catch (const pkgapply::mutation_lease_error& error) {
    rejected = error.code() == pkgapply::mutation_lease_error_code::not_held;
  }
  require(rejected, "released mutation lease was accepted");

  rejected = false;
  try {
    const fake_lease foreign_target(
        lease_identity,
        application_identity<pkgapply::application_target_context_identity>(31),
        context.mutation_exclusion_domain(),
        true);
    pkgapply::validate_target_mutation_lease(
        context, projection, foreign_target);
  } catch (const pkgapply::mutation_lease_error& error) {
    rejected = error.code() ==
        pkgapply::mutation_lease_error_code::target_context_mismatch;
  }
  require(rejected, "foreign target mutation lease was accepted");

  rejected = false;
  try {
    const fake_lease foreign_domain(
        lease_identity,
        context.identity(),
        application_identity<
            pkgapply::mutation_exclusion_domain_identity>(32),
        true);
    pkgapply::validate_target_mutation_lease(
        context, projection, foreign_domain);
  } catch (const pkgapply::mutation_lease_error& error) {
    rejected = error.code() ==
        pkgapply::mutation_lease_error_code::exclusion_domain_mismatch;
  }
  require(rejected, "foreign exclusion-domain lease was accepted");

  rejected = false;
  try {
    const fake_lease another_acquisition(
        application_identity<pkgapply::mutation_lease_instance_identity>(33),
        context.identity(),
        context.mutation_exclusion_domain(),
        true);
    pkgapply::validate_target_mutation_lease(
        context, projection, another_acquisition);
  } catch (const pkgapply::mutation_lease_error& error) {
    rejected = error.code() ==
        pkgapply::mutation_lease_error_code::state_projection_mismatch;
  }
  require(rejected, "state projection from another lease was accepted");

  return 0;
}
