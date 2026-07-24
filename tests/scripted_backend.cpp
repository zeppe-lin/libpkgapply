// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "scripted_backend.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace pkgapply::test {
namespace {

void
normalize_observations(
    std::vector<application_path_observation>& observations)
{
  std::sort(observations.begin(), observations.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.path() < rhs.path();
            });
  if (std::adjacent_find(
          observations.begin(), observations.end(),
          [](const auto& lhs, const auto& rhs) {
            return lhs.path() == rhs.path();
          }) != observations.end())
  {
    throw std::invalid_argument("duplicate scripted observation path");
  }
}

rejected_object_record_identity
rejected_record_identity(const pkgplan::package_path& path)
{
  rejected_object_record_identity::byte_array bytes{};
  std::size_t index = 0;
  for (const unsigned char byte : path.string()) {
    const std::size_t slot = index % bytes.size();
    bytes[slot] = static_cast<std::uint8_t>(
        (static_cast<unsigned int>(bytes[slot]) * 33U) ^ byte);
    ++index;
  }
  bytes.back() ^= static_cast<std::uint8_t>(path.string().size() & 0xffU);

  static constexpr char hexadecimal[] = "0123456789abcdef";
  std::string canonical = "v1:sha256:";
  canonical.reserve(canonical.size() + bytes.size() * 2);
  for (const std::uint8_t byte : bytes) {
    canonical.push_back(hexadecimal[(byte >> 4U) & 0x0fU]);
    canonical.push_back(hexadecimal[byte & 0x0fU]);
  }
  return rejected_object_record_identity::parse(canonical);
}

} // namespace

class scripted_payload_stage final : public incoming_payload_stage {
public:
  scripted_payload_stage(
      std::shared_ptr<scripted_backend_state> state,
      application_backend_evidence_identity evidence)
      : state_(std::move(state)), evidence_(std::move(evidence))
  {
  }

  ~scripted_payload_stage() override = default;

  void begin(const pkgimage::package_entry&) override
  {
    state_->record(scripted_backend_boundary::payload_begin);
    state_->maybe_throw(scripted_backend_boundary::payload_begin);
  }

  void write(const pkgimage::package_entry&,
             const std::byte*,
             std::size_t) override
  {
    state_->record(scripted_backend_boundary::payload_write);
    state_->maybe_throw(scripted_backend_boundary::payload_write);
  }

  void end(const pkgimage::package_entry&) override
  {
    state_->record(scripted_backend_boundary::payload_end);
    state_->maybe_throw(scripted_backend_boundary::payload_end);
  }

  backend_operation_result seal() override
  {
    state_->record(scripted_backend_boundary::payload_seal);
    state_->maybe_throw(scripted_backend_boundary::payload_seal);
    const backend_operation_outcome outcome =
        state_->outcome(scripted_backend_boundary::payload_seal);
    sealed_ = outcome == backend_operation_outcome::completed;
    backend_operation_result result(outcome, {evidence_});
    state_->incoming_payload_ = result;
    state_->retain_durability(
        application_durability_domain::incoming_staging,
        outcome == backend_operation_outcome::completed
            ? application_durability_status::visible
            : outcome == backend_operation_outcome::indeterminate
                ? application_durability_status::indeterminate
                : application_durability_status::unconfirmed);
    return result;
  }

  void abandon() noexcept override
  {
    try {
      state_->record(scripted_backend_boundary::payload_abandon);
    } catch (...) {
    }
  }

  bool sealed() const noexcept override
  {
    return sealed_;
  }

private:
  std::shared_ptr<scripted_backend_state> state_;
  application_backend_evidence_identity evidence_;
  bool sealed_ = false;
};

