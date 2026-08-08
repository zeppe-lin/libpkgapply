// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fixtures/plan.h"
#include "support/scripted_backend.h"

#include <libpkgapply/admission.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

pkgimage::complete_archive_digest
archive_digest(std::uint8_t value)
{
  pkgimage::sha256_digest_bytes bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return pkgimage::complete_archive_digest::from_sha256(bytes);
}

pkgapply::application_attempt_nonce
nonce()
{
  pkgapply::application_attempt_nonce::byte_array bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(70 + index);
  return pkgapply::application_attempt_nonce::from_bytes(bytes);
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

pkgapply::application_execution_control
control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::best_effort,
      pkgapply::application_durability_requirement::journal_and_recovery,
      pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

class fake_lease final : public pkgapply::target_mutation_lease {
public:
  fake_lease(pkgapply::mutation_lease_instance_identity identity,
             pkgapply::application_target_context_identity target,
             pkgapply::mutation_exclusion_domain_identity domain)
      : identity_(std::move(identity)),
        target_(std::move(target)),
        domain_(std::move(domain))
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

  bool
  held() const noexcept override
  {
    return true;
  }

private:
  pkgapply::mutation_lease_instance_identity identity_;
  pkgapply::application_target_context_identity target_;
  pkgapply::mutation_exclusion_domain_identity domain_;
};

class fake_archive final : public pkgimage::package_archive {
public:
  fake_archive(pkgimage::package_image image,
               pkgimage::complete_archive_digest digest)
      : image_(std::move(image)),
        receipt_(pkgimage::archive_backend_identity::parse(
                     "test/pkgimage-v1"),
                 std::move(digest),
                 image_.identity(),
                 image_.size())
  {
  }

  const pkgimage::package_image&
  image() const noexcept override
  {
    return image_;
  }

  const pkgimage::archive_inspection_receipt&
  inspection_receipt() const noexcept override
  {
    return receipt_;
  }

  void
  replay(const pkgimage::entry_selection&,
         pkgimage::payload_sink&) const override
  {
  }

private:
  pkgimage::package_image image_;
  pkgimage::archive_inspection_receipt receipt_;
};

pkgapply::lease_bound_state_projection
state(
    const fake_lease& lease,
    const pkgplan::operation_preconditions& preconditions,
    std::optional<pkgplan::installed_state_snapshot_identity> snapshot =
        std::nullopt,
    std::optional<std::vector<pkgplan::installed_package_identity>>
        first_path_owners = std::nullopt)
{
  std::vector<pkgapply::projected_path_owners> paths;
  paths.reserve(preconditions.paths().size());
  for (std::size_t index = 0; index < preconditions.paths().size(); ++index) {
    const auto& expected = preconditions.paths()[index];
    paths.emplace_back(
        expected.path(),
        index == 0 && first_path_owners
            ? *first_path_owners
            : expected.owners());
  }
  return pkgapply::lease_bound_state_projection::make(
      lease.identity(),
      snapshot ? std::move(*snapshot) : preconditions.installed_snapshot(),
      preconditions.ownership_inventory(),
      pkgapply::state_projection_completeness::complete,
      std::move(paths),
      application_identity<
          pkgapply::state_projection_evidence_identity>(30));
}

pkgapply::installation_application_request
installation_request(const pkgapply::application_target_context& context,
                     const fake_archive& archive)
{
  const auto path = pkgplan::package_path::parse("tool");
  const pkgapply::test::fixture::planning_authorities authorities(
      context.target());
  const auto plan = pkgapply::test::fixture::installation_plan(
      authorities,
      {pkgapply::test::fixture::regular_entry("tool", 7)},
      {pkgplan::target_path_observation::absent(path)},
      {},
      std::nullopt,
      archive.inspection_receipt().archive_digest());
  return pkgapply::installation_application_request::make(
      plan,
      pkgapply::test::fixture::incoming_package(
          archive.image().entries(),
          archive.inspection_receipt().archive_digest()),
      context, control());
}

template<class Function>
void
require_admission_error(Function&& function,
                        pkgapply::application_admission_error_code code,
                        std::string_view message)
{
  bool rejected = false;
  try {
    function();
  } catch (const pkgapply::application_admission_error& error) {
    rejected = error.code() == code;
  }
  require(rejected, message);
}

} // namespace

