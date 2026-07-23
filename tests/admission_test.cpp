// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scripted_backend.h"

#include <libpkgapply/admission.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
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
      : identity_(std::move(identity)), target_(std::move(target)),
        domain_(std::move(domain))
  {
  }

  const pkgapply::mutation_lease_instance_identity&
  identity() const noexcept override { return identity_; }
  const pkgapply::application_target_context_identity&
  target() const noexcept override { return target_; }
  const pkgapply::mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept override { return domain_; }
  bool held() const noexcept override { return true; }

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
        receipt_(pkgimage::archive_backend_identity::parse("test/archive-v1"),
                 std::move(digest),
                 image_.identity(),
                 image_.size())
  {
  }

  const pkgimage::package_image& image() const noexcept override
  { return image_; }
  const pkgimage::archive_inspection_receipt&
  inspection_receipt() const noexcept override
  { return receipt_; }
  void replay(const pkgimage::entry_selection&,
              pkgimage::payload_sink&) const override
  {
  }

private:
  pkgimage::package_image image_;
  pkgimage::archive_inspection_receipt receipt_;
};

pkgapply::lease_bound_state_projection
state(const fake_lease& lease,
      const pkgplan::installed_state_snapshot_identity& snapshot,
      const pkgplan::ownership_inventory_identity& ownership,
      const pkgplan::package_path& path,
      std::vector<pkgplan::installed_package_identity> owners)
{
  return pkgapply::lease_bound_state_projection::make(
      lease.identity(),
      snapshot,
      ownership,
      pkgapply::state_projection_completeness::complete,
      {pkgapply::projected_path_owners(path, std::move(owners))},
      application_identity<
          pkgapply::state_projection_evidence_identity>(30));
}

pkgapply::installation_application_request
installation_request(
    const pkgapply::application_target_context& context,
    const fake_archive& archive,
    const pkgplan::installed_state_snapshot_identity& snapshot,
    const pkgplan::ownership_inventory_identity& ownership,
    const pkgplan::package_path& plan_path,
    std::vector<pkgplan::installed_package_identity> owners)
{
  pkgplan::operation_preconditions preconditions(
      context.target(),
      snapshot,
      ownership,
      pkgplan::incoming_archive_precondition(
          archive.inspection_receipt().archive_digest(),
          archive.image().identity(),
          archive.inspection_receipt().identity()),
      {pkgplan::path_precondition(plan_path, owners)});
  pkgplan::installation_path_decision decision(
      plan_path,
      pkgplan::installation_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      pkgimage::entry_id{0},
      pkgplan::path_ownership_transition(owners, owners, true));
  return pkgapply::installation_application_request::make(
      pkgplan::installation_plan(
          planning_identity<pkgplan::operation_plan_identity>(40),
          std::move(preconditions),
          {std::move(decision)}),
      context,
      control());
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
  const auto lease_identity =
      application_identity<pkgapply::mutation_lease_instance_identity>(20);
  fake_lease lease(lease_identity,
                   context.identity(),
                   context.mutation_exclusion_domain());
  const auto snapshot =
      planning_identity<pkgplan::installed_state_snapshot_identity>(21);
  const auto ownership =
      planning_identity<pkgplan::ownership_inventory_identity>(22);
  const auto owner = planning_identity<pkgplan::installed_package_identity>(23);
  const auto path = pkgplan::package_path::parse("usr/bin/tool");

  fake_archive archive(
      pkgimage::package_image({pkgimage::package_entry(
          pkgimage::package_path::parse("usr/bin/tool"),
          pkgimage::entry_type::regular)}),
      archive_digest(50));
  const auto request = installation_request(
      context, archive, snapshot, ownership, path, {owner});
  const auto projection = state(lease, snapshot, ownership, path, {owner});

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
            planning_identity<pkgplan::installed_state_snapshot_identity>(99),
            ownership,
            path,
            {owner});
        pkgapply::validate_application_admission(
            request, stale, lease, backend, archive);
      },
      pkgapply::application_admission_error_code::installed_snapshot_mismatch,
      "stale installed snapshot was admitted");

  require_admission_error(
      [&] {
        const auto wrong_owners = state(
            lease, snapshot, ownership, path, {});
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
      pkgimage::package_image({pkgimage::package_entry(
          pkgimage::package_path::parse("usr/bin/tool"),
          pkgimage::entry_type::regular)}),
      archive_digest(51));
  require_admission_error(
      [&] {
        pkgapply::validate_application_admission(
            request, projection, lease, backend, different_archive);
      },
      pkgapply::application_admission_error_code::archive_digest_mismatch,
      "different archive bytes were admitted");

  const auto another_path = pkgplan::package_path::parse("usr/bin/other");
  const auto mismatched_request = installation_request(
      context,
      archive,
      snapshot,
      ownership,
      another_path,
      {owner});
  const auto mismatched_state = state(
      lease, snapshot, ownership, another_path, {owner});
  require_admission_error(
      [&] {
        pkgapply::validate_application_admission(
            mismatched_request,
            mismatched_state,
            lease,
            backend,
            archive);
      },
      pkgapply::application_admission_error_code::
          incoming_entry_path_mismatch,
      "incoming entry bound to another path was admitted");

  pkgplan::operation_preconditions removal_preconditions(
      context.target(), snapshot, ownership, std::nullopt,
      {pkgplan::path_precondition(path, {owner})});
  const auto removal_request = pkgapply::removal_application_request::make(
      pkgplan::removal_plan(
          planning_identity<pkgplan::operation_plan_identity>(60),
          std::move(removal_preconditions),
          {pkgplan::removal_path_decision(
              path,
              pkgplan::planned_active_outcome::remove_observed,
              pkgplan::planned_rejected_outcome::none,
              pkgplan::path_ownership_transition({owner}, {}, false))}),
      context,
      control());
  pkgapply::validate_application_admission(
      removal_request, projection, lease, backend);

  auto transaction =
      backend.begin_with_incoming_image(context, lease, archive.image());
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
