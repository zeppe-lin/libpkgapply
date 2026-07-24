// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scripted_backend.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
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

pkgimage::sha256_digest_bytes
image_bytes(std::uint8_t value)
{
  pkgimage::sha256_digest_bytes bytes{};
  bytes.fill(value);
  return bytes;
}

pkgimage::package_image
incoming_image()
{
  pkgimage::package_entry entry(
      pkgimage::package_path::parse("usr/bin/tool"),
      pkgimage::entry_type::regular);
  entry.mode = 0755;
  entry.uid = 0;
  entry.gid = 0;
  entry.size = 1;
  entry.regular_content =
      pkgimage::regular_content_digest::from_sha256(image_bytes(1));
  return pkgimage::package_image({std::move(entry)});
}

pkgapply::application_attempt_nonce
nonce()
{
  pkgapply::application_attempt_nonce::byte_array bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(90 + index);
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

pkgapply::completed_object_fact
directory(const pkgplan::package_path& path)
{
  return pkgapply::completed_object_fact(
      path,
      pkgapply::completed_object_kind::directory,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::unknown(),
      pkgapply::qualified_fact<
          pkgapply::completed_regular_content_identity>::not_applicable(),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_hardlink_relation>::not_applicable(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete);
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
    return true;
  }

private:
  pkgapply::mutation_lease_instance_identity identity_;
  pkgapply::application_target_context_identity target_;
  pkgapply::mutation_exclusion_domain_identity domain_;
};

pkgapply::application_journal_record
journal(const pkgapply::application_target_context& context,
        const fake_lease& lease,
        const pkgapply::application_attempt_nonce& attempt_nonce)
{
  const auto request =
      application_identity<pkgapply::application_request_identity>(40);
  const auto attempt = pkgapply::application_attempt::make(
      request, context.identity(), context.mutation_backend(), attempt_nonce);
  const auto header = pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install,
      request,
      planning_identity<pkgplan::operation_plan_identity>(41),
      attempt,
      context.identity(),
      application_identity<
          pkgapply::application_execution_control_identity>(42),
      application_identity<
          pkgapply::lease_bound_state_projection_identity>(43),
      lease.identity(),
      context.mutation_backend());
  return pkgapply::application_journal_record::make(
      header,
      pkgapply::application_journal_state::preparing,
      {},
      {});
}

} // namespace

