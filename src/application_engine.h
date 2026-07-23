// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <libpkgapply/admission.h>
#include <libpkgapply/attempt.h>
#include <libpkgapply/precondition.h>
#include <libpkgapply/result.h>
#include <libpkgapply/schedule.h>
#include <libpkgapply/journal.h>
#include <libpkgimage/package_archive.h>

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

/*! \brief One admitted transaction with its complete durable effect graph. */
class journaled_application final {
public:
  journaled_application(
      admitted_application admitted,
      std::optional<incoming_payload_plan> payloads,
      old_object_capture_plan captures,
      application_effect_schedule schedule,
      application_journal_record journal);

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
  [[nodiscard]] const application_journal_record& journal() const noexcept;

  /*! \brief Replace the durable snapshot without changing its effect graph. */
  void advance_journal(application_journal_record journal);

private:
  admitted_application admitted_;
  std::optional<incoming_payload_plan> payloads_;
  old_object_capture_plan captures_;
  application_effect_schedule schedule_;
  application_journal_record journal_;
};

[[nodiscard]] journaled_application journal_application_engine(
    admitted_application admitted,
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image);

[[nodiscard]] journaled_application journal_application_engine(
    admitted_application admitted,
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const pkgimage::package_image& image);

[[nodiscard]] journaled_application journal_application_engine(
    admitted_application admitted,
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease);


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

} // namespace pkgapply::detail