int
main()
{
  const auto context = target();
  fake_lease lease(
      application_identity<pkgapply::mutation_lease_instance_identity>(20),
      context.identity(),
      context.mutation_exclusion_domain());

  fake_archive archive(
      pkgimage::package_image({
          pkgapply::test::fixture::regular_entry("tool", 7),
      }),
      archive_digest(50));
  const auto request = installation_request(context, archive);
  const auto projection = state(lease, request.plan().preconditions());

  const auto backend_state =
      std::make_shared<pkgapply::test::scripted_backend_state>();
  pkgapply::test::scripted_backend backend(
      context.mutation_backend(),
      context.observation_backend(),
      context.capabilities(),
      nonce(),
      application_identity<
          pkgapply::application_backend_evidence_identity>(31),
      backend_state);

  pkgapply::validate_application_admission(
      request, projection, lease, backend, archive);

  require_admission_error(
      [&] {
        const auto stale = state(
            lease,
            request.plan().preconditions(),
            planning_identity<
                pkgplan::installed_state_snapshot_identity>(99));
        pkgapply::validate_application_admission(
            request, stale, lease, backend, archive);
      },
      pkgapply::application_admission_error_code::installed_snapshot_mismatch,
      "stale installed snapshot was admitted");

  require_admission_error(
      [&] {
        const auto wrong_owners = state(
            lease,
            request.plan().preconditions(),
            std::nullopt,
            std::vector<pkgplan::installed_package_identity>{
                planning_identity<pkgplan::installed_package_identity>(23),
            });
        pkgapply::validate_application_admission(
            request, wrong_owners, lease, backend, archive);
      },
      pkgapply::application_admission_error_code::state_path_owners_mismatch,
      "changed path owners were admitted");

  require_admission_error(
      [&] {
        pkgapply::test::scripted_backend foreign(
            application_identity<pkgapply::mutation_backend_identity>(80),
            context.observation_backend(),
            context.capabilities(),
            nonce(),
            application_identity<
                pkgapply::application_backend_evidence_identity>(81),
            std::make_shared<pkgapply::test::scripted_backend_state>());
        pkgapply::validate_application_admission(
            request, projection, lease, foreign, archive);
      },
      pkgapply::application_admission_error_code::backend_identity_mismatch,
      "foreign mutation backend was admitted");

  fake_archive different_archive(
      pkgimage::package_image({
          pkgapply::test::fixture::regular_entry("tool", 7),
      }),
      archive_digest(51));
  require_admission_error(
      [&] {
        pkgapply::validate_application_admission(
            request, projection, lease, backend, different_archive);
      },
      pkgapply::application_admission_error_code::archive_digest_mismatch,
      "different archive bytes were admitted");

  fake_archive different_image(
      pkgimage::package_image({
          pkgapply::test::fixture::regular_entry("other", 7),
      }),
      archive_digest(50));
  require_admission_error(
      [&] {
        pkgapply::validate_application_admission(
            request, projection, lease, backend, different_image);
      },
      pkgapply::application_admission_error_code::package_image_mismatch,
      "different package image was admitted");

  const auto removal_path = pkgplan::package_path::parse("tool");
  const auto old_object = pkgapply::test::fixture::regular_object(1, 0755);
  const pkgapply::test::fixture::planning_authorities removal_authorities(
      context.target());
  const auto removal_plan = pkgapply::test::fixture::removal_plan(
      removal_authorities,
      {pkgplan::installed_ownership_claim(
          removal_path,
          removal_authorities.installed_package,
          old_object)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(removal_path, old_object))});
  const auto removal_request =
      pkgapply::removal_application_request::make(
          removal_plan, context, control());
  const auto removal_projection =
      state(lease, removal_request.plan().preconditions());
  pkgapply::validate_application_admission(
      removal_request, removal_projection, lease, backend);

  auto transaction =
      backend.begin_with_incoming_image(
          pkgapply::package_application_request(request), lease,
          archive.image());
  pkgapply::validate_backend_transaction(
      context, lease, backend, *transaction);

  fake_lease another_lease(
      application_identity<pkgapply::mutation_lease_instance_identity>(90),
      context.identity(),
      context.mutation_exclusion_domain());
  require_admission_error(
      [&] {
        pkgapply::validate_backend_transaction(
            context, another_lease, backend, *transaction);
      },
      pkgapply::application_admission_error_code::transaction_lease_mismatch,
      "backend transaction from another lease was accepted");

  return 0;
}