int
main()
{
  const auto context = target();
  const auto lease_identity =
      application_identity<pkgapply::mutation_lease_instance_identity>(30);
  fake_lease lease(lease_identity,
                   context.identity(),
                   context.mutation_exclusion_domain());

  const auto state =
      std::make_shared<pkgapply::test::scripted_backend_state>();
  const auto path = pkgplan::package_path::parse("usr/bin/tool");
  state->set_observations({
      pkgapply::application_path_observation::present(directory(path))});

  pkgapply::test::scripted_backend backend(
      context.mutation_backend(),
      context.observation_backend(),
      context.capabilities(),
      nonce(),
      application_identity<
          pkgapply::application_backend_evidence_identity>(31),
      state);

  pkgimage::package_image image = incoming_image();
  const auto selection = pkgimage::entry_selection::all_regular(image);

  {
    auto transaction =
        backend.begin_with_incoming_image(context, lease, image);
    require(state->transaction_alive(),
            "scripted transaction lifetime was not retained");
    require(transaction->backend() == context.mutation_backend(),
            "scripted transaction changed backend identity");
    require(transaction->observation_backend() ==
                context.observation_backend(),
            "scripted transaction changed observation identity");
    require(transaction->capabilities() == context.capabilities(),
            "scripted transaction changed capability identity");
    require(transaction->target() == context.identity(),
            "scripted transaction changed target identity");
    require(transaction->lease() == lease.identity(),
            "scripted transaction changed lease identity");

    const auto observed = transaction->observe({path});
    require(observed.find(path) != nullptr,
            "scripted observation did not return configured fact");

    auto payload = transaction->begin_payload_stage(image, selection);
    const pkgimage::package_entry& entry = image.entries().front();
    const std::byte byte{0x42};
    payload->begin(entry);
    payload->write(entry, &byte, 1);
    payload->end(entry);
    const auto payload_result = payload->seal();
    require(payload_result.outcome() ==
                pkgapply::backend_operation_outcome::completed &&
            payload->sealed(),
            "scripted payload stage did not seal");

    const auto captured = transaction->capture_old(
        pkgapply::old_object_capture_request(path, true, true));
    require(captured.outcome() ==
                pkgapply::backend_operation_outcome::completed &&
            captured.exact_recovery_possible(),
            "scripted old-object capture did not retain recovery evidence");

    const auto active = transaction->execute_active(
        pkgapply::backend_active_effect_request::make(
            path, pkgplan::planned_active_outcome::retain_observed));
    require(active.outcome() ==
                pkgapply::backend_operation_outcome::completed,
            "scripted active effect did not complete");

    const auto rejected = transaction->execute_rejected(
        pkgapply::backend_rejected_effect_request::stage_old(path));
    require(rejected.outcome() ==
                pkgapply::backend_operation_outcome::completed &&
            rejected.record().has_value(),
            "scripted rejected effect did not publish a record");

    const auto recovery = transaction->recover(path);
    require(recovery.outcome() ==
                pkgapply::backend_operation_outcome::completed,
            "scripted recovery did not complete");

    const auto durability = transaction->synchronize(
        pkgapply::application_durability_domain::journal);
    require(durability.status() ==
                pkgapply::application_durability_status::confirmed,
            "scripted durability default changed");

    const auto record = journal(context, lease, transaction->attempt_nonce());
    const auto published = transaction->publish_journal(record);
    require(published.identity() == record.identity() &&
                state->published_journal()->identity() == record.identity(),
            "scripted journal publication changed the record");
  }

  require(!state->transaction_alive(),
          "scripted transaction destruction did not release lifetime state");
  require(!state->events().empty() &&
              state->events().front().boundary ==
                  pkgapply::test::scripted_backend_boundary::
                      begin_with_incoming_image &&
              state->events().back().boundary ==
                  pkgapply::test::scripted_backend_boundary::
                      transaction_destroyed,
          "scripted event order lost transaction boundaries");

  state->clear_events();
  state->set_outcome(
      pkgapply::test::scripted_backend_boundary::execute_active,
      pkgapply::backend_operation_outcome::failed);
  {
    auto transaction =
        backend.begin_without_incoming_image(context, lease);
    const auto failed = transaction->execute_active(
        pkgapply::backend_active_effect_request::make(
            path, pkgplan::planned_active_outcome::retain_observed));
    require(failed.outcome() ==
                pkgapply::backend_operation_outcome::failed,
            "scripted outcome injection was ignored");

    bool rejected = false;
    try {
      static_cast<void>(transaction->begin_payload_stage(image, selection));
    } catch (const std::logic_error&) {
      rejected = true;
    }
    require(rejected,
            "removal transaction accepted incoming payload staging");
  }

  state->clear_events();
  state->set_observation_sequence({
      {pkgapply::application_path_observation::present(directory(path))},
      {pkgapply::application_path_observation::absent(path)},
  });
  {
    auto transaction =
        backend.begin_without_incoming_image(context, lease);
    const auto before = transaction->observe({path});
    const auto after = transaction->observe({path});
    require(before.find(path) != nullptr &&
                before.find(path)->state() == pkgapply::fact_state::known &&
                after.find(path) != nullptr &&
                after.find(path)->state() ==
                    pkgapply::fact_state::not_applicable,
            "scripted observation sequence did not advance per batch");
  }

  state->clear_events();
  state->throw_at(pkgapply::test::scripted_backend_boundary::observe);
  {
    auto transaction =
        backend.begin_without_incoming_image(context, lease);
    bool failed = false;
    try {
      static_cast<void>(transaction->observe({path}));
    } catch (const std::runtime_error&) {
      failed = true;
    }
    require(failed, "scripted exception injection was ignored");
  }
  require(state->events().size() == 3 &&
              state->events()[1].boundary ==
                  pkgapply::test::scripted_backend_boundary::observe &&
              state->events()[2].boundary ==
                  pkgapply::test::scripted_backend_boundary::
                      transaction_destroyed,
          "scripted failure did not preserve observable cleanup order");

  return 0;
}
