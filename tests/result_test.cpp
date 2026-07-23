// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/result.h>

#include "plan_fixture.h"

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
    const pkgplan::installation_path_decision& decision,
    pkgapply::application_effect_status active =
        pkgapply::application_effect_status::completed,
    std::optional<pkgimage::entry_id> incoming_entry_override = std::nullopt,
    pkgapply::ownership_publication_status publication =
        pkgapply::ownership_publication_status::eligible)
{
  const auto incoming_entry = incoming_entry_override
      ? incoming_entry_override
      : decision.incoming_entry();
  return pkgapply::application_path_consequence(
      decision.path(),
      pkgapply::application_path_role::incoming_entry,
      decision.active(),
      decision.rejected(),
      incoming_entry,
      decision.ownership(),
      active,
      pkgapply::application_effect_status::not_attempted,
      pkgapply::application_path_observation::absent(decision.path()),
      pkgapply::application_path_observation::present(
          directory(decision.path())),
      std::nullopt,
      publication);
}

pkgapply::application_path_consequence rejected_path_result(
    const pkgplan::installation_path_decision& decision,
    pkgapply::application_effect_status rejected,
    std::optional<pkgapply::rejected_object_record_identity> record)
{
  const auto observed = pkgapply::application_path_observation::present(
      directory(decision.path()));
  return pkgapply::application_path_consequence(
      decision.path(), pkgapply::application_path_role::incoming_entry,
      decision.active(), decision.rejected(), decision.incoming_entry(),
      decision.ownership(), pkgapply::application_effect_status::not_attempted,
      rejected, observed, observed, std::move(record),
      pkgapply::ownership_publication_status::ineligible);
}

pkgapply::application_durability_profile rejected_durability(
    pkgapply::application_durability_status rejected)
{
  using D = pkgapply::application_durability_domain;
  using S = pkgapply::application_durability_status;
  return pkgapply::application_durability_profile({
      {D::journal, S::confirmed},
      {D::incoming_staging, S::confirmed},
      {D::recovery_staging, S::not_attempted},
      {D::active_namespace, S::not_attempted},
      {D::rejected_object_store, rejected},
      {D::completed_evidence, S::not_attempted},
  });
}

}