class scripted_backend_transaction final
    : public application_backend_transaction {
public:
  scripted_backend_transaction(
      mutation_backend_identity backend,
      observation_backend_identity observation,
      execution_capability_profile_identity capabilities,
      application_target_context_identity target,
      mutation_lease_instance_identity lease,
      application_attempt_nonce nonce,
      application_backend_evidence_identity evidence,
      bool has_incoming_image,
      std::optional<application_journal_record_identity> resumed_journal,
      std::shared_ptr<scripted_backend_state> state)
      : backend_(std::move(backend)),
        observation_(std::move(observation)),
        capabilities_(std::move(capabilities)),
        target_(std::move(target)),
        lease_(std::move(lease)),
        nonce_(std::move(nonce)),
        evidence_(std::move(evidence)),
        has_incoming_image_(has_incoming_image),
        resumed_journal_(std::move(resumed_journal)),
        state_(std::move(state))
  {
    if (state_->transaction_alive_)
      throw std::logic_error("scripted backend transaction already active");
    state_->transaction_alive_ = true;
  }

  ~scripted_backend_transaction() override
  {
    try {
      state_->record(scripted_backend_boundary::transaction_destroyed);
    } catch (...) {
    }
    state_->transaction_alive_ = false;
  }

  const mutation_backend_identity& backend() const noexcept override
  {
    return backend_;
  }

  const observation_backend_identity&
  observation_backend() const noexcept override
  {
    return observation_;
  }

  const execution_capability_profile_identity&
  capabilities() const noexcept override
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

  std::optional<application_journal_record_identity>
  resumed_journal() const noexcept override
  {
    return resumed_journal_;
  }

  application_restart_checkpoint restart_checkpoint(
      const application_journal_record& journal) override
  {
    if (!resumed_journal_ || *resumed_journal_ != journal.identity())
      throw std::logic_error("scripted checkpoint names another journal");
    if (!state_->admitted_observations_)
      throw std::logic_error("scripted checkpoint lacks admitted observations");
    return application_restart_checkpoint::make(
        journal.identity(), *state_->admitted_observations_,
        state_->incoming_payload_, state_->restart_captures_,
        state_->restart_rejected_effects_, state_->restart_active_effects_,
        state_->restart_recovery_effects_,
        state_->restart_synchronizations_, state_->checkpoint_durability(),
        {evidence_},
        state_->published_completed_evidence_);
  }

  backend_observation_batch observe(
      const std::vector<pkgplan::package_path>& paths) override
  {
    state_->record(scripted_backend_boundary::observe);
    state_->maybe_throw(scripted_backend_boundary::observe);
    state_->select_observations_for_next_batch();
    std::vector<application_path_observation> observations;
    observations.reserve(paths.size());
    for (const pkgplan::package_path& path : paths) {
      const application_path_observation* observation =
          state_->find_observation(path);
      if (observation != nullptr)
        observations.push_back(*observation);
    }
    backend_observation_batch batch = backend_observation_batch::make(
        paths, std::move(observations), {evidence_});
    if (!resumed_journal_ && !state_->admitted_observations_)
      state_->admitted_observations_ = batch;
    return batch;
  }

  std::unique_ptr<incoming_payload_stage> begin_payload_stage(
      const pkgimage::package_image&,
      const pkgimage::entry_selection&) override
  {
    state_->record(scripted_backend_boundary::begin_payload_stage);
    state_->maybe_throw(scripted_backend_boundary::begin_payload_stage);
    if (!has_incoming_image_)
      throw std::logic_error(
          "removal transaction cannot stage incoming payloads");
    return std::make_unique<scripted_payload_stage>(state_, evidence_);
  }

  old_object_capture_result capture_old(
      const old_object_capture_request& request) override
  {
    state_->record(scripted_backend_boundary::capture_old, request.path());
    state_->maybe_throw(scripted_backend_boundary::capture_old);
    const application_path_observation* observation =
        state_->find_observation(request.path());
    application_path_observation captured = observation == nullptr
        ? application_path_observation::unknown(request.path())
        : *observation;
    const backend_operation_outcome outcome =
        state_->outcome(scripted_backend_boundary::capture_old);
    old_object_capture_result result(
        outcome,
        std::move(captured),
        outcome == backend_operation_outcome::completed &&
            state_->exact_recovery_possible_,
        {evidence_});
    state_->retain_capture(application_restart_capture(result));
    state_->retain_durability(
        application_durability_domain::recovery_staging,
        outcome == backend_operation_outcome::completed
            ? application_durability_status::visible
            : outcome == backend_operation_outcome::indeterminate
                ? application_durability_status::indeterminate
                : application_durability_status::unconfirmed);
    return result;
  }

  backend_operation_result execute_active(
      const backend_active_effect_request& request) override
  {
    state_->record(scripted_backend_boundary::execute_active, request.path());
    state_->maybe_throw(scripted_backend_boundary::execute_active);
    backend_operation_result result(
        state_->outcome(scripted_backend_boundary::execute_active),
        {evidence_});
    state_->retain_active(
        application_restart_active_effect(request.path(), result));
    if (result.outcome() == backend_operation_outcome::completed) {
      state_->retain_durability(
          application_durability_domain::active_namespace,
          application_durability_status::visible);
    }
    else if (result.outcome() == backend_operation_outcome::indeterminate) {
      state_->retain_durability(
          application_durability_domain::active_namespace,
          application_durability_status::indeterminate);
    }
    return result;
  }

  rejected_object_publication_result execute_rejected(
      const backend_rejected_effect_request& request) override
  {
    state_->record(scripted_backend_boundary::execute_rejected, request.path());
    state_->maybe_throw(scripted_backend_boundary::execute_rejected);
    const backend_operation_outcome outcome =
        state_->outcome(scripted_backend_boundary::execute_rejected);
    rejected_object_publication_result result(
        outcome,
        outcome == backend_operation_outcome::completed
            ? std::optional<rejected_object_record_identity>(
                  rejected_record_identity(request.path()))
            : std::nullopt,
        {evidence_});
    state_->retain_rejected(
        application_restart_rejected_effect(request.path(), result));
    state_->retain_durability(
        application_durability_domain::rejected_object_store,
        outcome == backend_operation_outcome::completed
            ? application_durability_status::visible
            : outcome == backend_operation_outcome::indeterminate
                ? application_durability_status::indeterminate
                : application_durability_status::unconfirmed);
    return result;
  }

  completed_evidence_publication_result publish_completed_evidence(
      const completed_application_evidence& evidence) override
  {
    state_->record(scripted_backend_boundary::publish_completed_evidence);
    state_->maybe_throw(
        scripted_backend_boundary::publish_completed_evidence);
    const backend_operation_outcome outcome = state_->outcome(
        scripted_backend_boundary::publish_completed_evidence);
    if (outcome == backend_operation_outcome::completed)
      state_->published_completed_evidence_ = evidence;
    state_->retain_durability(
        application_durability_domain::completed_evidence,
        outcome == backend_operation_outcome::completed
            ? application_durability_status::visible
            : outcome == backend_operation_outcome::indeterminate
                ? application_durability_status::indeterminate
                : application_durability_status::unconfirmed);
    return completed_evidence_publication_result(
        outcome,
        outcome == backend_operation_outcome::completed
            ? std::optional<completed_application_evidence_identity>(
                  evidence.identity())
            : std::nullopt,
        {evidence_});
  }

  backend_operation_result recover(
      const pkgplan::package_path& path) override
  {
    state_->record(scripted_backend_boundary::recover, path);
    state_->maybe_throw(scripted_backend_boundary::recover);
    backend_operation_result result(
        state_->outcome(scripted_backend_boundary::recover), {evidence_});
    state_->retain_recovery(
        application_restart_recovery_effect(path, result));
    return result;
  }

  application_durability_fact synchronize(
      application_durability_domain domain) override
  {
    state_->record(scripted_backend_boundary::synchronize,
                   std::nullopt,
                   domain);
    state_->maybe_throw(scripted_backend_boundary::synchronize);
    const application_durability_status status = state_->durability(domain);
    application_durability_fact result(domain, status);
    state_->retain_durability(domain, status);
    state_->retain_synchronization(result);
    return result;
  }

  application_journal_record publish_journal(
      const application_journal_record& record) override
  {
    state_->record(scripted_backend_boundary::publish_journal);
    state_->maybe_throw(scripted_backend_boundary::publish_journal);
    state_->published_journal_ = record;
    const auto current = state_->established_durability_.find(
        application_durability_domain::journal);
    if (current == state_->established_durability_.end() ||
        current->second == application_durability_status::not_attempted)
    {
      state_->retain_durability(
          application_durability_domain::journal,
          application_durability_status::visible);
    }
    return *state_->published_journal_;
  }

private:
  mutation_backend_identity backend_;
  observation_backend_identity observation_;
  execution_capability_profile_identity capabilities_;
  application_target_context_identity target_;
  mutation_lease_instance_identity lease_;
  application_attempt_nonce nonce_;
  application_backend_evidence_identity evidence_;
  bool has_incoming_image_;
  std::optional<application_journal_record_identity> resumed_journal_;
  std::shared_ptr<scripted_backend_state> state_;
};

