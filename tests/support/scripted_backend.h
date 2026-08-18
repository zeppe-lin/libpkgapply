// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgapply/restart.h>

namespace pkgapply::test {

/*! \brief Named mechanism boundary exposed by the deterministic test backend. */
enum class scripted_backend_boundary {
  begin_with_incoming_image,
  begin_without_incoming_image,
  resume_with_incoming_image,
  resume_without_incoming_image,
  observe,
  begin_payload_stage,
  payload_begin,
  payload_write,
  payload_end,
  payload_seal,
  payload_abandon,
  capture_old,
  execute_active,
  execute_rejected,
  publish_completed_evidence,
  recover,
  synchronize,
  transaction_destroyed,
};

/*! \brief One ordered backend event retained for semantic assertions. */
struct scripted_backend_event final {
  scripted_backend_boundary boundary;
  std::optional<pkgplan::package_path> path;
  std::optional<application_durability_domain> durability_domain;
};

/*! \brief Shared deterministic mechanism state used by backend transactions. */
class scripted_backend_state final {
public:
  void set_observations(
      std::vector<application_path_observation> observations);
  void set_observation_sequence(
      std::vector<std::vector<application_path_observation>> observations);
  void set_outcome(scripted_backend_boundary boundary,
                   backend_operation_outcome outcome);
  void set_durability(application_durability_domain domain,
                      application_durability_status status);
  void set_exact_recovery_possible(bool possible) noexcept;
  void set_transaction_target(
      application_target_context_identity target);
  void clear_transaction_target() noexcept;
  void throw_at(scripted_backend_boundary boundary);
  void clear_throw(scripted_backend_boundary boundary);

  [[nodiscard]] const std::vector<scripted_backend_event>& events() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence>&
  published_completed_evidence() const noexcept;
  [[nodiscard]] bool transaction_alive() const noexcept;
  void clear_events() noexcept;

private:
  friend class scripted_backend;
  friend class scripted_backend_transaction;
  friend class scripted_payload_stage;

  void record(scripted_backend_boundary boundary,
              std::optional<pkgplan::package_path> path = std::nullopt,
              std::optional<application_durability_domain> domain = std::nullopt);
  void maybe_throw(scripted_backend_boundary boundary) const;
  [[nodiscard]] backend_operation_outcome
  outcome(scripted_backend_boundary boundary) const noexcept;
  [[nodiscard]] application_durability_status
  durability(application_durability_domain domain) const noexcept;
  void select_observations_for_next_batch();
  [[nodiscard]] const application_path_observation*
  find_observation(const pkgplan::package_path& path) const noexcept;
  void reset_attempt_checkpoint();
  void retain_capture(application_restart_capture capture);
  void retain_rejected(application_restart_rejected_effect effect);
  void retain_active(application_restart_active_effect effect);
  void retain_recovery(application_restart_recovery_effect effect);
  void retain_durability(application_durability_domain domain,
                         application_durability_status status);
  void retain_synchronization(application_durability_fact result);
  [[nodiscard]] application_durability_profile
  checkpoint_durability() const;

  std::vector<application_path_observation> observations_;
  std::vector<std::vector<application_path_observation>>
      observation_sequence_;
  std::size_t observation_sequence_index_ = 0;
  std::map<scripted_backend_boundary, backend_operation_outcome> outcomes_;
  std::map<application_durability_domain, application_durability_status>
      durability_;
  std::set<scripted_backend_boundary> throws_;
  std::vector<scripted_backend_event> events_;
  std::optional<completed_application_evidence> published_completed_evidence_;
  std::optional<backend_observation_batch> admitted_observations_;
  std::optional<backend_operation_result> incoming_payload_;
  std::vector<application_restart_capture> restart_captures_;
  std::vector<application_restart_rejected_effect> restart_rejected_effects_;
  std::vector<application_restart_active_effect> restart_active_effects_;
  std::vector<application_restart_recovery_effect> restart_recovery_effects_;
  std::vector<application_restart_synchronization>
      restart_synchronizations_;
  std::map<application_durability_domain, application_durability_status>
      established_durability_;
  bool exact_recovery_possible_ = true;
  std::optional<application_target_context_identity>
      transaction_target_;
  bool transaction_alive_ = false;
};

/*! \brief Deterministic backend for engine sequencing and failure injection. */
class scripted_backend final : public application_backend {
public:
  scripted_backend(
      mutation_backend_identity backend,
      observation_backend_identity observation,
      execution_capability_profile_identity capabilities,
      application_attempt_nonce nonce,
      application_backend_evidence_identity evidence,
      std::shared_ptr<scripted_backend_state> state);

  [[nodiscard]] const mutation_backend_identity&
  identity() const noexcept override;
  [[nodiscard]] const observation_backend_identity&
  observation_identity() const noexcept override;
  [[nodiscard]] const execution_capability_profile_identity&
  capabilities() const noexcept override;

  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  begin_with_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease,
      const pkgimage::package_image& incoming_image) override;

  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  begin_without_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease) override;

  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  resume_with_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease,
      const application_journal_record& journal,
      const pkgimage::package_image& incoming_image) override;

  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  resume_without_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease,
      const application_journal_record& journal) override;

private:
  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  begin(const package_application_request& request,
        target_mutation_lease& lease,
        bool has_incoming_image,
        scripted_backend_boundary boundary,
        std::optional<application_journal_record_identity>
            resumed_journal = std::nullopt);

  mutation_backend_identity backend_;
  observation_backend_identity observation_;
  execution_capability_profile_identity capabilities_;
  application_attempt_nonce nonce_;
  application_backend_evidence_identity evidence_;
  std::shared_ptr<scripted_backend_state> state_;
};

} // namespace pkgapply::test