int main()
{
  auto context = target(); auto execution = control();
  const auto path = pkgplan::package_path::parse("tool");
  const pkgapply::test::fixture::planning_authorities authorities(
      context.target());
  const auto plan = pkgapply::test::fixture::installation_plan(
      authorities,
      {pkgapply::test::fixture::directory_entry("tool")},
      {pkgplan::target_path_observation::absent(path)});
  const auto& decision = plan.paths().front();
  auto request = pkgapply::installation_application_request::make(
      plan, context, execution);
  auto evidence = pkgapply::completed_application_evidence::installation(
      request, app_identity<pkgapply::application_attempt_identity>(40),
      app_identity<pkgapply::lease_bound_state_projection_identity>(41),
      app_identity<pkgapply::application_journal_identity>(42),
      {path_result(decision)}, durability(),
      {app_identity<pkgapply::application_backend_evidence_identity>(43)});
  auto receipt = pkgapply::application_receipt::completed(
      evidence, pkgapply::application_recovery_state::recovery_assets_retained);

  require(receipt.outcome() == pkgapply::application_attempt_outcome::completed,
          "completed receipt outcome changed");
  require(receipt.kind() == pkgplan::operation_kind::install,
          "completed receipt operation kind changed");
  require(receipt.completed_evidence().has_value(),
          "completed receipt lost completed evidence");
  const auto repeated_evidence =
      pkgapply::completed_application_evidence::installation(
          request,
          app_identity<pkgapply::application_attempt_identity>(40),
          app_identity<pkgapply::lease_bound_state_projection_identity>(41),
          app_identity<pkgapply::application_journal_identity>(42),
          {path_result(decision)}, durability(),
          {app_identity<pkgapply::application_backend_evidence_identity>(43)});
  const auto repeated_receipt = pkgapply::application_receipt::completed(
      repeated_evidence,
      pkgapply::application_recovery_state::recovery_assets_retained);
  require(repeated_evidence.identity() == evidence.identity(),
          "identical completed evidence changed identity");
  require(repeated_receipt.identity() == receipt.identity(),
          "identical application receipts changed identity");
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

  const auto failed_command_path = pkgapply::application_path_consequence(
      decision.path(), pkgapply::application_path_role::incoming_entry,
      decision.active(), decision.rejected(), decision.incoming_entry(),
      decision.ownership(), pkgapply::application_effect_status::failed,
      pkgapply::application_effect_status::not_attempted,
      pkgapply::application_path_observation::absent(decision.path()),
      pkgapply::application_path_observation::absent(decision.path()),
      std::nullopt, pkgapply::ownership_publication_status::ineligible);
  const auto failed_command = pkgapply::application_receipt::failed(
      request, app_identity<pkgapply::application_attempt_identity>(51),
      app_identity<pkgapply::lease_bound_state_projection_identity>(41),
      pkgapply::application_attempt_outcome::failed_before_target_mutation,
      pkgapply::application_recovery_state::unchanged, durability(),
      {failed_command_path},
      app_identity<pkgapply::application_journal_identity>(52));
  require(failed_command.paths().front().active_status() ==
              pkgapply::application_effect_status::failed,
          "known failed active command was erased before mutation");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::completed_application_evidence::installation(
        request, app_identity<pkgapply::application_attempt_identity>(40),
        app_identity<pkgapply::lease_bound_state_projection_identity>(41),
        app_identity<pkgapply::application_journal_identity>(42),
        {path_result(decision, pkgapply::application_effect_status::failed)}, durability()));
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
        {path_result(decision, pkgapply::application_effect_status::completed, pkgimage::entry_id{1})},
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
        {path_result(decision)}, pkgapply::application_durability_profile({
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
        durability(), {path_result(decision)}, std::nullopt));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "failed receipt retained publication-eligible path");

  const auto rejected_path = pkgplan::package_path::parse("tool.conf");
  const auto rejected_policy = pkgapply::test::fixture::policy_snapshot(
      authorities, pkgapply::test::fixture::path_policy(
          pkgplan::incoming_path_policy::retain(
              pkgplan::rejected_object_policy::stage,
              pkgplan::retained_active_ownership_policy::
                  add_operated_owner)));
  const auto rejected_plan = pkgapply::test::fixture::installation_plan(
      authorities,
      {pkgapply::test::fixture::regular_entry("tool.conf", 7)},
      {pkgplan::target_path_observation::present(
          pkgplan::filesystem_object_fact(
              rejected_path,
              pkgapply::test::fixture::directory_object()))},
      {}, rejected_policy);
  const auto rejected_request =
      pkgapply::installation_application_request::make(
          rejected_plan, context, execution);
  const auto& rejected_decision = rejected_request.plan().paths().front();
  const auto rejected_record =
      app_identity<pkgapply::rejected_object_record_identity>(70);
  const auto rejected_consequence = rejected_path_result(
      rejected_decision, pkgapply::application_effect_status::completed,
      rejected_record);

  const auto rejected_failure = pkgapply::application_receipt::failed(
      rejected_request,
      app_identity<pkgapply::application_attempt_identity>(71),
      app_identity<pkgapply::lease_bound_state_projection_identity>(72),
      pkgapply::application_attempt_outcome::failed_before_target_mutation,
      pkgapply::application_recovery_state::unchanged,
      rejected_durability(pkgapply::application_durability_status::visible),
      {rejected_consequence},
      app_identity<pkgapply::application_journal_identity>(73));
  require(rejected_failure.paths().front().active_status() ==
              pkgapply::application_effect_status::not_attempted &&
              rejected_failure.paths().front().rejected_object() ==
                  rejected_record,
          "pre-active failure lost its independent rejected consequence");

  const auto visible_failure = pkgapply::application_receipt::failed(
      rejected_request,
      app_identity<pkgapply::application_attempt_identity>(74),
      app_identity<pkgapply::lease_bound_state_projection_identity>(72),
      pkgapply::application_attempt_outcome::
          effects_visible_durability_unconfirmed,
      pkgapply::application_recovery_state::recovery_assets_retained,
      rejected_durability(pkgapply::application_durability_status::visible),
      {rejected_consequence},
      app_identity<pkgapply::application_journal_identity>(73));
  require(visible_failure.durability().status(
              pkgapply::application_durability_domain::
                  rejected_object_store) ==
              pkgapply::application_durability_status::visible,
          "visible durability was promoted to confirmed");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_durability_fact(
        static_cast<pkgapply::application_durability_domain>(0),
        pkgapply::application_durability_status::confirmed));
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "invalid durability domain was accepted");
  return 0;
}