void
scripted_backend_state::reset_attempt_checkpoint()
{
  published_journal_.reset();
  published_completed_evidence_.reset();
  admitted_observations_.reset();
  incoming_payload_.reset();
  restart_captures_.clear();
  restart_rejected_effects_.clear();
  restart_active_effects_.clear();
  restart_recovery_effects_.clear();
  restart_synchronizations_.clear();
  established_durability_.clear();
}

template<class Value>
void retain_restart_value(std::vector<Value>& values, Value value)
{
  const auto item = std::lower_bound(
      values.begin(), values.end(), value.path(),
      [](const auto& candidate, const auto& path) {
        return candidate.path() < path;
      });
  if (item != values.end() && item->path() == value.path())
    *item = std::move(value);
  else
    values.insert(item, std::move(value));
}

void
scripted_backend_state::retain_capture(application_restart_capture capture)
{
  retain_restart_value(restart_captures_, std::move(capture));
}

void
scripted_backend_state::retain_rejected(
    application_restart_rejected_effect effect)
{
  retain_restart_value(restart_rejected_effects_, std::move(effect));
}

void
scripted_backend_state::retain_active(
    application_restart_active_effect effect)
{
  retain_restart_value(restart_active_effects_, std::move(effect));
}

void
scripted_backend_state::retain_recovery(
    application_restart_recovery_effect effect)
{
  retain_restart_value(restart_recovery_effects_, std::move(effect));
}

