// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fixtures/checkpoint.h"
#include "fixtures/plan.h"

#include <libpkgapply/admission.h>
#include <libpkgapply/restart.h>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace pkgapply;
namespace fixture = pkgapply::test::fixture;
namespace checkpoint = pkgapply::test::checkpoint_fixture;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

class lease final : public target_mutation_lease {
public:
  lease(mutation_lease_instance_identity identity,
        application_target_context_identity target,
        mutation_exclusion_domain_identity domain)
      : identity_(std::move(identity)),
        target_(std::move(target)),
        domain_(std::move(domain))
  {
  }

  const mutation_lease_instance_identity& identity() const noexcept override
  {
    return identity_;
  }
  const application_target_context_identity& target() const noexcept override
  {
    return target_;
  }
  const mutation_exclusion_domain_identity&
  exclusion_domain() const noexcept override
  {
    return domain_;
  }
  bool held() const noexcept override { return true; }

private:
  mutation_lease_instance_identity identity_;
  application_target_context_identity target_;
  mutation_exclusion_domain_identity domain_;
};

class archive final : public pkgimage::package_archive {
public:
  explicit archive(const incoming_package_authority& incoming)
      : image_(incoming.image().image()),
        receipt_(incoming.image().receipt())
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
  void replay(const pkgimage::entry_selection&,
              pkgimage::payload_sink&) const override
  {
  }

private:
  pkgimage::package_image image_;
  pkgimage::archive_inspection_receipt receipt_;
};

class transaction final : public application_backend_transaction {
public:
  transaction(mutation_backend_identity backend,
              observation_backend_identity observation,
              execution_capability_profile_identity capabilities,
              application_target_context_identity target,
              mutation_lease_instance_identity lease,
              application_attempt_nonce nonce)
      : backend_(std::move(backend)),
        observation_(std::move(observation)),
        capabilities_(std::move(capabilities)),
        target_(std::move(target)),
        lease_(std::move(lease)),
        nonce_(std::move(nonce))
  {
  }

  const mutation_backend_identity& backend() const noexcept override
  {
    return backend_;
  }
  const observation_backend_identity& observation_backend() const noexcept override
  {
    return observation_;
  }
  const execution_capability_profile_identity& capabilities() const noexcept override
  {
    return capabilities_;
  }
  const application_target_context_identity& target() const noexcept override
  {
    return target_;
  }
  const mutation_lease_instance_identity& lease() const noexcept override
  {
    return lease_;
  }
  const application_attempt_nonce& attempt_nonce() const noexcept override
  {
    return nonce_;
  }

  backend_observation_batch observe(
      const std::vector<pkgplan::package_path>&) override
  {
    throw std::logic_error("unused test transaction observation");
  }
  std::unique_ptr<incoming_payload_stage> begin_payload_stage(
      const pkgimage::package_image&,
      const pkgimage::entry_selection&) override
  {
    throw std::logic_error("unused test payload stage");
  }
  old_object_capture_result capture_old(
      const old_object_capture_request&) override
  {
    throw std::logic_error("unused test capture");
  }
  backend_operation_result execute_active(
      const backend_active_effect_request&) override
  {
    throw std::logic_error("unused test active effect");
  }
  rejected_object_publication_result execute_rejected(
      const backend_rejected_effect_request&) override
  {
    throw std::logic_error("unused test rejected effect");
  }
  completed_evidence_publication_result publish_completed_evidence(
      const completed_application_evidence&) override
  {
    throw std::logic_error("unused test completed evidence");
  }
  backend_operation_result recover(const pkgplan::package_path&) override
  {
    throw std::logic_error("unused test recovery");
  }
  application_durability_fact synchronize(
      application_durability_domain) override
  {
    throw std::logic_error("unused test synchronization");
  }
  application_journal_record publish_journal(
      const application_journal_record&) override
  {
    throw std::logic_error("unused test journal publication");
  }

private:
  mutation_backend_identity backend_;
  observation_backend_identity observation_;
  execution_capability_profile_identity capabilities_;
  application_target_context_identity target_;
  mutation_lease_instance_identity lease_;
  application_attempt_nonce nonce_;
};

