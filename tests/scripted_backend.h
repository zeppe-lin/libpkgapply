// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include <libpkgapply/backend.h>

namespace pkgapply::test {

/*! \brief Named mechanism boundary exposed by the deterministic test backend. */
enum class scripted_backend_boundary {
  begin_with_incoming_image,
  begin_without_incoming_image,
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
  recover,
  synchronize,
  publish_journal,
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
  [[nodiscard]] const std::optional<application_journal_record>&
  published_journal() const noexcept;
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
  [[nodiscard]] const application_path_observation*
  find_observation(const pkgplan::package_path& path) const noexcept;

  std::vector<application_path_observation> observations_;
  std::map<scripted_backend_boundary, backend_operation_outcome> outcomes_;
  std::map<application_durability_domain, application_durability_status>
      durability_;
  std::set<scripted_backend_boundary> throws_;
  std::vector<scripted_backend_event> events_;
  std::optional<application_journal_record> published_journal_;
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
      const application_target_context& target,
      target_mutation_lease& lease,
      const pkgimage::package_image& incoming_image) override;

  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  begin_without_incoming_image(
      const application_target_context& target,
      target_mutation_lease& lease) override;

private:
  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  begin(const application_target_context& target,
        target_mutation_lease& lease,
        bool has_incoming_image,
        scripted_backend_boundary boundary);

  mutation_backend_identity backend_;
  observation_backend_identity observation_;
  execution_capability_profile_identity capabilities_;
  application_attempt_nonce nonce_;
  application_backend_evidence_identity evidence_;
  std::shared_ptr<scripted_backend_state> state_;
};

} // namespace pkgapply::test