void
scripted_backend_state::retain_durability(
    application_durability_domain domain,
    application_durability_status status)
{
  established_durability_[domain] = status;
}

void
scripted_backend_state::retain_synchronization(
    application_durability_fact result)
{
  const auto item = std::lower_bound(
      restart_synchronizations_.begin(), restart_synchronizations_.end(),
      result.domain(), [](const auto& candidate, const auto domain) {
        return candidate.domain() < domain;
      });
  application_restart_synchronization value(std::move(result));
  if (item != restart_synchronizations_.end() &&
      item->domain() == value.domain())
  {
    *item = std::move(value);
  }
  else {
    restart_synchronizations_.insert(item, std::move(value));
  }
}

application_durability_profile
scripted_backend_state::checkpoint_durability() const
{
  std::vector<application_durability_fact> facts;
  facts.reserve(6);
  for (const application_durability_domain domain : {
           application_durability_domain::journal,
           application_durability_domain::incoming_staging,
           application_durability_domain::recovery_staging,
           application_durability_domain::active_namespace,
           application_durability_domain::rejected_object_store,
           application_durability_domain::completed_evidence})
  {
    const auto item = established_durability_.find(domain);
    facts.emplace_back(
        domain, item == established_durability_.end()
            ? application_durability_status::not_attempted
            : item->second);
  }
  return application_durability_profile(std::move(facts));
}

void
scripted_backend_state::set_observations(
    std::vector<application_path_observation> observations)
{
  normalize_observations(observations);
  observations_ = std::move(observations);
  observation_sequence_.clear();
  observation_sequence_index_ = 0;
}

void
scripted_backend_state::set_observation_sequence(
    std::vector<std::vector<application_path_observation>> observations)
{
  for (auto& batch : observations)
    normalize_observations(batch);
  observation_sequence_ = std::move(observations);
  observation_sequence_index_ = 0;
  observations_.clear();
}

void
scripted_backend_state::set_outcome(
    scripted_backend_boundary boundary,
    backend_operation_outcome outcome)
{
  static_cast<void>(backend_operation_result(outcome));
  outcomes_[boundary] = outcome;
}

void
scripted_backend_state::set_durability(
    application_durability_domain domain,
    application_durability_status status)
{
  static_cast<void>(application_durability_fact(domain, status));
  durability_[domain] = status;
}

void
scripted_backend_state::set_exact_recovery_possible(bool possible) noexcept
{
  exact_recovery_possible_ = possible;
}

void
scripted_backend_state::set_transaction_target(
    application_target_context_identity target)
{
  transaction_target_ = std::move(target);
}

void
scripted_backend_state::clear_transaction_target() noexcept
{
  transaction_target_.reset();
}

void
scripted_backend_state::throw_at(scripted_backend_boundary boundary)
{
  throws_.insert(boundary);
}

void
scripted_backend_state::clear_throw(scripted_backend_boundary boundary)
{
  throws_.erase(boundary);
}

const std::vector<scripted_backend_event>&
scripted_backend_state::events() const noexcept
{
  return events_;
}

const std::optional<application_journal_record>&
scripted_backend_state::published_journal() const noexcept
{
  return published_journal_;
}

const std::optional<completed_application_evidence>&
scripted_backend_state::published_completed_evidence() const noexcept
{
  return published_completed_evidence_;
}

bool
scripted_backend_state::transaction_alive() const noexcept
{
  return transaction_alive_;
}

void
scripted_backend_state::clear_events() noexcept
{
  events_.clear();
}