class drifting_backend final : public application_backend {
public:
  drifting_backend(mutation_backend_identity first_backend,
                   mutation_backend_identity later_backend,
                   observation_backend_identity observation,
                   execution_capability_profile_identity first_capabilities,
                   execution_capability_profile_identity later_capabilities)
      : first_backend_(std::move(first_backend)),
        later_backend_(std::move(later_backend)),
        observation_(std::move(observation)),
        first_capabilities_(std::move(first_capabilities)),
        later_capabilities_(std::move(later_capabilities))
  {
  }

  const mutation_backend_identity& identity() const noexcept override
  {
    return identity_calls_++ == 0 ? first_backend_ : later_backend_;
  }
  const observation_backend_identity& observation_identity() const noexcept override
  {
    return observation_;
  }
  const execution_capability_profile_identity& capabilities() const noexcept override
  {
    return capability_calls_++ == 0 ? first_capabilities_ : later_capabilities_;
  }

  std::unique_ptr<application_backend_transaction> begin_with_incoming_image(
      const package_application_request&,
      target_mutation_lease&,
      const pkgimage::package_image&) override
  {
    throw std::logic_error("unused test backend begin");
  }
  std::unique_ptr<application_backend_transaction> begin_without_incoming_image(
      const package_application_request&,
      target_mutation_lease&) override
  {
    throw std::logic_error("unused test backend begin");
  }

private:
  mutation_backend_identity first_backend_;
  mutation_backend_identity later_backend_;
  observation_backend_identity observation_;
  execution_capability_profile_identity first_capabilities_;
  execution_capability_profile_identity later_capabilities_;
  mutable std::size_t identity_calls_ = 0;
  mutable std::size_t capability_calls_ = 0;
};

lease make_lease(const application_target_context& target)
{
  return lease(
      checkpoint::application_identity<mutation_lease_instance_identity>(20),
      target.identity(), target.mutation_exclusion_domain());
}

lease_bound_state_projection state(
    const lease& held,
    const pkgplan::operation_preconditions& preconditions)
{
  std::vector<projected_path_owners> paths;
  paths.reserve(preconditions.paths().size());
  for (const auto& path : preconditions.paths())
    paths.emplace_back(path.path(), path.owners());
  return lease_bound_state_projection::make(
      held.identity(), preconditions.installed_snapshot(),
      preconditions.ownership_inventory(), state_projection_completeness::complete,
      std::move(paths),
      checkpoint::application_identity<state_projection_evidence_identity>(21));
}

installation_application_request request(const application_target_context& target)
{
  const fixture::planning_authorities authorities(target.target());
  return installation_application_request::make(
      fixture::ordinary_installation(authorities),
      fixture::ordinary_installation_incoming(),
      target,
      checkpoint::control());
}

template<class Function>
void require_admission_error(Function&& function,
                             application_admission_error_code code,
                             std::string_view message)
{
  bool rejected = false;
  try {
    function();
  }
  catch (const application_admission_error& error) {
    rejected = error.code() == code;
  }
  require(rejected, message);
}

template<class Function>
void require_restart_error(Function&& function,
                           application_restart_error_code code,
                           std::string_view message)
{
  bool rejected = false;
  try {
    function();
  }
  catch (const application_restart_error& error) {
    rejected = error.code() == code;
  }
  require(rejected, message);
}

} // namespace

