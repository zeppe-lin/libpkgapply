// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <libpkgapply/admission.h>
#include <libpkgapply/attempt.h>
#include <libpkgapply/precondition.h>
#include <libpkgapply/result.h>
#include <libpkgapply/restart.h>
#include <libpkgapply/schedule.h>
#include <libpkgapply/journal.h>
#include <libpkgimage/package_archive.h>

#include "journal_history.h"

namespace pkgapply::detail {

/*! \brief One admitted, freshly revalidated backend transaction.
 *
 * This is an internal engine value.  It keeps the exact transaction alive
 * after static authority admission and fresh precondition observation, so the
 * next engine phase cannot reopen the backend or obtain another attempt nonce.
 */
class admitted_application final {
public:
  admitted_application(
      application_attempt attempt,
      application_precondition_check preconditions,
      std::unique_ptr<application_backend_transaction> transaction);

  admitted_application(const admitted_application&) = delete;
  admitted_application& operator=(const admitted_application&) = delete;
  admitted_application(admitted_application&&) noexcept = default;
  admitted_application& operator=(admitted_application&&) noexcept = default;

  [[nodiscard]] const application_attempt& attempt() const noexcept;
  [[nodiscard]] const application_precondition_check&
  preconditions() const noexcept;
  [[nodiscard]] application_backend_transaction& transaction() noexcept;
  [[nodiscard]] const application_backend_transaction&
  transaction() const noexcept;

private:
  application_attempt attempt_;
  application_precondition_check preconditions_;
  std::unique_ptr<application_backend_transaction> transaction_;
};

/*! \brief Refusal or one transaction admitted to the mutation engine. */
class application_engine_admission final {
public:
  [[nodiscard]] static application_engine_admission
  refused(application_receipt receipt);

  [[nodiscard]] static application_engine_admission
  admitted(application_attempt attempt,
           application_precondition_check preconditions,
           std::unique_ptr<application_backend_transaction> transaction);

  application_engine_admission(const application_engine_admission&) = delete;
  application_engine_admission& operator=(
      const application_engine_admission&) = delete;
  application_engine_admission(
      application_engine_admission&&) noexcept = default;
  application_engine_admission& operator=(
      application_engine_admission&&) noexcept = default;

