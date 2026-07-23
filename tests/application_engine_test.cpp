// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_engine.h"
#include "plan_fixture.h"
#include "scripted_backend.h"

#include <array>
#include <cstddef>
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

pkgapply::application_attempt_nonce
attempt_nonce(std::uint8_t value = 70)
{
  pkgapply::application_attempt_nonce::byte_array bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return pkgapply::application_attempt_nonce::from_bytes(bytes);
}

pkgapply::application_target_context
target()
{
  return pkgapply::application_target_context::make(
      pkgapply::test::fixture::planning_identity<
          pkgplan::target_system_context_identity>(1),
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

  bool held() const noexcept override
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
  fake_archive(std::vector<pkgimage::package_entry> entries,
               pkgimage::complete_archive_digest digest =
                   pkgapply::test::fixture::archive_digest())
      : image_(std::move(entries)),
        receipt_(pkgimage::archive_backend_identity::parse(
                     "test/pkgimage-v1"),
                 std::move(digest),
                 image_.identity(),
                 image_.size())
  {
  }

  const pkgimage::package_image& image() const noexcept override
  {
    return image_;
  }

  const pkgimage::archive_inspection_receipt&
  inspection_receipt() const noexcept override
  {
    return receipt_;
  }

  void replay(const pkgimage::entry_selection& selection,
              pkgimage::payload_sink& sink) const override
  {
    selection.validate(image_);
    for (const auto& entry : image_.entries()) {
      if (!selection.contains(entry.id))
        continue;
      sink.begin(entry);
      std::vector<std::byte> payload(
          static_cast<std::size_t>(entry.size), std::byte{0x5a});
      if (!payload.empty())
        sink.write(entry, payload.data(), payload.size());
      sink.end(entry);
    }
  }

private:
  pkgimage::package_image image_;
  pkgimage::archive_inspection_receipt receipt_;
};

pkgapply::lease_bound_state_projection
state(const fake_lease& lease,
      const pkgplan::operation_preconditions& preconditions)
{
  std::vector<pkgapply::projected_path_owners> paths;
  paths.reserve(preconditions.paths().size());
  for (const auto& path : preconditions.paths())
    paths.emplace_back(path.path(), path.owners());
  return pkgapply::lease_bound_state_projection::make(
      lease.identity(),
      preconditions.installed_snapshot(),
      preconditions.ownership_inventory(),
      pkgapply::state_projection_completeness::complete,
      std::move(paths),
      application_identity<
          pkgapply::state_projection_evidence_identity>(30));
}

pkgapply::completed_object_fact
observed_regular(const pkgplan::package_path& path,
                 const pkgplan::filesystem_object_metadata& metadata)
{
  require(metadata.kind() == pkgplan::filesystem_object_kind::regular,
          "engine fixture expected a regular planning object");

  const auto size = metadata.size()
      ? pkgapply::qualified_fact<std::uint64_t>::known(*metadata.size())
      : pkgapply::qualified_fact<std::uint64_t>::unknown();
  const auto mtime = metadata.mtime()
      ? pkgapply::qualified_fact<
            pkgapply::completed_object_timestamp>::known(
            {metadata.mtime()->seconds(), metadata.mtime()->nanoseconds()})
      : pkgapply::qualified_fact<
            pkgapply::completed_object_timestamp>::unknown();
  const auto content = metadata.regular_content()
      ? pkgapply::qualified_fact<
            pkgapply::completed_regular_content_identity>::known(
            pkgapply::completed_regular_content_identity::parse(
                metadata.regular_content()->string()))
      : pkgapply::qualified_fact<
            pkgapply::completed_regular_content_identity>::unknown();

  return pkgapply::completed_object_fact(
      path,
      pkgapply::completed_object_kind::regular,
      pkgapply::qualified_fact<std::uint32_t>::known(metadata.mode()),
      pkgapply::qualified_fact<std::uint64_t>::known(metadata.uid()),
      pkgapply::qualified_fact<std::uint64_t>::known(metadata.gid()),
      size,
      mtime,
      content,
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_hardlink_relation>::unknown(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::partial);
}

std::vector<pkgapply::application_path_observation>
matching_observations(const pkgplan::operation_preconditions& preconditions)
{
  std::vector<pkgapply::application_path_observation> observations;
  observations.reserve(preconditions.paths().size());
  for (const auto& path : preconditions.paths()) {
    if (!path.observation().is_present()) {
      observations.push_back(
          pkgapply::application_path_observation::absent(path.path()));
      continue;
    }
    require(path.observation().object() != nullptr,
            "present planning observation lacks object metadata");
    observations.push_back(pkgapply::application_path_observation::present(
        observed_regular(path.path(), *path.observation().object())));
  }
  return observations;
}

void
require_no_effect_boundaries(
    const std::vector<pkgapply::test::scripted_backend_event>& events)
{
  for (const auto& event : events) {
    using boundary = pkgapply::test::scripted_backend_boundary;
    require(event.boundary == boundary::begin_with_incoming_image ||
                event.boundary == boundary::begin_without_incoming_image ||
                event.boundary == boundary::observe ||
                event.boundary == boundary::transaction_destroyed,
            "admission phase crossed an effectful backend boundary");
  }
}

void
require_no_target_effects(
    const std::vector<pkgapply::test::scripted_backend_event>& events)
{
  using boundary = pkgapply::test::scripted_backend_boundary;
  for (const auto& event : events) {
    require(event.boundary != boundary::execute_active &&
                event.boundary != boundary::execute_rejected &&
                event.boundary != boundary::recover,
            "preparation crossed the target-mutation boundary");
  }
}

std::size_t
first_boundary(
    const std::vector<pkgapply::test::scripted_backend_event>& events,
    pkgapply::test::scripted_backend_boundary wanted)
{
  for (std::size_t index = 0; index < events.size(); ++index) {
    if (events[index].boundary == wanted)
      return index;
  }
  return events.size();
}

template<class Request>
pkgapply::application_attempt
expected_attempt(const Request& request,
                 const pkgapply::application_backend& backend,
                 const pkgapply::application_attempt_nonce& nonce)
{
  return pkgapply::application_attempt::make(
      request.identity(),
      request.target().identity(),
      backend.identity(),
      nonce);
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
  using boundary = pkgapply::test::scripted_backend_boundary;

  const auto context = target();
  fake_lease lease(
      application_identity<pkgapply::mutation_lease_instance_identity>(20),
      context.identity(),
      context.mutation_exclusion_domain());

  const pkgapply::test::fixture::planning_authorities authorities(
      context.target());
  fake_archive install_archive({
      pkgapply::test::fixture::regular_entry("tool", 7),
  });
  const auto install_request = pkgapply::installation_application_request::make(
      pkgapply::test::fixture::ordinary_installation(authorities),
      context,
      control());
  const auto install_state = state(
      lease, install_request.plan().preconditions());

  auto backend_state =
      std::make_shared<pkgapply::test::scripted_backend_state>();
  const auto nonce = attempt_nonce();
  const auto evidence = application_identity<
      pkgapply::application_backend_evidence_identity>(31);
  pkgapply::test::scripted_backend backend(
      context.mutation_backend(),
      context.observation_backend(),
      context.capabilities(),
      nonce,
      evidence,
      backend_state);

  // Static authority rejection must happen before the backend is entered.
  pkgapply::test::scripted_backend foreign_backend(
      application_identity<pkgapply::mutation_backend_identity>(90),
      context.observation_backend(),
      context.capabilities(),
      nonce,
      evidence,
      backend_state);
  require_admission_error(
      [&] {
        static_cast<void>(pkgapply::detail::admit_application_engine(
            install_request,
            install_state,
            lease,
            foreign_backend,
            install_archive));
      },
      pkgapply::application_admission_error_code::backend_identity_mismatch,
      "static authority failure opened a backend transaction");
  require(backend_state->events().empty(),
          "static authority rejection emitted backend events");

  // A malformed transaction is destroyed without observing the target.
  backend_state->set_observations(
      matching_observations(install_request.plan().preconditions()));
  backend_state->set_transaction_target(
      application_identity<
          pkgapply::application_target_context_identity>(91));
  require_admission_error(
      [&] {
        static_cast<void>(pkgapply::detail::admit_application_engine(
            install_request,
            install_state,
            lease,
            backend,
            install_archive));
      },
      pkgapply::application_admission_error_code::transaction_target_mismatch,
      "foreign transaction target was admitted");
  require(backend_state->events().size() == 2 &&
              backend_state->events()[0].boundary ==
                  boundary::begin_with_incoming_image &&
              backend_state->events()[1].boundary ==
                  boundary::transaction_destroyed,
          "transaction mismatch performed target observation");
  backend_state->clear_transaction_target();
  backend_state->clear_events();

  // Fresh drift is a truthful receipt, not an exception or mutation attempt.
  const auto install_path = pkgplan::package_path::parse("tool");
  backend_state->set_observations({
      pkgapply::application_path_observation::present(
          observed_regular(
              install_path,
              pkgapply::test::fixture::regular_object(9))),
  });
  const auto refusal = pkgapply::detail::admit_application_engine(
      install_request,
      install_state,
      lease,
      backend,
      install_archive);
  require(!refusal.is_admitted() && refusal.refusal() != nullptr,
          "stale live facts did not return a refusal receipt");
  require(refusal.refusal()->outcome() ==
              pkgapply::application_attempt_outcome::precondition_refused &&
              refusal.refusal()->recovery() ==
                  pkgapply::application_recovery_state::unchanged,
          "precondition refusal classified the target incorrectly");
  require(refusal.refusal()->attempt() ==
              expected_attempt(install_request, backend, nonce).identity(),
          "backend nonce did not bind the refusal attempt");
  require(!refusal.refusal()->journal().has_value() &&
              !refusal.refusal()->completed_evidence().has_value() &&
              refusal.refusal()->paths().empty(),
          "precondition refusal retained mutation or publication claims");
  for (const auto& fact : refusal.refusal()->durability().facts()) {
    require(fact.status() ==
                pkgapply::application_durability_status::not_attempted,
            "precondition refusal claimed attempted durability");
  }
  require(refusal.refusal()->backend_evidence().size() == 1 &&
              refusal.refusal()->backend_evidence().front() == evidence,
          "fresh observation evidence was not retained in refusal");
  require(backend_state->events().size() == 3 &&
              backend_state->events()[0].boundary ==
                  boundary::begin_with_incoming_image &&
              backend_state->events()[1].boundary == boundary::observe &&
              backend_state->events()[2].boundary ==
                  boundary::transaction_destroyed,
          "precondition refusal crossed an unexpected backend boundary");
  require_no_effect_boundaries(backend_state->events());
  require(!backend_state->published_journal().has_value(),
          "precondition refusal published a journal");
  backend_state->clear_events();

  // Installation admission retains the exact observed transaction.
  backend_state->set_observations(
      matching_observations(install_request.plan().preconditions()));
  {
    auto admitted = pkgapply::detail::admit_application_engine(
        install_request,
        install_state,
        lease,
        backend,
        install_archive);
    require(admitted.is_admitted() && admitted.admitted() != nullptr,
            "fresh installation was not admitted");
    require(admitted.admitted()->attempt().identity() ==
                expected_attempt(install_request, backend, nonce).identity(),
            "installation admission changed the physical attempt");
    require(admitted.admitted()->preconditions().satisfied() &&
                backend_state->transaction_alive(),
            "installation admission did not retain its transaction");
    require(backend_state->events().size() == 2 &&
                backend_state->events()[1].boundary == boundary::observe,
            "installation admission observed more than once");
    require_no_effect_boundaries(backend_state->events());
  }
  require(backend_state->events().back().boundary ==
              boundary::transaction_destroyed,
          "admitted installation transaction was not released with session");
  backend_state->clear_events();

  // The complete effect graph is durable before any effectful boundary.
  backend_state->set_observations(
      matching_observations(install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        install_request, install_state, lease, backend, install_archive);
    require(admission.is_admitted() && admission.admitted() != nullptr,
            "journal fixture was not admitted");
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()),
        install_request,
        install_state,
        lease,
        install_archive.image());
    require(journaled.journal().state() ==
                pkgapply::application_journal_state::preparing,
            "initial durable journal is not preparing");
    require(journaled.journal().header().attempt().identity() ==
                journaled.admitted().attempt().identity(),
            "durable journal changed the admitted attempt");
    require(!journaled.journal().effects().empty() &&
                journaled.journal().events().empty(),
            "initial durable journal did not freeze an untouched effect graph");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->identity() ==
                    journaled.journal().identity(),
            "backend did not retain the exact initial journal snapshot");
    require(backend_state->events().size() == 3 &&
                backend_state->events()[0].boundary ==
                    boundary::begin_with_incoming_image &&
                backend_state->events()[1].boundary == boundary::observe &&
                backend_state->events()[2].boundary ==
                    boundary::publish_journal,
            "journal preparation crossed an effect boundary");
  }
  require(backend_state->events().back().boundary ==
              boundary::transaction_destroyed,
          "journaled transaction was not retained through preparation");
  backend_state->clear_events();

  // Upgrade follows the same archive-bearing admission discipline.
  fake_archive upgrade_archive({
      pkgapply::test::fixture::regular_entry("tool", 2, 0755),
  });
  const auto upgrade_request = pkgapply::upgrade_application_request::make(
      pkgapply::test::fixture::ordinary_upgrade(authorities),
      context,
      control());
  const auto upgrade_state = state(
      lease, upgrade_request.plan().preconditions());
  backend_state->set_observations(
      matching_observations(upgrade_request.plan().preconditions()));
  {
    auto admitted = pkgapply::detail::admit_application_engine(
        upgrade_request,
        upgrade_state,
        lease,
        backend,
        upgrade_archive);
    require(admitted.is_admitted(), "fresh upgrade was not admitted");
    require(backend_state->events().size() == 2 &&
                backend_state->events()[0].boundary ==
                    boundary::begin_with_incoming_image &&
                backend_state->events()[1].boundary == boundary::observe,
            "upgrade admission did not use one incoming-image transaction");
    require_no_effect_boundaries(backend_state->events());
  }
  backend_state->clear_events();

  // Removal opens the archive-free transaction and still observes exactly once.
  const auto removal_request = pkgapply::removal_application_request::make(
      pkgapply::test::fixture::ordinary_removal(authorities),
      context,
      control());
  const auto removal_state = state(
      lease, removal_request.plan().preconditions());
  backend_state->set_observations(
      matching_observations(removal_request.plan().preconditions()));
  {
    auto admitted = pkgapply::detail::admit_application_engine(
        removal_request,
        removal_state,
        lease,
        backend);
    require(admitted.is_admitted(), "fresh removal was not admitted");
    require(backend_state->events().size() == 2 &&
                backend_state->events()[0].boundary ==
                    boundary::begin_without_incoming_image &&
                backend_state->events()[1].boundary == boundary::observe,
            "removal admission opened an archive-bearing transaction");
    require_no_effect_boundaries(backend_state->events());
  }
  backend_state->clear_events();

  // Installation preparation replays and seals its exact payload closure.
  backend_state->set_observations(
      matching_observations(install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        install_request, install_state, lease, backend, install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), install_request, install_state,
        lease, install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), install_request, install_state, lease,
        install_archive);
    require(preparation.is_prepared() && preparation.prepared() != nullptr,
            "installation preparation did not complete");
    require(preparation.prepared()->journaled().journal().state() ==
                pkgapply::application_journal_state::prepared,
            "installation preparation did not publish prepared journal");
    require(preparation.prepared()->captures().empty(),
            "ordinary installation captured an absent old object");
    require(preparation.prepared()->durability().status(
                pkgapply::application_durability_domain::incoming_staging) ==
                pkgapply::application_durability_status::confirmed &&
                preparation.prepared()->durability().status(
                    pkgapply::application_durability_domain::recovery_staging) ==
                    pkgapply::application_durability_status::not_attempted,
            "installation staging durability is incorrect");
    require(first_boundary(backend_state->events(), boundary::payload_begin) <
                first_boundary(backend_state->events(), boundary::payload_seal),
            "payload replay was not sealed in order");
    require_no_target_effects(backend_state->events());
  }
  backend_state->clear_events();

  // Upgrade preparation captures the old object before replaying replacement.
  backend_state->set_observations(
      matching_observations(upgrade_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        upgrade_request, upgrade_state, lease, backend, upgrade_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), upgrade_request, upgrade_state,
        lease, upgrade_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), upgrade_request, upgrade_state, lease,
        upgrade_archive);
    require(preparation.is_prepared() && preparation.prepared() != nullptr,
            "upgrade preparation did not complete");
    require(preparation.prepared()->captures().size() == 1 &&
                preparation.prepared()->captures().front().outcome() ==
                    pkgapply::backend_operation_outcome::completed,
            "upgrade recovery capture is incomplete");
    require(preparation.prepared()->durability().status(
                pkgapply::application_durability_domain::incoming_staging) ==
                pkgapply::application_durability_status::confirmed &&
                preparation.prepared()->durability().status(
                    pkgapply::application_durability_domain::recovery_staging) ==
                    pkgapply::application_durability_status::confirmed,
            "upgrade preparation did not confirm both staging domains");
    require(first_boundary(backend_state->events(), boundary::capture_old) <
                first_boundary(backend_state->events(),
                               boundary::begin_payload_stage),
            "upgrade replay preceded required old-object capture");
    require_no_target_effects(backend_state->events());
  }
  backend_state->clear_events();

  // Removal preparation uses no archive but durably captures recovery input.
  backend_state->set_observations(
      matching_observations(removal_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        removal_request, removal_state, lease, backend);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), removal_request, removal_state, lease);
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), removal_request, removal_state, lease);
    require(preparation.is_prepared() &&
                preparation.prepared()->captures().size() == 1,
            "removal preparation did not retain its old object");
    require(first_boundary(backend_state->events(),
                           boundary::begin_payload_stage) ==
                backend_state->events().size(),
            "removal preparation opened an incoming payload stage");
    require_no_target_effects(backend_state->events());
  }
  backend_state->clear_events();

  // A typed payload-stage failure seals a truthful pre-mutation receipt.
  backend_state->set_outcome(
      boundary::payload_seal, pkgapply::backend_operation_outcome::failed);
  backend_state->set_observations(
      matching_observations(install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        install_request, install_state, lease, backend, install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), install_request, install_state,
        lease, install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), install_request, install_state, lease,
        install_archive);
    require(!preparation.is_prepared() && preparation.failure() != nullptr,
            "failed payload stage did not return a failure receipt");
    require(preparation.failure()->outcome() ==
                pkgapply::application_attempt_outcome::
                    failed_before_target_mutation &&
                preparation.failure()->recovery() ==
                    pkgapply::application_recovery_state::unchanged &&
                preparation.failure()->journal().has_value(),
            "payload failure crossed or lost the mutation boundary");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::abandoned &&
                backend_state->published_journal()->receipt().has_value() &&
                *backend_state->published_journal()->receipt() ==
                    preparation.failure()->identity(),
            "payload failure journal did not retain its receipt");
    require_no_target_effects(backend_state->events());
  }
  backend_state->set_outcome(
      boundary::payload_seal, pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // Exact recovery is refused before payload replay when capture is insufficient.
  const auto exact_upgrade_request =
      pkgapply::upgrade_application_request::make(
          upgrade_request.plan(),
          context,
          pkgapply::application_execution_control::make(
              pkgapply::application_recovery_requirement::exact_prior_state,
              pkgapply::application_durability_requirement::journal_and_recovery,
              pkgapply::application_cancellation_policy::
                  recover_after_target_mutation));
  const auto exact_upgrade_state = state(
      lease, exact_upgrade_request.plan().preconditions());
  backend_state->set_exact_recovery_possible(false);
  backend_state->set_observations(
      matching_observations(exact_upgrade_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        exact_upgrade_request, exact_upgrade_state, lease, backend,
        upgrade_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), exact_upgrade_request,
        exact_upgrade_state, lease, upgrade_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), exact_upgrade_request, exact_upgrade_state,
        lease, upgrade_archive);
    require(!preparation.is_prepared() && preparation.failure() != nullptr,
            "inexact recovery capture satisfied an exact requirement");
    require(preparation.failure()->durability().status(
                pkgapply::application_durability_domain::recovery_staging) ==
                pkgapply::application_durability_status::visible,
            "exact-recovery refusal lost established capture visibility");
    require(first_boundary(backend_state->events(),
                           boundary::begin_payload_stage) ==
                backend_state->events().size(),
            "exact-recovery refusal replayed incoming payloads");
    require_no_target_effects(backend_state->events());
  }
  backend_state->set_exact_recovery_possible(true);
  backend_state->clear_events();

  // Failed journal synchronization remains explicit in the failure receipt.
  backend_state->set_durability(
      pkgapply::application_durability_domain::journal,
      pkgapply::application_durability_status::unconfirmed);
  backend_state->set_observations(
      matching_observations(install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        install_request, install_state, lease, backend, install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), install_request, install_state,
        lease, install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), install_request, install_state, lease,
        install_archive);
    require(!preparation.is_prepared() && preparation.failure() != nullptr,
            "unconfirmed journal synchronization produced a prepared attempt");
    require(preparation.failure()->durability().status(
                pkgapply::application_durability_domain::journal) ==
                pkgapply::application_durability_status::unconfirmed &&
                preparation.failure()->durability().status(
                    pkgapply::application_durability_domain::incoming_staging) ==
                    pkgapply::application_durability_status::confirmed,
            "journal synchronization failure was reported as confirmed");
    require_no_target_effects(backend_state->events());
  }
  backend_state->set_durability(
      pkgapply::application_durability_domain::journal,
      pkgapply::application_durability_status::confirmed);

  return 0;
}
