// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/result.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, std::string_view message)
{ if (!condition) { std::cerr << message << '\n'; std::exit(1); } }

template<class Identity> Identity app_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:"; constexpr char h[] = "0123456789abcdef";
  for (std::size_t i = 0; i < 32; ++i) {
    auto b = static_cast<std::uint8_t>(value + i);
    text += h[(b >> 4) & 15]; text += h[b & 15];
  }
  return Identity::parse(text);
}
template<class Identity> Identity plan_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bytes[i] = static_cast<std::uint8_t>(value + i);
  return Identity::from_sha256(bytes);
}

pkgapply::application_target_context target()
{
  return pkgapply::application_target_context::make(
      plan_identity<pkgplan::target_system_context_identity>(1),
      app_identity<pkgapply::managed_target_identity>(2),
      app_identity<pkgapply::root_view_identity>(3),
      app_identity<pkgapply::observation_backend_identity>(4),
      app_identity<pkgapply::mutation_backend_identity>(5),
      app_identity<pkgapply::mutation_exclusion_domain_identity>(6),
      app_identity<pkgapply::active_object_namespace_identity>(7),
      app_identity<pkgapply::rejected_object_store_identity>(8),
      app_identity<pkgapply::staging_namespace_identity>(9),
      app_identity<pkgapply::journal_namespace_identity>(10),
      app_identity<pkgapply::execution_capability_profile_identity>(11));
}

pkgapply::application_execution_control control()
{
  return pkgapply::application_execution_control::make(
      pkgapply::application_recovery_requirement::best_effort,
      pkgapply::application_durability_requirement::all_application_domains,
      pkgapply::application_cancellation_policy::recover_after_target_mutation);
}

pkgapply::application_durability_profile durability(
    pkgapply::application_durability_status completed =
        pkgapply::application_durability_status::confirmed)
{
  using D = pkgapply::application_durability_domain;
  using S = pkgapply::application_durability_status;
  return pkgapply::application_durability_profile({
      {D::journal, S::confirmed}, {D::incoming_staging, S::confirmed},
      {D::recovery_staging, S::confirmed}, {D::active_namespace, S::confirmed},
      {D::rejected_object_store, S::confirmed}, {D::completed_evidence, completed},
  });
}

pkgapply::completed_object_fact directory(const pkgplan::package_path& path)
{
  return pkgapply::completed_object_fact(
      path, pkgapply::completed_object_kind::directory,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::unknown(),
      pkgapply::qualified_fact<pkgapply::completed_regular_content_identity>::not_applicable(),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_hardlink_relation>::not_applicable(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete);
}

pkgapply::application_path_consequence path_result(
    pkgapply::application_effect_status active =
        pkgapply::application_effect_status::completed,
    pkgimage::entry_id incoming_entry = 0,
    pkgapply::ownership_publication_status publication =
        pkgapply::ownership_publication_status::eligible)
{
  auto path = pkgplan::package_path::parse("usr/share/tool");
  return pkgapply::application_path_consequence(
      path, pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      incoming_entry, pkgplan::path_ownership_transition({}, {}, true), active,
      pkgapply::application_effect_status::not_attempted,
      pkgapply::application_path_observation::absent(path),
      pkgapply::application_path_observation::present(directory(path)),
      std::nullopt, publication);
}

pkgplan::installation_plan installation_plan(
    pkgplan::operation_plan_identity identity,
    const pkgapply::application_target_context& context)
{
  const auto path = pkgplan::package_path::parse("usr/share/tool");
  return pkgplan::installation_plan(
      std::move(identity), context.target(),
      {pkgplan::installation_path_decision(
          path, pkgplan::installation_path_role::incoming_entry,
          pkgplan::planned_active_outcome::activate_incoming,
          pkgplan::planned_rejected_outcome::none,
          pkgimage::entry_id{0},
          pkgplan::path_ownership_transition({}, {}, true))});
}
}