  [[nodiscard]] bool is_admitted() const noexcept;
  [[nodiscard]] const application_receipt* refusal() const noexcept;
  [[nodiscard]] admitted_application* admitted() noexcept;
  [[nodiscard]] const admitted_application* admitted() const noexcept;

private:
  using value_type = std::variant<application_receipt, admitted_application>;
  explicit application_engine_admission(value_type value);
  value_type value_;
};

/*! \brief Admit and freshly revalidate one installation transaction. */
[[nodiscard]] application_engine_admission
admit_application_engine(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Admit and freshly revalidate one upgrade transaction. */
[[nodiscard]] application_engine_admission
admit_application_engine(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Admit and freshly revalidate one removal transaction. */
[[nodiscard]] application_engine_admission
admit_application_engine(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend);

} // namespace pkgapply::detail

namespace pkgapply::detail {

/*! \brief One exact durable attempt reopened under a new outer lease. */
class reopened_application final {
public:
  reopened_application(
      application_attempt attempt,
      application_restart_assessment assessment,
      application_journal_history history,
      application_journal_store& store,
      application_restart_view restart,
      std::unique_ptr<application_backend_transaction> transaction);

  reopened_application(const reopened_application&) = delete;
  reopened_application& operator=(const reopened_application&) = delete;
  reopened_application(reopened_application&&) noexcept = default;
  reopened_application& operator=(reopened_application&&) noexcept = default;

  [[nodiscard]] const application_attempt& attempt() const noexcept;
  [[nodiscard]] const application_restart_assessment&
  assessment() const noexcept;
  [[nodiscard]] const application_journal_history& journal() const noexcept;
  [[nodiscard]] const application_restart_view&
  restart_view() const noexcept;
  [[nodiscard]] application_backend_transaction& transaction() noexcept;
  [[nodiscard]] const application_backend_transaction&
  transaction() const noexcept;
  [[nodiscard]] std::unique_ptr<application_backend_transaction>
  release_transaction() noexcept;
  [[nodiscard]] application_journal_history release_history() noexcept;
  [[nodiscard]] application_journal_store& journal_store() noexcept;

private:
  application_attempt attempt_;
  application_restart_assessment assessment_;
  application_journal_history history_;
  application_journal_store* store_;
  application_restart_view restart_;
  std::unique_ptr<application_backend_transaction> transaction_;
};

[[nodiscard]] reopened_application reopen_application_engine(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    application_journal_store& journal_store,
    const application_journal_declaration_identity& declaration,
    const pkgimage::package_archive& archive);

[[nodiscard]] reopened_application reopen_application_engine(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    application_journal_store& journal_store,
    const application_journal_declaration_identity& declaration,
    const pkgimage::package_archive& archive);

[[nodiscard]] reopened_application reopen_application_engine(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    application_journal_store& journal_store,
    const application_journal_declaration_identity& declaration);

/*! \brief One admitted transaction with its complete durable effect graph. */
class journaled_application final {
public:
  journaled_application(
      admitted_application admitted,
      std::optional<incoming_payload_plan> payloads,
      old_object_capture_plan captures,
      application_effect_schedule schedule,
      application_journal_history history,
      application_journal_store& store,
      lease_bound_state_projection_identity state_projection,
      mutation_lease_instance_identity lease);

  journaled_application(const journaled_application&) = delete;
  journaled_application& operator=(const journaled_application&) = delete;
  journaled_application(journaled_application&&) noexcept = default;
  journaled_application& operator=(journaled_application&&) noexcept = default;

  [[nodiscard]] admitted_application& admitted() noexcept;
  [[nodiscard]] const admitted_application& admitted() const noexcept;
  [[nodiscard]] const std::optional<incoming_payload_plan>&
  payloads() const noexcept;
  [[nodiscard]] const old_object_capture_plan& captures() const noexcept;
  [[nodiscard]] const application_effect_schedule& schedule() const noexcept;
  [[nodiscard]] const application_journal_history& journal() const noexcept;
  [[nodiscard]] const lease_bound_state_projection_identity&
  state_projection() const noexcept;
  [[nodiscard]] const mutation_lease_instance_identity& lease() const noexcept;

  /*! \brief Append and durably commit one owner-authored journal transition. */
  void append_journal_step(
      application_journal_state state,
      std::optional<application_journal_event> event = std::nullopt,
      application_journal_replay_encoding replay_fact = {},
      std::optional<application_receipt_identity> receipt = std::nullopt,
      std::optional<completed_application_evidence_identity>
          completed_evidence = std::nullopt);

private:
  admitted_application admitted_;
  std::optional<incoming_payload_plan> payloads_;
  old_object_capture_plan captures_;
  application_effect_schedule schedule_;
  application_journal_history history_;
  application_journal_store* store_;
  lease_bound_state_projection_identity state_projection_;
  mutation_lease_instance_identity lease_;
};

[[nodiscard]] journaled_application journal_application_engine(
    admitted_application admitted,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    application_journal_store& store,
    const pkgimage::package_image& image);

[[nodiscard]] journaled_application journal_application_engine(
    admitted_application admitted,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    application_journal_store& store,
    const pkgimage::package_image& image);

[[nodiscard]] journaled_application journal_application_engine(
    admitted_application admitted,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    application_journal_store& store);

/*! \brief One transaction whose private preparation domains are durable. */
class prepared_application final {
public:
  prepared_application(
      journaled_application journaled,
      std::vector<old_object_capture_result> captures,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  prepared_application(const prepared_application&) = delete;
  prepared_application& operator=(const prepared_application&) = delete;
  prepared_application(prepared_application&&) noexcept = default;
  prepared_application& operator=(prepared_application&&) noexcept = default;

  [[nodiscard]] journaled_application& journaled() noexcept;
  [[nodiscard]] const journaled_application& journaled() const noexcept;
  [[nodiscard]] const std::vector<old_object_capture_result>&
  captures() const noexcept;
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  journaled_application journaled_;
  std::vector<old_object_capture_result> captures_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

/*! \brief Pre-mutation failure or one fully prepared transaction. */
class application_engine_preparation final {
public:
  [[nodiscard]] static application_engine_preparation
  failed(application_receipt receipt);

  [[nodiscard]] static application_engine_preparation
  prepared(journaled_application journaled,
           std::vector<old_object_capture_result> captures,
           application_durability_profile durability,
           std::vector<application_backend_evidence_identity>
               backend_evidence);

  application_engine_preparation(const application_engine_preparation&) = delete;
  application_engine_preparation& operator=(
      const application_engine_preparation&) = delete;
  application_engine_preparation(
      application_engine_preparation&&) noexcept = default;
  application_engine_preparation& operator=(
      application_engine_preparation&&) noexcept = default;

  [[nodiscard]] bool is_prepared() const noexcept;
  [[nodiscard]] const application_receipt* failure() const noexcept;
  [[nodiscard]] prepared_application* prepared() noexcept;
  [[nodiscard]] const prepared_application* prepared() const noexcept;

private:
  using value_type = std::variant<application_receipt, prepared_application>;
  explicit application_engine_preparation(value_type value);
  value_type value_;
};

/*! \brief Capture and stage installation inputs without target mutation. */
[[nodiscard]] application_engine_preparation
prepare_application_engine(
    journaled_application journaled,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive);

/*! \brief Capture and stage upgrade inputs without target mutation. */
[[nodiscard]] application_engine_preparation
prepare_application_engine(
    journaled_application journaled,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive);

/*! \brief Capture removal recovery inputs without target mutation. */
[[nodiscard]] application_engine_preparation
prepare_application_engine(
    journaled_application journaled,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief One attempted rejected-object publication and its exact command. */
class rejected_effect_application_result final {
public:
  rejected_effect_application_result(
      backend_rejected_effect_request request,
      rejected_object_publication_result result);

  [[nodiscard]] const backend_rejected_effect_request&
  request() const noexcept;
  [[nodiscard]] const rejected_object_publication_result&
  result() const noexcept;

private:
  backend_rejected_effect_request request_;
  rejected_object_publication_result result_;
};

/*! \brief Prepared transaction after all rejected-object effects complete. */
class rejected_published_application final {
public:
  rejected_published_application(
      prepared_application prepared,
      std::vector<rejected_effect_application_result> rejected_effects,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  rejected_published_application(
      const rejected_published_application&) = delete;
  rejected_published_application& operator=(
      const rejected_published_application&) = delete;
  rejected_published_application(
      rejected_published_application&&) noexcept = default;
  rejected_published_application& operator=(
      rejected_published_application&&) noexcept = default;

  [[nodiscard]] prepared_application& prepared() noexcept;
  [[nodiscard]] const prepared_application& prepared() const noexcept;
  [[nodiscard]] const std::vector<rejected_effect_application_result>&
  rejected_effects() const noexcept;
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  prepared_application prepared_;
  std::vector<rejected_effect_application_result> rejected_effects_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

/*! \brief Rejected-store failure or transaction ready for active mutation. */
class application_engine_rejected_publication final {
public:
  [[nodiscard]] static application_engine_rejected_publication
  failed(application_receipt receipt);

  [[nodiscard]] static application_engine_rejected_publication
  published(
      prepared_application prepared,
      std::vector<rejected_effect_application_result> rejected_effects,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  application_engine_rejected_publication(
      const application_engine_rejected_publication&) = delete;
  application_engine_rejected_publication& operator=(
      const application_engine_rejected_publication&) = delete;
  application_engine_rejected_publication(
      application_engine_rejected_publication&&) noexcept = default;
  application_engine_rejected_publication& operator=(
      application_engine_rejected_publication&&) noexcept = default;

  [[nodiscard]] bool is_published() const noexcept;
  [[nodiscard]] const application_receipt* failure() const noexcept;
  [[nodiscard]] rejected_published_application* published() noexcept;
  [[nodiscard]] const rejected_published_application*
  published() const noexcept;

private:
  using value_type =
      std::variant<application_receipt, rejected_published_application>;
  explicit application_engine_rejected_publication(value_type value);
  value_type value_;
};

/*! \brief Publish installation rejected objects before active mutation. */
[[nodiscard]] application_engine_rejected_publication
publish_rejected_application_engine(
    prepared_application prepared,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Publish upgrade rejected objects before active mutation. */
[[nodiscard]] application_engine_rejected_publication
publish_rejected_application_engine(
    prepared_application prepared,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Publish removal rejected objects before active mutation. */
[[nodiscard]] application_engine_rejected_publication
publish_rejected_application_engine(
    prepared_application prepared,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief One attempted managed active-object effect and its exact command. */
class active_effect_application_result final {
public:
  active_effect_application_result(
      backend_active_effect_request request,
      backend_operation_result result);

  [[nodiscard]] const backend_active_effect_request&
  request() const noexcept;
  [[nodiscard]] const backend_operation_result& result() const noexcept;

  /*! \brief Test whether this effect is known to have changed the target. */
  [[nodiscard]] bool changed_target() const noexcept;

private:
  backend_active_effect_request request_;
  backend_operation_result result_;
};

/*! \brief Reason active execution stopped before final observation. */
enum class active_execution_interruption : std::uint8_t {
  effect_failed = 1,
  effect_indeterminate = 2,
  durability_unconfirmed = 3,
  durability_indeterminate = 4,
  result_observation_mismatch = 5,
  result_observation_indeterminate = 6,
};

/*! \brief Transaction after every active effect reached a semantic terminal state. */
class active_mutated_application final {
public:
  active_mutated_application(
      rejected_published_application rejected,
      std::vector<active_effect_application_result> active_effects,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  active_mutated_application(const active_mutated_application&) = delete;
  active_mutated_application& operator=(
      const active_mutated_application&) = delete;
  active_mutated_application(active_mutated_application&&) noexcept = default;
  active_mutated_application& operator=(
      active_mutated_application&&) noexcept = default;

  [[nodiscard]] rejected_published_application& rejected() noexcept;
  [[nodiscard]] const rejected_published_application& rejected() const noexcept;
  [[nodiscard]] std::vector<active_effect_application_result>&
  active_effects() noexcept;
  [[nodiscard]] const std::vector<active_effect_application_result>&
  active_effects() const noexcept;
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  [[nodiscard]] std::vector<application_backend_evidence_identity>&
  backend_evidence() noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  rejected_published_application rejected_;
  std::vector<active_effect_application_result> active_effects_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

/*! \brief Interrupted active execution retaining transaction and recovery assets. */
class active_interrupted_application final {
public:
  active_interrupted_application(
      rejected_published_application rejected,
      std::vector<active_effect_application_result> active_effects,
      active_execution_interruption interruption,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  active_interrupted_application(
      const active_interrupted_application&) = delete;
  active_interrupted_application& operator=(
      const active_interrupted_application&) = delete;
  active_interrupted_application(
      active_interrupted_application&&) noexcept = default;
  active_interrupted_application& operator=(
      active_interrupted_application&&) noexcept = default;

  [[nodiscard]] rejected_published_application& rejected() noexcept;
  [[nodiscard]] const rejected_published_application& rejected() const noexcept;
  [[nodiscard]] const std::vector<active_effect_application_result>&
  active_effects() const noexcept;
  [[nodiscard]] active_execution_interruption interruption() const noexcept;
  [[nodiscard]] const application_durability_profile&
  durability() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;

private:
  rejected_published_application rejected_;
  std::vector<active_effect_application_result> active_effects_;
  active_execution_interruption interruption_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
};

/*! \brief Complete active phase or interruption awaiting recovery policy. */
class application_engine_active_execution final {
public:
  [[nodiscard]] static application_engine_active_execution complete(
      rejected_published_application rejected,
      std::vector<active_effect_application_result> active_effects,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  [[nodiscard]] static application_engine_active_execution interrupted(
      rejected_published_application rejected,
      std::vector<active_effect_application_result> active_effects,
      active_execution_interruption interruption,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence);

  application_engine_active_execution(
      const application_engine_active_execution&) = delete;
  application_engine_active_execution& operator=(
      const application_engine_active_execution&) = delete;
  application_engine_active_execution(
      application_engine_active_execution&&) noexcept = default;
  application_engine_active_execution& operator=(
      application_engine_active_execution&&) noexcept = default;

  [[nodiscard]] bool is_complete() const noexcept;
  [[nodiscard]] active_mutated_application* complete() noexcept;
  [[nodiscard]] const active_mutated_application* complete() const noexcept;
  [[nodiscard]] active_interrupted_application* interruption() noexcept;
  [[nodiscard]] const active_interrupted_application*
  interruption() const noexcept;

private:
  using value_type =
      std::variant<active_mutated_application, active_interrupted_application>;
  explicit application_engine_active_execution(value_type value);
  value_type value_;
};

/*! \brief Execute installation active effects without final observation. */
[[nodiscard]] application_engine_active_execution
execute_active_application_engine(
    rejected_published_application rejected,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Execute upgrade active effects without final observation. */
[[nodiscard]] application_engine_active_execution
execute_active_application_engine(
    rejected_published_application rejected,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Execute removal active effects without final observation. */
[[nodiscard]] application_engine_active_execution
execute_active_application_engine(
    rejected_published_application rejected,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Final receipt or an observation interruption awaiting recovery. */
class application_engine_completion final {
public:
  [[nodiscard]] static application_engine_completion
  sealed(application_receipt receipt);

  [[nodiscard]] static application_engine_completion
  interrupted(rejected_published_application rejected,
              std::vector<active_effect_application_result> active_effects,
              active_execution_interruption interruption,
              application_durability_profile durability,
              std::vector<application_backend_evidence_identity>
                  backend_evidence);

  application_engine_completion(const application_engine_completion&) = delete;
  application_engine_completion& operator=(
      const application_engine_completion&) = delete;
  application_engine_completion(application_engine_completion&&) noexcept =
      default;
  application_engine_completion& operator=(
      application_engine_completion&&) noexcept = default;

  [[nodiscard]] bool has_receipt() const noexcept;
  [[nodiscard]] application_receipt* receipt() noexcept;
  [[nodiscard]] const application_receipt* receipt() const noexcept;
  [[nodiscard]] active_interrupted_application* interruption() noexcept;
  [[nodiscard]] const active_interrupted_application*
  interruption() const noexcept;

private:
  using value_type =
      std::variant<application_receipt, active_interrupted_application>;
  explicit application_engine_completion(value_type value);
  value_type value_;
};

/*! \brief Observe and seal one successful installation attempt. */
[[nodiscard]] application_engine_completion
complete_application_engine(
    active_mutated_application active,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image);

/*! \brief Observe and seal one successful upgrade attempt. */
[[nodiscard]] application_engine_completion
complete_application_engine(
    active_mutated_application active,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image);

/*! \brief Observe and seal one successful removal attempt. */
[[nodiscard]] application_engine_completion
complete_application_engine(
    active_mutated_application active,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Recover an interrupted installation and seal its failure receipt. */
[[nodiscard]] application_receipt
recover_application_engine(
    active_interrupted_application interrupted,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Recover an interrupted upgrade and seal its failure receipt. */
[[nodiscard]] application_receipt
recover_application_engine(
    active_interrupted_application interrupted,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

/*! \brief Recover an interrupted removal and seal its failure receipt. */
[[nodiscard]] application_receipt
recover_application_engine(
    active_interrupted_application interrupted,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

[[nodiscard]] application_receipt replay_application_engine(
    reopened_application reopened,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive);

[[nodiscard]] application_receipt replay_application_engine(
    reopened_application reopened,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_archive& archive);

[[nodiscard]] application_receipt replay_application_engine(
    reopened_application reopened,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);

} // namespace pkgapply::detail