int main()
{
  const auto target = checkpoint::target();
  const auto application = request(target);
  auto held = make_lease(target);
  const auto projection = state(held, application.plan().preconditions());
  const archive replay(application.incoming());
  const auto alternate_capabilities = checkpoint::application_identity<
      execution_capability_profile_identity>(90);
  const auto alternate_backend =
      checkpoint::application_identity<mutation_backend_identity>(91);

  // A provider may not drift its advertised capability authority after static
  // admission and then make the transaction agree with the drifted value.
  drifting_backend capability_drift(
      target.mutation_backend(), target.mutation_backend(),
      target.observation_backend(), target.capabilities(),
      alternate_capabilities);
  validate_application_admission(
      application, projection, held, capability_drift, replay);
  transaction drifted_transaction(
      target.mutation_backend(), target.observation_backend(),
      alternate_capabilities, target.identity(), held.identity(),
      checkpoint::nonce(70));
  require_admission_error(
      [&] {
        validate_backend_transaction(
            target, held, capability_drift, drifted_transaction);
      },
      application_admission_error_code::capability_profile_mismatch,
      "backend capability drift was accepted after target admission");

  const auto alternate_observation = checkpoint::application_identity<
      observation_backend_identity>(92);
  const auto alternate_target = checkpoint::application_identity<
      application_target_context_identity>(93);
  const auto stable_backend = [&] {
    return drifting_backend(
        target.mutation_backend(), target.mutation_backend(),
        target.observation_backend(), target.capabilities(), target.capabilities());
  };

  {
    auto backend = stable_backend();
    transaction foreign(
        alternate_backend, target.observation_backend(), target.capabilities(),
        target.identity(), held.identity(), checkpoint::nonce(71));
    require_admission_error(
        [&] { validate_backend_transaction(target, held, backend, foreign); },
        application_admission_error_code::transaction_backend_mismatch,
        "transaction from another mutation backend was accepted");
  }
  {
    auto backend = stable_backend();
    transaction foreign(
        target.mutation_backend(), alternate_observation, target.capabilities(),
        target.identity(), held.identity(), checkpoint::nonce(72));
    require_admission_error(
        [&] { validate_backend_transaction(target, held, backend, foreign); },
        application_admission_error_code::transaction_observation_backend_mismatch,
        "transaction from another observation backend was accepted");
  }
  {
    auto backend = stable_backend();
    transaction foreign(
        target.mutation_backend(), target.observation_backend(),
        alternate_capabilities, target.identity(), held.identity(),
        checkpoint::nonce(73));
    require_admission_error(
        [&] { validate_backend_transaction(target, held, backend, foreign); },
        application_admission_error_code::transaction_capability_mismatch,
        "transaction with foreign capability evidence was accepted");
  }
  {
    auto backend = stable_backend();
    transaction foreign(
        target.mutation_backend(), target.observation_backend(),
        target.capabilities(), alternate_target, held.identity(),
        checkpoint::nonce(74));
    require_admission_error(
        [&] { validate_backend_transaction(target, held, backend, foreign); },
        application_admission_error_code::transaction_target_mismatch,
        "transaction for another target was accepted");
  }

  // Restart authority comes from the immutable target context, not a later
  // backend identity callback. A journal from another mutation backend remains
  // foreign even if a defective provider changes its reported identity.
  const auto valid_journal = checkpoint::journal(application);
  const auto foreign_attempt = application_attempt::make(
      application.identity(), target.identity(), alternate_backend,
      valid_journal.header().attempt().nonce());
  const auto foreign_header = application_journal_header::make(
      valid_journal.header().kind(), application.identity(),
      application.plan().identity(), foreign_attempt, target.identity(),
      application.control().identity(), valid_journal.header().state_projection(),
      valid_journal.header().lease(), alternate_backend);
  const auto foreign_journal = application_journal_record::make(
      foreign_header, valid_journal.state(), valid_journal.effects(),
      valid_journal.events());

  drifting_backend identity_drift(
      target.mutation_backend(), alternate_backend,
      target.observation_backend(), target.capabilities(), target.capabilities());
  require_restart_error(
      [&] {
        validate_application_restart(
            application, projection, held, identity_drift, foreign_journal,
            replay);
      },
      application_restart_error_code::journal_backend_mismatch,
      "restart accepted a journal from a drifted backend identity");

  return 0;
}
