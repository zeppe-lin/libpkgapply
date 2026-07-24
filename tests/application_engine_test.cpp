// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_engine.h"
#include "plan_fixture.h"
#include "scripted_backend.h"

#include <algorithm>
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
observed_object(const pkgplan::package_path& path,
                const pkgplan::filesystem_object_metadata& metadata)
{
  const auto mode =
      pkgapply::qualified_fact<std::uint32_t>::known(metadata.mode());
  const auto uid =
      pkgapply::qualified_fact<std::uint64_t>::known(metadata.uid());
  const auto gid =
      pkgapply::qualified_fact<std::uint64_t>::known(metadata.gid());
  const auto mtime = metadata.mtime()
      ? pkgapply::qualified_fact<
            pkgapply::completed_object_timestamp>::known(
            {metadata.mtime()->seconds(), metadata.mtime()->nanoseconds()})
      : pkgapply::qualified_fact<
            pkgapply::completed_object_timestamp>::unknown();

  switch (metadata.kind()) {
    case pkgplan::filesystem_object_kind::regular: {
      const auto size = metadata.size()
          ? pkgapply::qualified_fact<std::uint64_t>::known(*metadata.size())
          : pkgapply::qualified_fact<std::uint64_t>::unknown();
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
          mode,
          uid,
          gid,
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

    case pkgplan::filesystem_object_kind::directory:
      return pkgapply::completed_object_fact(
          path,
          pkgapply::completed_object_kind::directory,
          mode,
          uid,
          gid,
          pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
          mtime,
          pkgapply::qualified_fact<
              pkgapply::completed_regular_content_identity>::not_applicable(),
          pkgapply::qualified_fact<std::string>::not_applicable(),
          pkgapply::qualified_fact<
              pkgapply::completed_device_number>::not_applicable(),
          pkgapply::qualified_fact<
              pkgapply::completed_hardlink_relation>::not_applicable(),
          pkgapply::object_fact_provenance::application_observation,
          pkgapply::object_fact_completeness::partial);

    default:
      throw std::runtime_error(
          "engine fixture supports only regular and directory observations");
  }
}

pkgapply::completed_object_fact
observed_incoming(const pkgimage::package_entry& entry)
{
  const auto path = pkgplan::package_path::parse(entry.path.string());
  const auto mode =
      pkgapply::qualified_fact<std::uint32_t>::known(entry.mode);
  const auto uid =
      pkgapply::qualified_fact<std::uint64_t>::known(entry.uid);
  const auto gid =
      pkgapply::qualified_fact<std::uint64_t>::known(entry.gid);
  const auto mtime = pkgapply::qualified_fact<
      pkgapply::completed_object_timestamp>::known(
      {entry.mtime, entry.mtime_nanoseconds});

  if (entry.type == pkgimage::entry_type::regular) {
    require(entry.regular_content.has_value(),
            "regular incoming fixture lacks content identity");
    return pkgapply::completed_object_fact(
        path,
        pkgapply::completed_object_kind::regular,
        mode,
        uid,
        gid,
        pkgapply::qualified_fact<std::uint64_t>::known(entry.size),
        mtime,
        pkgapply::qualified_fact<
            pkgapply::completed_regular_content_identity>::known(
            pkgapply::completed_regular_content_identity::parse(
                entry.regular_content->string())),
        pkgapply::qualified_fact<std::string>::not_applicable(),
        pkgapply::qualified_fact<
            pkgapply::completed_device_number>::not_applicable(),
        pkgapply::qualified_fact<
            pkgapply::completed_hardlink_relation>::unknown(),
        pkgapply::object_fact_provenance::application_observation,
        pkgapply::object_fact_completeness::complete);
  }

  if (entry.type == pkgimage::entry_type::directory) {
    return pkgapply::completed_object_fact(
        path,
        pkgapply::completed_object_kind::directory,
        mode,
        uid,
        gid,
        pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
        mtime,
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

  throw std::runtime_error(
      "engine completion fixture supports regular and directory entries");
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
        observed_object(path.path(), *path.observation().object())));
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

void
require_no_active_effects(
    const std::vector<pkgapply::test::scripted_backend_event>& events)
{
  using boundary = pkgapply::test::scripted_backend_boundary;
  for (const auto& event : events) {
    require(event.boundary != boundary::execute_active &&
                event.boundary != boundary::recover,
            "rejected publication crossed the active-mutation boundary");
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

std::size_t
last_boundary(
    const std::vector<pkgapply::test::scripted_backend_event>& events,
    pkgapply::test::scripted_backend_boundary wanted)
{
  for (std::size_t index = events.size(); index != 0; --index) {
    if (events[index - 1].boundary == wanted)
      return index - 1;
  }
  return events.size();
}

std::size_t
count_boundary(
    const std::vector<pkgapply::test::scripted_backend_event>& events,
    pkgapply::test::scripted_backend_boundary wanted)
{
  return static_cast<std::size_t>(std::count_if(
      events.begin(), events.end(),
      [wanted](const auto& event) { return event.boundary == wanted; }));
}

std::size_t
first_synchronization(
    const std::vector<pkgapply::test::scripted_backend_event>& events,
    pkgapply::application_durability_domain domain)
{
  using boundary = pkgapply::test::scripted_backend_boundary;
  for (std::size_t index = 0; index < events.size(); ++index) {
    if (events[index].boundary == boundary::synchronize &&
        events[index].durability_domain.has_value() &&
        *events[index].durability_domain == domain)
    {
      return index;
    }
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
          observed_object(
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

  const auto rejected_install_path =
      pkgplan::package_path::parse("tool.conf");
  const auto rejected_install_active =
      pkgapply::test::fixture::regular_object(9);
  const auto rejected_install_policy =
      pkgapply::test::fixture::policy_snapshot(
          authorities,
          pkgapply::test::fixture::path_policy(
              pkgplan::incoming_path_policy::retain(
                  pkgplan::rejected_object_policy::stage,
                  pkgplan::retained_active_ownership_policy::
                      add_operated_owner)));
  fake_archive rejected_install_archive({
      pkgapply::test::fixture::regular_entry("tool.conf", 7),
  });
  const auto rejected_install_request =
      pkgapply::installation_application_request::make(
          pkgapply::test::fixture::installation_plan(
              authorities,
              {pkgapply::test::fixture::regular_entry("tool.conf", 7)},
              {pkgplan::target_path_observation::present(
                  pkgplan::filesystem_object_fact(
                      rejected_install_path, rejected_install_active))},
              {}, rejected_install_policy),
          context, control());
  const auto rejected_install_state = state(
      lease, rejected_install_request.plan().preconditions());
  const auto durable_rejected_install_request =
      pkgapply::installation_application_request::make(
          rejected_install_request.plan(), context,
          pkgapply::application_execution_control::make(
              pkgapply::application_recovery_requirement::best_effort,
              pkgapply::application_durability_requirement::
                  all_application_domains,
              pkgapply::application_cancellation_policy::
                  recover_after_target_mutation));
  const auto durable_rejected_install_state = state(
      lease, durable_rejected_install_request.plan().preconditions());

  const auto rejected_old_path =
      pkgplan::package_path::parse("var/lib/tool/state");
  const auto rejected_old_active =
      pkgapply::test::fixture::regular_object(4);
  const auto rejected_old_policy =
      pkgapply::test::fixture::policy_snapshot(
          authorities,
          pkgapply::test::fixture::path_policy(
              pkgplan::incoming_path_policy::activate(),
              pkgplan::obsolete_path_policy::remove(
                  pkgplan::rejected_object_policy::stage)));
  const auto rejected_old_request =
      pkgapply::removal_application_request::make(
          pkgapply::test::fixture::removal_plan(
              authorities,
              {pkgplan::installed_ownership_claim(
                  rejected_old_path, authorities.installed_package,
                  rejected_old_active)},
              {pkgplan::target_path_observation::present(
                  pkgplan::filesystem_object_fact(
                      rejected_old_path, rejected_old_active))},
              rejected_old_policy),
          context, control());
  const auto rejected_old_state = state(
      lease, rejected_old_request.plan().preconditions());

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

  // A plan without rejected effects crosses no rejected-store boundary.
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
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()), install_request,
            install_state, lease);
    require(publication.is_published() && publication.published() != nullptr,
            "rejected-free installation did not reach active readiness");
    require(publication.published()->rejected_effects().empty() &&
                publication.published()->durability().status(
                    pkgapply::application_durability_domain::
                        rejected_object_store) ==
                    pkgapply::application_durability_status::not_attempted &&
                publication.published()->prepared().journaled().journal().state() ==
                    pkgapply::application_journal_state::mutating,
            "rejected-free publication invented rejected-store effects");
    require(first_boundary(backend_state->events(),
                           boundary::execute_rejected) ==
                backend_state->events().size(),
            "rejected-free publication entered the rejected store");
    require_no_active_effects(backend_state->events());
  }
  backend_state->clear_events();

  // Incoming rejected objects publish from the sealed payload stage first.
  backend_state->set_observations(matching_observations(
      rejected_install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        rejected_install_request, rejected_install_state, lease, backend,
        rejected_install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), rejected_install_request,
        rejected_install_state, lease, rejected_install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), rejected_install_request,
        rejected_install_state, lease, rejected_install_archive);
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()), rejected_install_request,
            rejected_install_state, lease);
    require(publication.is_published() && publication.published() != nullptr &&
                publication.published()->rejected_effects().size() == 1,
            "incoming rejected object was not published");
    const auto& rejected =
        publication.published()->rejected_effects().front();
    require(rejected.request().outcome() ==
                pkgplan::planned_rejected_outcome::stage_incoming &&
                rejected.request().incoming_entry().has_value() &&
                rejected.result().record().has_value(),
            "incoming rejected publication lost source or record authority");
    require(publication.published()->durability().status(
                pkgapply::application_durability_domain::
                    rejected_object_store) ==
                pkgapply::application_durability_status::visible,
            "journal-and-recovery publication invented rejected durability");
    require(first_boundary(backend_state->events(), boundary::payload_seal) <
                first_boundary(backend_state->events(),
                               boundary::execute_rejected),
            "incoming rejected publication preceded payload sealing");
    require(first_synchronization(
                backend_state->events(),
                pkgapply::application_durability_domain::
                    rejected_object_store) == backend_state->events().size(),
            "journal-and-recovery synchronized an unrequired rejected store");
    require_no_active_effects(backend_state->events());
  }
  backend_state->clear_events();

  // Old rejected objects publish only after their admitted object is captured.
  backend_state->set_observations(
      matching_observations(rejected_old_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        rejected_old_request, rejected_old_state, lease, backend);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), rejected_old_request,
        rejected_old_state, lease);
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), rejected_old_request, rejected_old_state,
        lease);
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()), rejected_old_request,
            rejected_old_state, lease);
    require(publication.is_published() && publication.published() != nullptr &&
                publication.published()->rejected_effects().size() == 1 &&
                publication.published()->rejected_effects().front().request().
                    outcome() ==
                    pkgplan::planned_rejected_outcome::stage_old,
            "old rejected object was not published from capture authority");
    require(first_boundary(backend_state->events(), boundary::capture_old) <
                first_boundary(backend_state->events(),
                               boundary::execute_rejected),
            "old rejected publication preceded old-object capture");
    require_no_active_effects(backend_state->events());
  }
  backend_state->clear_events();

  // A rejected publication failure retains exact path failure evidence while
  // the active namespace remains untouched.
  backend_state->set_outcome(
      boundary::execute_rejected, pkgapply::backend_operation_outcome::failed);
  backend_state->set_observations(matching_observations(
      rejected_install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        rejected_install_request, rejected_install_state,
        lease, backend, rejected_install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), rejected_install_request,
        rejected_install_state, lease,
        rejected_install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), rejected_install_request,
        rejected_install_state, lease, rejected_install_archive);
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()),
            rejected_install_request,
            rejected_install_state, lease);
    require(!publication.is_published() && publication.failure() != nullptr &&
                publication.failure()->outcome() ==
                    pkgapply::application_attempt_outcome::
                        failed_before_target_mutation &&
                publication.failure()->paths().size() == 1 &&
                publication.failure()->paths().front().active_status() ==
                    pkgapply::application_effect_status::not_attempted &&
                publication.failure()->paths().front().rejected_status() ==
                    pkgapply::application_effect_status::failed,
            "rejected failure crossed or concealed the active boundary");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::abandoned,
            "rejected publication failure did not terminate its journal");
    require_no_active_effects(backend_state->events());
  }
  backend_state->set_outcome(
      boundary::execute_rejected,
      pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // An indeterminate rejected operation cannot be collapsed into failure.
  backend_state->set_outcome(
      boundary::execute_rejected,
      pkgapply::backend_operation_outcome::indeterminate);
  backend_state->set_observations(matching_observations(
      rejected_install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        rejected_install_request, rejected_install_state, lease, backend,
        rejected_install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), rejected_install_request,
        rejected_install_state, lease, rejected_install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), rejected_install_request,
        rejected_install_state, lease, rejected_install_archive);
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()), rejected_install_request,
            rejected_install_state, lease);
    require(!publication.is_published() && publication.failure() != nullptr &&
                publication.failure()->outcome() ==
                    pkgapply::application_attempt_outcome::indeterminate &&
                publication.failure()->recovery() ==
                    pkgapply::application_recovery_state::
                        requires_authoritative_observation &&
                publication.failure()->paths().front().rejected_status() ==
                    pkgapply::application_effect_status::indeterminate,
            "indeterminate rejected publication was collapsed");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::indeterminate,
            "indeterminate rejected publication used the wrong journal state");
    require_no_active_effects(backend_state->events());
  }
  backend_state->set_outcome(
      boundary::execute_rejected,
      pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // All-domain durability synchronizes the rejected store after publication.
  backend_state->set_durability(
      pkgapply::application_durability_domain::rejected_object_store,
      pkgapply::application_durability_status::confirmed);
  backend_state->set_observations(matching_observations(
      durable_rejected_install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        durable_rejected_install_request, durable_rejected_install_state,
        lease, backend, rejected_install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), durable_rejected_install_request,
        durable_rejected_install_state, lease,
        rejected_install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), durable_rejected_install_request,
        durable_rejected_install_state, lease, rejected_install_archive);
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()),
            durable_rejected_install_request,
            durable_rejected_install_state, lease);
    require(publication.is_published() &&
                publication.published()->durability().status(
                    pkgapply::application_durability_domain::
                        rejected_object_store) ==
                    pkgapply::application_durability_status::confirmed,
            "all-domain publication did not confirm rejected durability");
    require(first_boundary(backend_state->events(),
                           boundary::execute_rejected) <
                first_synchronization(
                    backend_state->events(),
                    pkgapply::application_durability_domain::
                        rejected_object_store),
            "all-domain rejected synchronization preceded publication");
    require_no_active_effects(backend_state->events());
  }
  backend_state->clear_events();

  // A visible but not durable rejected store stops before active mutation with
  // a truthful durability-unconfirmed receipt and immutable rejected record.
  backend_state->set_durability(
      pkgapply::application_durability_domain::rejected_object_store,
      pkgapply::application_durability_status::visible);
  backend_state->set_observations(matching_observations(
      durable_rejected_install_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        durable_rejected_install_request, durable_rejected_install_state,
        lease, backend, rejected_install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), durable_rejected_install_request,
        durable_rejected_install_state, lease, rejected_install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), durable_rejected_install_request,
        durable_rejected_install_state, lease, rejected_install_archive);
    auto publication =
        pkgapply::detail::publish_rejected_application_engine(
            std::move(*preparation.prepared()), durable_rejected_install_request,
            durable_rejected_install_state, lease);
    require(!publication.is_published() && publication.failure() != nullptr &&
                publication.failure()->outcome() ==
                    pkgapply::application_attempt_outcome::
                        effects_visible_durability_unconfirmed &&
                publication.failure()->recovery() ==
                    pkgapply::application_recovery_state::
                        recovery_assets_retained &&
                publication.failure()->paths().size() == 1 &&
                publication.failure()->paths().front().rejected_status() ==
                    pkgapply::application_effect_status::completed &&
                publication.failure()->paths().front().rejected_object().
                    has_value(),
            "visible rejected store was promoted to durable completion");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::effects_visible,
            "visible rejected durability used the wrong journal state");
    require_no_active_effects(backend_state->events());
  }
  backend_state->set_durability(
      pkgapply::application_durability_domain::rejected_object_store,
      pkgapply::application_durability_status::confirmed);
  backend_state->clear_events();

  // Ordinary installation executes the exact incoming active command and
  // stops at the effects-visible boundary before final observation.
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    require(active.is_complete() && active.complete() != nullptr &&
                active.complete()->active_effects().size() == 1,
            "ordinary installation did not complete its active effect");
    const auto& effect = active.complete()->active_effects().front();
    require(effect.request().outcome() ==
                pkgplan::planned_active_outcome::activate_incoming &&
                effect.request().incoming_entry().has_value() &&
                effect.result().outcome() ==
                    pkgapply::backend_operation_outcome::completed &&
                effect.changed_target(),
            "installation active effect lost incoming or completion authority");
    require(active.complete()->durability().status(
                pkgapply::application_durability_domain::active_namespace) ==
                pkgapply::application_durability_status::visible &&
                active.complete()->rejected().prepared().journaled().journal().
                    state() ==
                    pkgapply::application_journal_state::effects_visible,
            "installation active visibility used the wrong boundary");
    require(first_boundary(backend_state->events(), boundary::payload_seal) <
                first_boundary(backend_state->events(),
                               boundary::execute_active),
            "installation active effect preceded payload sealing");
    require(first_synchronization(
                backend_state->events(),
                pkgapply::application_durability_domain::active_namespace) ==
                backend_state->events().size(),
            "journal-and-recovery synchronized the active namespace");
    require(first_boundary(backend_state->events(), boundary::recover) ==
                backend_state->events().size(),
            "successful active execution entered recovery");
  }
  backend_state->clear_events();

  // Upgrade replacement uses the same prepared transaction and runs only
  // after its exact old-object capture and incoming payload sealing.
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), upgrade_request, upgrade_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), upgrade_request, upgrade_state,
        lease);
    require(active.is_complete() && active.complete() != nullptr &&
                active.complete()->active_effects().size() == 1 &&
                active.complete()->active_effects().front().changed_target(),
            "upgrade replacement did not complete its active effect");
    require(first_boundary(backend_state->events(), boundary::capture_old) <
                first_boundary(backend_state->events(),
                               boundary::execute_active) &&
                first_boundary(backend_state->events(),
                               boundary::payload_seal) <
                first_boundary(backend_state->events(),
                               boundary::execute_active),
            "upgrade active replacement preceded preparation authority");
  }
  backend_state->clear_events();

  // Conditional directory cleanup is semantic completion without claiming a
  // managed active-namespace mutation or unnecessary synchronization.
  const auto directory_path = pkgplan::package_path::parse("usr/share/tool");
  const auto directory_active = pkgapply::test::fixture::directory_object();
  const auto directory_removal_request =
      pkgapply::removal_application_request::make(
          pkgapply::test::fixture::removal_plan(
              authorities,
              {pkgplan::installed_ownership_claim(
                  directory_path, authorities.installed_package,
                  directory_active)},
              {pkgplan::target_path_observation::present(
                  pkgplan::filesystem_object_fact(
                      directory_path, directory_active))}),
          context, control());
  const auto directory_removal_state = state(
      lease, directory_removal_request.plan().preconditions());
  backend_state->set_outcome(
      boundary::execute_active,
      pkgapply::backend_operation_outcome::conditional_retained);
  backend_state->set_observations(matching_observations(
      directory_removal_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        directory_removal_request, directory_removal_state, lease, backend);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), directory_removal_request,
        directory_removal_state, lease);
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), directory_removal_request,
        directory_removal_state, lease);
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), directory_removal_request,
        directory_removal_state, lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), directory_removal_request,
        directory_removal_state, lease);
    require(active.is_complete() && active.complete() != nullptr &&
                active.complete()->active_effects().size() == 1 &&
                active.complete()->active_effects().front().result().outcome() ==
                    pkgapply::backend_operation_outcome::conditional_retained &&
                !active.complete()->active_effects().front().changed_target() &&
                active.complete()->durability().status(
                    pkgapply::application_durability_domain::active_namespace) ==
                    pkgapply::application_durability_status::not_attempted,
            "conditional directory retention claimed a target mutation");
    require(first_synchronization(
                backend_state->events(),
                pkgapply::application_durability_domain::active_namespace) ==
                backend_state->events().size(),
            "conditional directory retention synchronized an unchanged target");
  }
  backend_state->set_outcome(
      boundary::execute_active,
      pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // A failed active effect retains the live transaction and recovery assets;
  // recovery itself belongs to the next engine phase.
  backend_state->set_outcome(
      boundary::execute_active, pkgapply::backend_operation_outcome::failed);
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    require(!active.is_complete() && active.interruption() != nullptr &&
                active.interruption()->interruption() ==
                    pkgapply::detail::active_execution_interruption::
                        effect_failed &&
                active.interruption()->active_effects().size() == 1 &&
                active.interruption()->durability().status(
                    pkgapply::application_durability_domain::active_namespace) ==
                    pkgapply::application_durability_status::not_attempted &&
                active.interruption()->rejected().prepared().journaled().
                    journal().state() ==
                    pkgapply::application_journal_state::recovery_pending,
            "failed active effect was collapsed or finalized prematurely");
    require(first_boundary(backend_state->events(), boundary::recover) ==
                backend_state->events().size(),
            "active execution performed recovery inside the effect phase");
    auto receipt = pkgapply::detail::recover_application_engine(
        std::move(*active.interruption()), install_request, install_state,
        lease);
    require(receipt.outcome() ==
                pkgapply::application_attempt_outcome::
                    failed_before_target_mutation &&
                receipt.recovery() ==
                    pkgapply::application_recovery_state::unchanged &&
                receipt.paths().size() == 1 &&
                receipt.paths().front().active_status() ==
                    pkgapply::application_effect_status::failed,
            "known failed command did not seal an unchanged receipt");
    require(first_boundary(backend_state->events(), boundary::recover) ==
                backend_state->events().size() &&
                backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::abandoned,
            "unchanged failure entered recovery or retained a live journal");
  }
  backend_state->set_outcome(
      boundary::execute_active,
      pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // An indeterminate active effect retains indeterminate target durability and
  // an indeterminate journal rather than pretending the target is unchanged.
  backend_state->set_outcome(
      boundary::execute_active,
      pkgapply::backend_operation_outcome::indeterminate);
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    require(!active.is_complete() && active.interruption() != nullptr &&
                active.interruption()->interruption() ==
                    pkgapply::detail::active_execution_interruption::
                        effect_indeterminate &&
                active.interruption()->durability().status(
                    pkgapply::application_durability_domain::active_namespace) ==
                    pkgapply::application_durability_status::indeterminate &&
                active.interruption()->rejected().prepared().journaled().
                    journal().state() ==
                    pkgapply::application_journal_state::indeterminate,
            "indeterminate active effect was collapsed into ordinary failure");
  }
  backend_state->set_outcome(
      boundary::execute_active,
      pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // All-domain success synchronizes the active namespace after every active
  // command and retains confirmed durability for final observation.
  const auto durable_active_request =
      pkgapply::installation_application_request::make(
          install_request.plan(), context,
          pkgapply::application_execution_control::make(
              pkgapply::application_recovery_requirement::best_effort,
              pkgapply::application_durability_requirement::
                  all_application_domains,
              pkgapply::application_cancellation_policy::
                  recover_after_target_mutation));
  const auto durable_active_state = state(
      lease, durable_active_request.plan().preconditions());
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::confirmed);
  backend_state->set_observations(matching_observations(
      durable_active_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        durable_active_request, durable_active_state, lease, backend,
        install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), durable_active_request,
        durable_active_state, lease, install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), durable_active_request, durable_active_state,
        lease, install_archive);
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), durable_active_request,
        durable_active_state, lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), durable_active_request,
        durable_active_state, lease);
    require(active.is_complete() && active.complete() != nullptr &&
                active.complete()->durability().status(
                    pkgapply::application_durability_domain::active_namespace) ==
                    pkgapply::application_durability_status::confirmed,
            "all-domain active execution did not confirm durability");
    require(first_boundary(backend_state->events(), boundary::execute_active) <
                first_synchronization(
                    backend_state->events(),
                    pkgapply::application_durability_domain::active_namespace),
            "active-namespace synchronization preceded active effects");
  }
  backend_state->clear_events();

  // A failed active synchronization retains all completed effects and hands
  // the same transaction to recovery instead of manufacturing success.
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::unconfirmed);
  backend_state->set_observations(matching_observations(
      durable_active_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        durable_active_request, durable_active_state, lease, backend,
        install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), durable_active_request,
        durable_active_state, lease, install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), durable_active_request, durable_active_state,
        lease, install_archive);
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), durable_active_request,
        durable_active_state, lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), durable_active_request,
        durable_active_state, lease);
    require(!active.is_complete() && active.interruption() != nullptr &&
                active.interruption()->interruption() ==
                    pkgapply::detail::active_execution_interruption::
                        durability_unconfirmed &&
                active.interruption()->active_effects().size() == 1 &&
                active.interruption()->active_effects().front().changed_target() &&
                active.interruption()->durability().status(
                    pkgapply::application_durability_domain::active_namespace) ==
                    pkgapply::application_durability_status::unconfirmed &&
                active.interruption()->rejected().prepared().journaled().
                    journal().state() ==
                    pkgapply::application_journal_state::recovery_pending,
            "active durability failure discarded completed target effects");
    require(first_boundary(backend_state->events(), boundary::recover) ==
                backend_state->events().size(),
            "active durability failure recovered inside the effect phase");
  }
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::confirmed);
  backend_state->clear_events();

  // Final observation binds the physical target to the accepted plan, then
  // publishes and confirms completed evidence before sealing a success receipt.
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    require(active.is_complete() && active.complete() != nullptr,
            "completion fixture did not reach active visibility");

    backend_state->set_observations({
        pkgapply::application_path_observation::present(
            observed_incoming(install_archive.image().entries().front())),
    });
    auto completion = pkgapply::detail::complete_application_engine(
        std::move(*active.complete()), install_request, install_state, lease,
        install_archive.image());
    require(completion.has_receipt() && completion.receipt() != nullptr &&
                completion.receipt()->outcome() ==
                    pkgapply::application_attempt_outcome::completed &&
                completion.receipt()->completed_evidence().has_value(),
            "verified installation did not produce completed evidence");
    const auto& receipt = *completion.receipt();
    require(receipt.paths().size() == 1 &&
                receipt.paths().front().active_status() ==
                    pkgapply::application_effect_status::completed &&
                receipt.paths().front().after().state() ==
                    pkgapply::fact_state::known &&
                receipt.paths().front().publication() ==
                    pkgapply::ownership_publication_status::eligible,
            "completed installation path is not publication eligible");
    require(receipt.durability().status(
                pkgapply::application_durability_domain::completed_evidence) ==
                pkgapply::application_durability_status::confirmed &&
                backend_state->published_completed_evidence().has_value() &&
                backend_state->published_completed_evidence()->identity() ==
                    receipt.completed_evidence()->identity(),
            "completed evidence was not exactly published and confirmed");
    require(count_boundary(backend_state->events(), boundary::observe) == 2 &&
                first_boundary(backend_state->events(),
                               boundary::execute_active) <
                    last_boundary(backend_state->events(), boundary::observe) &&
                last_boundary(backend_state->events(), boundary::observe) <
                    first_boundary(
                        backend_state->events(),
                        boundary::publish_completed_evidence) &&
                first_boundary(
                    backend_state->events(),
                    boundary::publish_completed_evidence) <
                    first_synchronization(
                        backend_state->events(),
                        pkgapply::application_durability_domain::
                            completed_evidence),
            "completion boundaries are not observation-publication-durability");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::
                        application_completed &&
                backend_state->published_journal()->receipt() ==
                    receipt.identity() &&
                backend_state->published_journal()->completed_evidence() ==
                    receipt.completed_evidence()->identity(),
            "terminal completion journal lost receipt or evidence identity");
  }
  backend_state->clear_events();

  // A contradictory final observation re-enters the recovery branch.  It does
  // not publish completed evidence or claim ownership publication eligibility.
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    backend_state->set_observations({
        pkgapply::application_path_observation::absent(install_path),
    });
    auto completion = pkgapply::detail::complete_application_engine(
        std::move(*active.complete()), install_request, install_state, lease,
        install_archive.image());
    require(!completion.has_receipt() &&
                completion.interruption() != nullptr &&
                completion.interruption()->interruption() ==
                    pkgapply::detail::active_execution_interruption::
                        result_observation_mismatch &&
                completion.interruption()->rejected().prepared().journaled().
                    journal().state() ==
                    pkgapply::application_journal_state::recovery_pending,
            "contradictory result observation manufactured completion");
    const std::size_t evidence_publications = count_boundary(
        backend_state->events(), boundary::publish_completed_evidence);
    auto receipt = pkgapply::detail::recover_application_engine(
        std::move(*completion.interruption()), install_request, install_state,
        lease);
    require(receipt.outcome() ==
                pkgapply::application_attempt_outcome::failed_fully_recovered &&
                receipt.recovery() ==
                    pkgapply::application_recovery_state::
                        exact_prior_state_restored &&
                count_boundary(backend_state->events(),
                               boundary::publish_completed_evidence) ==
                    evidence_publications,
            "result mismatch was completed or recovered after evidence publish");
  }
  backend_state->clear_events();

  // An unknown final observation cannot be collapsed into a mismatch or a
  // successful result.  The live transaction remains available for an
  // authoritative recovery decision, and no completed record is published.
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    backend_state->set_observations({
        pkgapply::application_path_observation::unknown(install_path),
    });
    const std::size_t evidence_publications = count_boundary(
        backend_state->events(), boundary::publish_completed_evidence);
    auto completion = pkgapply::detail::complete_application_engine(
        std::move(*active.complete()), install_request, install_state, lease,
        install_archive.image());
    require(!completion.has_receipt() &&
                completion.interruption() != nullptr &&
                completion.interruption()->interruption() ==
                    pkgapply::detail::active_execution_interruption::
                        result_observation_indeterminate &&
                completion.interruption()->rejected().prepared().journaled().
                    journal().state() ==
                    pkgapply::application_journal_state::indeterminate &&
                count_boundary(backend_state->events(),
                               boundary::publish_completed_evidence) ==
                    evidence_publications,
            "unknown result observation manufactured completion truth");
  }
  backend_state->clear_events();

  // Completed-evidence storage failure leaves the already verified target
  // visible, but makes every path ineligible for installed-state publication.
  backend_state->set_outcome(
      boundary::publish_completed_evidence,
      pkgapply::backend_operation_outcome::failed);
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    backend_state->set_observations({
        pkgapply::application_path_observation::present(
            observed_incoming(install_archive.image().entries().front())),
    });
    auto completion = pkgapply::detail::complete_application_engine(
        std::move(*active.complete()), install_request, install_state, lease,
        install_archive.image());
    require(completion.has_receipt() && completion.receipt() != nullptr &&
                completion.receipt()->outcome() ==
                    pkgapply::application_attempt_outcome::
                        effects_visible_durability_unconfirmed &&
                !completion.receipt()->completed_evidence().has_value() &&
                completion.receipt()->paths().size() == 1 &&
                completion.receipt()->paths().front().publication() ==
                    pkgapply::ownership_publication_status::ineligible &&
                completion.receipt()->durability().status(
                    pkgapply::application_durability_domain::
                        completed_evidence) ==
                    pkgapply::application_durability_status::unconfirmed,
            "failed evidence publication promoted installed-state truth");
    require(backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::effects_visible &&
                !backend_state->published_journal()->completed_evidence().
                    has_value(),
            "evidence publication failure retained completed journal truth");
  }
  backend_state->set_outcome(
      boundary::publish_completed_evidence,
      pkgapply::backend_operation_outcome::completed);
  backend_state->clear_events();

  // A visible completed-evidence record is not durable evidence.  Failed
  // synchronization keeps the verified active result but denies publication
  // eligibility and omits completed evidence from the terminal journal.
  backend_state->set_durability(
      pkgapply::application_durability_domain::completed_evidence,
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
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), install_request, install_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), install_request, install_state,
        lease);
    backend_state->set_observations({
        pkgapply::application_path_observation::present(
            observed_incoming(install_archive.image().entries().front())),
    });
    auto completion = pkgapply::detail::complete_application_engine(
        std::move(*active.complete()), install_request, install_state, lease,
        install_archive.image());
    require(completion.has_receipt() && completion.receipt() != nullptr &&
                completion.receipt()->outcome() ==
                    pkgapply::application_attempt_outcome::
                        effects_visible_durability_unconfirmed &&
                !completion.receipt()->completed_evidence().has_value() &&
                completion.receipt()->paths().front().publication() ==
                    pkgapply::ownership_publication_status::ineligible &&
                completion.receipt()->durability().status(
                    pkgapply::application_durability_domain::
                        completed_evidence) ==
                    pkgapply::application_durability_status::unconfirmed &&
                backend_state->published_journal().has_value() &&
                !backend_state->published_journal()->completed_evidence().
                    has_value(),
            "visible but unsynchronized evidence became installed-state truth");
  }
  backend_state->set_durability(
      pkgapply::application_durability_domain::completed_evidence,
      pkgapply::application_durability_status::confirmed);
  backend_state->clear_events();

  // Archive-free removal observes absence and seals the same completed-evidence
  // contract without inventing incoming image authority.
  backend_state->set_observations(
      matching_observations(removal_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        removal_request, removal_state, lease, backend);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), removal_request, removal_state,
        lease);
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), removal_request, removal_state, lease);
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), removal_request, removal_state,
        lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), removal_request, removal_state,
        lease);
    backend_state->set_observations({
        pkgapply::application_path_observation::absent(
            removal_request.plan().paths().front().path()),
    });
    auto completion = pkgapply::detail::complete_application_engine(
        std::move(*active.complete()), removal_request, removal_state, lease);
    require(completion.has_receipt() && completion.receipt() != nullptr &&
                completion.receipt()->outcome() ==
                    pkgapply::application_attempt_outcome::completed &&
                completion.receipt()->paths().front().after().state() ==
                    pkgapply::fact_state::not_applicable,
            "archive-free removal did not seal absent result evidence");
  }
  backend_state->clear_events();

  // Recovery reverses the completed active prefix, including dependency order,
  // and seals a fully recovered failure only after the restored namespace is
  // visible and the requested all-domain synchronization completes.
  const auto tree = pkgplan::package_path::parse("tree");
  const auto tree_file = pkgplan::package_path::parse("tree/file");
  fake_archive recovery_archive({
      pkgapply::test::fixture::directory_entry("tree"),
      pkgapply::test::fixture::regular_entry("tree/file", 6),
  });
  const auto recovery_request =
      pkgapply::installation_application_request::make(
          pkgapply::test::fixture::installation_plan(
              authorities,
              {pkgapply::test::fixture::directory_entry("tree"),
               pkgapply::test::fixture::regular_entry("tree/file", 6)},
              {pkgplan::target_path_observation::absent(tree),
               pkgplan::target_path_observation::absent(tree_file)}),
          context,
          pkgapply::application_execution_control::make(
              pkgapply::application_recovery_requirement::best_effort,
              pkgapply::application_durability_requirement::
                  all_application_domains,
              pkgapply::application_cancellation_policy::
                  recover_after_target_mutation));
  const auto recovery_state =
      state(lease, recovery_request.plan().preconditions());

  const auto interrupt_recovery_request = [&]() {
    backend_state->set_observations(
        matching_observations(recovery_request.plan().preconditions()));
    auto admission = pkgapply::detail::admit_application_engine(
        recovery_request, recovery_state, lease, backend, recovery_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), recovery_request, recovery_state,
        lease, recovery_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), recovery_request, recovery_state, lease,
        recovery_archive);
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), recovery_request, recovery_state,
        lease);
    return pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), recovery_request, recovery_state,
        lease);
  };

  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::unconfirmed);
  {
    auto active = interrupt_recovery_request();
    require(!active.is_complete() && active.interruption() != nullptr &&
                active.interruption()->active_effects().size() == 2,
            "multi-path recovery fixture did not reach interruption");
    backend_state->set_durability(
        pkgapply::application_durability_domain::active_namespace,
        pkgapply::application_durability_status::confirmed);
    auto receipt = pkgapply::detail::recover_application_engine(
        std::move(*active.interruption()), recovery_request, recovery_state,
        lease);
    require(receipt.outcome() ==
                pkgapply::application_attempt_outcome::
                    failed_fully_recovered &&
                receipt.recovery() ==
                    pkgapply::application_recovery_state::
                        exact_prior_state_restored &&
                receipt.paths().size() == 2 &&
                receipt.paths()[0].after().state() ==
                    pkgapply::fact_state::not_applicable &&
                receipt.paths()[1].after().state() ==
                    pkgapply::fact_state::not_applicable,
            "completed reverse recovery did not restore the admitted state");
    std::vector<pkgplan::package_path> recovered_paths;
    for (const auto& event : backend_state->events()) {
      if (event.boundary == boundary::recover && event.path)
        recovered_paths.push_back(*event.path);
    }
    require(recovered_paths.size() == 2 &&
                recovered_paths[0] == tree_file &&
                recovered_paths[1] == tree &&
                backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::recovered &&
                backend_state->published_journal()->receipt() ==
                    receipt.identity(),
            "recovery did not reverse the active dependency order");
  }
  backend_state->clear_events();

  // A failed recovery stops the reverse walk and reports known residual
  // effects without claiming a restored or publication-eligible target.
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::unconfirmed);
  backend_state->set_outcome(
      boundary::recover, pkgapply::backend_operation_outcome::failed);
  {
    auto active = interrupt_recovery_request();
    auto receipt = pkgapply::detail::recover_application_engine(
        std::move(*active.interruption()), recovery_request, recovery_state,
        lease);
    require(receipt.outcome() ==
                pkgapply::application_attempt_outcome::
                    failed_with_partial_effects &&
                receipt.recovery() ==
                    pkgapply::application_recovery_state::
                        known_residual_effects &&
                receipt.paths().size() == 2 &&
                receipt.paths()[0].after().state() ==
                    pkgapply::fact_state::unknown &&
                receipt.paths()[1].after().state() ==
                    pkgapply::fact_state::unknown &&
                backend_state->published_journal().has_value() &&
                backend_state->published_journal()->state() ==
                    pkgapply::application_journal_state::effects_visible,
            "failed reverse recovery concealed residual target effects");
  }
  backend_state->set_outcome(
      boundary::recover, pkgapply::backend_operation_outcome::completed);
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::confirmed);
  backend_state->clear_events();

  // Recovery policy none resolves a durability interruption without issuing
  // backend recovery commands or pretending recovery assets exist.
  const auto no_recovery_request =
      pkgapply::installation_application_request::make(
          install_request.plan(), context,
          pkgapply::application_execution_control::make(
              pkgapply::application_recovery_requirement::none,
              pkgapply::application_durability_requirement::
                  all_application_domains,
              pkgapply::application_cancellation_policy::
                  recover_after_target_mutation));
  const auto no_recovery_state =
      state(lease, no_recovery_request.plan().preconditions());
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::unconfirmed);
  backend_state->set_observations(
      matching_observations(no_recovery_request.plan().preconditions()));
  {
    auto admission = pkgapply::detail::admit_application_engine(
        no_recovery_request, no_recovery_state, lease, backend,
        install_archive);
    auto journaled = pkgapply::detail::journal_application_engine(
        std::move(*admission.admitted()), no_recovery_request,
        no_recovery_state, lease, install_archive.image());
    auto preparation = pkgapply::detail::prepare_application_engine(
        std::move(journaled), no_recovery_request, no_recovery_state, lease,
        install_archive);
    auto publication = pkgapply::detail::publish_rejected_application_engine(
        std::move(*preparation.prepared()), no_recovery_request,
        no_recovery_state, lease);
    auto active = pkgapply::detail::execute_active_application_engine(
        std::move(*publication.published()), no_recovery_request,
        no_recovery_state, lease);
    const std::size_t recovery_commands_before =
        count_boundary(backend_state->events(), boundary::recover);
    auto receipt = pkgapply::detail::recover_application_engine(
        std::move(*active.interruption()), no_recovery_request,
        no_recovery_state, lease);
    require(receipt.outcome() ==
                pkgapply::application_attempt_outcome::
                    effects_visible_durability_unconfirmed &&
                receipt.recovery() ==
                    pkgapply::application_recovery_state::
                        recovery_not_representable &&
                count_boundary(backend_state->events(), boundary::recover) ==
                    recovery_commands_before,
            "recovery-none control issued or invented recovery");
  }
  backend_state->set_durability(
      pkgapply::application_durability_domain::active_namespace,
      pkgapply::application_durability_status::confirmed);
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