void
scripted_backend_state::record(
    scripted_backend_boundary boundary,
    std::optional<pkgplan::package_path> path,
    std::optional<application_durability_domain> domain)
{
  events_.push_back({boundary, std::move(path), domain});
}

void
scripted_backend_state::maybe_throw(
    scripted_backend_boundary boundary) const
{
  if (throws_.find(boundary) != throws_.end())
    throw std::runtime_error("injected scripted backend failure");
}

backend_operation_outcome
scripted_backend_state::outcome(
    scripted_backend_boundary boundary) const noexcept
{
  const auto item = outcomes_.find(boundary);
  return item == outcomes_.end()
      ? backend_operation_outcome::completed
      : item->second;
}

application_durability_status
scripted_backend_state::durability(
    application_durability_domain domain) const noexcept
{
  const auto item = durability_.find(domain);
  return item == durability_.end()
      ? application_durability_status::confirmed
      : item->second;
}

void
scripted_backend_state::select_observations_for_next_batch()
{
  if (observation_sequence_index_ >= observation_sequence_.size())
    return;
  observations_ = observation_sequence_[observation_sequence_index_];
  ++observation_sequence_index_;
}

const application_path_observation*
scripted_backend_state::find_observation(
    const pkgplan::package_path& path) const noexcept
{
  const auto item = std::lower_bound(
      observations_.begin(), observations_.end(), path,
      [](const auto& observation, const auto& wanted) {
        return observation.path() < wanted;
      });
  return item != observations_.end() && item->path() == path
      ? &*item
      : nullptr;
}

scripted_backend::scripted_backend(
    mutation_backend_identity backend,
    observation_backend_identity observation,
    execution_capability_profile_identity capabilities,
    application_attempt_nonce nonce,
    application_backend_evidence_identity evidence,
    std::shared_ptr<scripted_backend_state> state)
    : backend_(std::move(backend)),
      observation_(std::move(observation)),
      capabilities_(std::move(capabilities)),
      nonce_(std::move(nonce)),
      evidence_(std::move(evidence)),
      state_(std::move(state))
{
  if (!state_)
    throw std::invalid_argument("scripted backend requires shared state");
}

const mutation_backend_identity&
scripted_backend::identity() const noexcept
{
  return backend_;
}

const observation_backend_identity&
scripted_backend::observation_identity() const noexcept
{
  return observation_;
}

const execution_capability_profile_identity&
scripted_backend::capabilities() const noexcept
{
  return capabilities_;
}

std::unique_ptr<application_backend_transaction>
scripted_backend::begin_with_incoming_image(
    const application_target_context& target,
    target_mutation_lease& lease,
    const pkgimage::package_image&)
{
  return begin(target,
               lease,
               true,
               scripted_backend_boundary::begin_with_incoming_image);
}

std::unique_ptr<application_backend_transaction>
scripted_backend::begin_without_incoming_image(
    const application_target_context& target,
    target_mutation_lease& lease)
{
  return begin(target,
               lease,
               false,
               scripted_backend_boundary::begin_without_incoming_image);
}

std::unique_ptr<application_backend_transaction>
scripted_backend::resume_with_incoming_image(
    const application_target_context& target,
    target_mutation_lease& lease,
    const application_journal_record& journal,
    const pkgimage::package_image&)
{
  return begin(target,
               lease,
               true,
               scripted_backend_boundary::resume_with_incoming_image,
               journal.identity());
}

std::unique_ptr<application_backend_transaction>
scripted_backend::resume_without_incoming_image(
    const application_target_context& target,
    target_mutation_lease& lease,
    const application_journal_record& journal)
{
  return begin(target,
               lease,
               false,
               scripted_backend_boundary::resume_without_incoming_image,
               journal.identity());
}

std::unique_ptr<application_backend_transaction>
scripted_backend::begin(
    const application_target_context& target,
    target_mutation_lease& lease,
    bool has_incoming_image,
    scripted_backend_boundary boundary,
    std::optional<application_journal_record_identity> resumed_journal)
{
  if (!resumed_journal)
    state_->reset_attempt_checkpoint();
  state_->record(boundary);
  state_->maybe_throw(boundary);
  return std::make_unique<scripted_backend_transaction>(
      backend_,
      observation_,
      capabilities_,
      state_->transaction_target_
          ? *state_->transaction_target_
          : target.identity(),
      lease.identity(),
      nonce_,
      evidence_,
      has_incoming_image,
      std::move(resumed_journal),
      state_);
}

} // namespace pkgapply::test