int main()
{
  auto context = target(); auto execution = control();
  auto plan_id = plan_identity<pkgplan::operation_plan_identity>(30);
  auto request = pkgapply::installation_application_request::make(
      installation_plan(plan_id, context), context, execution);
  auto evidence = pkgapply::completed_application_evidence::installation(
      request, app_identity<pkgapply::application_attempt_identity>(40),
      app_identity<pkgapply::lease_bound_state_projection_identity>(41),
      app_identity<pkgapply::application_journal_identity>(42),
      {path_result()}, durability(),
      {app_identity<pkgapply::application_backend_evidence_identity>(43)});
  auto receipt = pkgapply::application_receipt::completed(
      evidence, pkgapply::application_recovery_state::recovery_assets_retained);

  require(receipt.outcome() == pkgapply::application_attempt_outcome::completed,
          "completed receipt outcome changed");
  require(receipt.kind() == pkgplan::operation_kind::install,
          "completed receipt operation kind changed");
  require(receipt.completed_evidence().has_value(),
          "completed receipt lost completed evidence");
  require(evidence.identity().string() == "v1:sha256:3e22d8dd0327c02769ff7072bf322ec4324cbd7ffb4b94afd65f543d61d12b60",
          "completed evidence identity vector changed");
  require(receipt.identity().string() == "v1:sha256:cf611ca945e6175fdfcb63b3f03359d374a4ff4779ecefe272a4b69f53e075a7",
          "application receipt identity vector changed");
  require(evidence.identity().string() != receipt.identity().string(),
          "receipt and completed evidence identities must be distinct");

  auto failed = pkgapply::application_receipt::failed(
      request,
      app_identity<pkgapply::application_attempt_identity>(50),
      app_identity<pkgapply::lease_bound_state_projection_identity>(41),
      pkgapply::application_attempt_outcome::precondition_refused,
      pkgapply::application_recovery_state::unchanged, durability(), {},
      std::nullopt);
  require(!failed.completed_evidence().has_value(),
          "failed receipt must not contain completed evidence");
  require(failed.kind() == pkgplan::operation_kind::install,
          "failed receipt operation kind changed");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::completed_application_evidence::installation(
        request, app_identity<pkgapply::application_attempt_identity>(40),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        app_identity<pkgapply::application_journal_identity>(42),
        {path_result(pkgapply::application_effect_status::failed)}, durability()));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "failed path must not become completed evidence");

  rejected = false;
  try {
    static_cast<void>(pkgapply::completed_application_evidence::installation(
        request, app_identity<pkgapply::application_attempt_identity>(40),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        app_identity<pkgapply::application_journal_identity>(42), {},
        durability()));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "incomplete plan path universe became completed evidence");

  rejected = false;
  try {
    static_cast<void>(pkgapply::completed_application_evidence::installation(
        request, app_identity<pkgapply::application_attempt_identity>(40),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        app_identity<pkgapply::application_journal_identity>(42),
        {path_result(pkgapply::application_effect_status::completed, 1)},
        durability()));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "changed incoming entry became completed evidence");

  rejected = false;
  try {
    using D = pkgapply::application_durability_domain;
    using S = pkgapply::application_durability_status;
    static_cast<void>(pkgapply::completed_application_evidence::installation(
        request, app_identity<pkgapply::application_attempt_identity>(40),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        app_identity<pkgapply::application_journal_identity>(42),
        {path_result()}, pkgapply::application_durability_profile({
            {D::journal, S::confirmed},
            {D::incoming_staging, S::confirmed},
            {D::recovery_staging, S::confirmed},
            {D::active_namespace, S::unconfirmed},
            {D::rejected_object_store, S::confirmed},
            {D::completed_evidence, S::confirmed},
        })));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "unconfirmed required durability became completed evidence");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_receipt::failed(
        request,
        app_identity<pkgapply::application_attempt_identity>(50),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        pkgapply::application_attempt_outcome::failed_with_partial_effects,
        pkgapply::application_recovery_state::known_residual_effects,
        durability(), {}, std::nullopt));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "post-mutation failure without journal must be rejected");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_receipt::failed(
        request, app_identity<pkgapply::application_attempt_identity>(50),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        pkgapply::application_attempt_outcome::precondition_refused,
        pkgapply::application_recovery_state::unchanged,
        durability(), {path_result()}, std::nullopt));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "failed receipt retained publication-eligible path");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_durability_fact(
        static_cast<pkgapply::application_durability_domain>(0),
        pkgapply::application_durability_status::confirmed));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "invalid durability domain was accepted");
  return 0;
}
