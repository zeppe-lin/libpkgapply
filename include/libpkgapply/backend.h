// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <libpkgapply/attempt.h>
#include <libpkgapply/journal.h>
#include <libpkgapply/mutation_lease.h>
#include <libpkgapply/path_consequence.h>
#include <libpkgapply/result.h>
#include <libpkgimage/entry_selection.h>
#include <libpkgimage/package_image.h>
#include <libpkgimage/payload_sink.h>
#include <libpkgplan/package_path.h>
#include <libpkgplan/plan.h>

namespace pkgapply {

/*! \brief Mechanism-level completion reported by an application backend. */
enum class backend_operation_outcome {
  completed,
  conditional_retained,
  failed,
  indeterminate,
};

/*! \brief Immutable result of one backend mechanism operation. */
class backend_operation_result final {
public:
  backend_operation_result(
      backend_operation_outcome outcome,
      std::vector<application_backend_evidence_identity> evidence = {});

  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

private:
  backend_operation_outcome outcome_;
  std::vector<application_backend_evidence_identity> evidence_;
};

/*! \brief Result of publishing one immutable rejected-object record. */
class rejected_object_publication_result final {
public:
  /*!
   * \brief Retain publication outcome, completed record, and mechanism evidence.
   *
   * A completed outcome requires exactly one immutable record identity. Failed
   * and indeterminate outcomes cannot claim a completed rejected record.
   */
  rejected_object_publication_result(
      backend_operation_outcome outcome,
      std::optional<rejected_object_record_identity> record,
      std::vector<application_backend_evidence_identity> evidence = {});

  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  [[nodiscard]] const std::optional<rejected_object_record_identity>&
  record() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

private:
  backend_operation_outcome outcome_;
  std::optional<rejected_object_record_identity> record_;
  std::vector<application_backend_evidence_identity> evidence_;
};

/*! \brief Result of publishing one immutable completed-evidence record. */
class completed_evidence_publication_result final {
public:
  /*!
   * \brief Retain publication outcome, completed record, and mechanism evidence.
   *
   * A completed outcome requires the exact completed-evidence identity. Failed
   * and indeterminate outcomes cannot claim a published evidence record.
   */
  completed_evidence_publication_result(
      backend_operation_outcome outcome,
      std::optional<completed_application_evidence_identity> record,
      std::vector<application_backend_evidence_identity> evidence = {});

  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  record() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

private:
  backend_operation_outcome outcome_;
  std::optional<completed_application_evidence_identity> record_;
  std::vector<application_backend_evidence_identity> evidence_;
};

/*! \brief Exact observation closure returned for one requested path set. */
class backend_observation_batch final {
public:
  [[nodiscard]] static backend_observation_batch make(
      std::vector<pkgplan::package_path> requested,
      std::vector<application_path_observation> observations,
      std::vector<application_backend_evidence_identity> evidence = {});

  [[nodiscard]] const std::vector<pkgplan::package_path>&
  requested() const noexcept;
  [[nodiscard]] const std::vector<application_path_observation>&
  observations() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

  [[nodiscard]] const application_path_observation*
  find(const pkgplan::package_path& path) const noexcept;

private:
  backend_observation_batch(
      std::vector<pkgplan::package_path> requested,
      std::vector<application_path_observation> observations,
      std::vector<application_backend_evidence_identity> evidence);

  std::vector<pkgplan::package_path> requested_;
  std::vector<application_path_observation> observations_;
  std::vector<application_backend_evidence_identity> evidence_;
};

/*! \brief Why an existing target object must be captured before mutation. */
class old_object_capture_request final {
public:
  old_object_capture_request(pkgplan::package_path path,
                             bool for_rejected_object,
                             bool for_recovery);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] bool for_rejected_object() const noexcept;
  [[nodiscard]] bool for_recovery() const noexcept;

private:
  pkgplan::package_path path_;
  bool for_rejected_object_;
  bool for_recovery_;
};

/*! \brief Backend evidence established while capturing one old object. */
class old_object_capture_result final {
public:
  old_object_capture_result(
      backend_operation_outcome outcome,
      application_path_observation captured,
      bool exact_recovery_possible,
      std::vector<application_backend_evidence_identity> evidence = {});

  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  [[nodiscard]] const application_path_observation& captured() const noexcept;
  [[nodiscard]] bool exact_recovery_possible() const noexcept;
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

private:
  backend_operation_outcome outcome_;
  application_path_observation captured_;
  bool exact_recovery_possible_;
  std::vector<application_backend_evidence_identity> evidence_;
};

/*! \brief Exact active-object command derived from one accepted plan path. */
class backend_active_effect_request final {
public:
  [[nodiscard]] static backend_active_effect_request make(
      pkgplan::package_path path,
      pkgplan::planned_active_outcome outcome,
      std::optional<pkgimage::entry_id> incoming_entry = std::nullopt);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] pkgplan::planned_active_outcome outcome() const noexcept;
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;

private:
  backend_active_effect_request(
      pkgplan::package_path path,
      pkgplan::planned_active_outcome outcome,
      std::optional<pkgimage::entry_id> incoming_entry);

  pkgplan::package_path path_;
  pkgplan::planned_active_outcome outcome_;
  std::optional<pkgimage::entry_id> incoming_entry_;
};

/*! \brief Exact rejected-object command derived from one accepted plan path. */
class backend_rejected_effect_request final {
public:
  [[nodiscard]] static backend_rejected_effect_request stage_incoming(
      pkgplan::package_path path,
      pkgimage::entry_id incoming_entry);

  [[nodiscard]] static backend_rejected_effect_request stage_old(
      pkgplan::package_path path);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] pkgplan::planned_rejected_outcome outcome() const noexcept;
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;

private:
  backend_rejected_effect_request(
      pkgplan::package_path path,
      pkgplan::planned_rejected_outcome outcome,
      std::optional<pkgimage::entry_id> incoming_entry);

  pkgplan::package_path path_;
  pkgplan::planned_rejected_outcome outcome_;
  std::optional<pkgimage::entry_id> incoming_entry_;
};

/*! \brief Backend-owned sink for one exact incoming regular payload closure. */
class incoming_payload_stage : public pkgimage::payload_sink {
public:
  ~incoming_payload_stage() override = default;

  /*! \brief Seal all consumed payloads into the transaction's private stage. */
  [[nodiscard]] virtual backend_operation_result seal() = 0;

  /*! \brief Discard unsealed resources without claiming rollback. */
  virtual void abandon() noexcept = 0;

  /*! \brief Return whether seal() completed successfully. */
  [[nodiscard]] virtual bool sealed() const noexcept = 0;
};

/*! \brief One backend transaction bound to an already-held outer lease. */
class application_backend_transaction {
public:
  application_backend_transaction() = default;
  application_backend_transaction(const application_backend_transaction&) = delete;
  application_backend_transaction& operator=(
      const application_backend_transaction&) = delete;
  application_backend_transaction(application_backend_transaction&&) = delete;
  application_backend_transaction& operator=(
      application_backend_transaction&&) = delete;
  virtual ~application_backend_transaction() = default;

  [[nodiscard]] virtual const mutation_backend_identity&
  backend() const noexcept = 0;
  [[nodiscard]] virtual const observation_backend_identity&
  observation_backend() const noexcept = 0;
  [[nodiscard]] virtual const execution_capability_profile_identity&
  capabilities() const noexcept = 0;
  [[nodiscard]] virtual const application_target_context_identity&
  target() const noexcept = 0;
  [[nodiscard]] virtual const mutation_lease_instance_identity&
  lease() const noexcept = 0;
  [[nodiscard]] virtual const application_attempt_nonce&
  attempt_nonce() const noexcept = 0;

  [[nodiscard]] virtual backend_observation_batch observe(
      const std::vector<pkgplan::package_path>& paths) = 0;

  [[nodiscard]] virtual std::unique_ptr<incoming_payload_stage>
  begin_payload_stage(const pkgimage::package_image& image,
                      const pkgimage::entry_selection& selection) = 0;

  [[nodiscard]] virtual old_object_capture_result capture_old(
      const old_object_capture_request& request) = 0;

  [[nodiscard]] virtual backend_operation_result execute_active(
      const backend_active_effect_request& request) = 0;

  [[nodiscard]] virtual rejected_object_publication_result execute_rejected(
      const backend_rejected_effect_request& request) = 0;

  /*! \brief Publish one exact completed-evidence record before final receipt sealing. */
  [[nodiscard]] virtual completed_evidence_publication_result
  publish_completed_evidence(
      const completed_application_evidence& evidence) = 0;

  [[nodiscard]] virtual backend_operation_result recover(
      const pkgplan::package_path& path) = 0;

  [[nodiscard]] virtual application_durability_fact synchronize(
      application_durability_domain domain) = 0;

  /*! \brief Durably replace the backend journal with one validated snapshot. */
  [[nodiscard]] virtual application_journal_record publish_journal(
      const application_journal_record& record) = 0;
};

/*! \brief Backend factory selected by the transaction controller. */
class application_backend {
public:
  application_backend() = default;
  application_backend(const application_backend&) = delete;
  application_backend& operator=(const application_backend&) = delete;
  application_backend(application_backend&&) = delete;
  application_backend& operator=(application_backend&&) = delete;
  virtual ~application_backend() = default;

  [[nodiscard]] virtual const mutation_backend_identity&
  identity() const noexcept = 0;
  [[nodiscard]] virtual const observation_backend_identity&
  observation_identity() const noexcept = 0;
  [[nodiscard]] virtual const execution_capability_profile_identity&
  capabilities() const noexcept = 0;

  /*! \brief Begin an install or upgrade transaction with exact archive truth. */
  [[nodiscard]] virtual std::unique_ptr<application_backend_transaction>
  begin_with_incoming_image(
      const application_target_context& target,
      target_mutation_lease& lease,
      const pkgimage::package_image& incoming_image) = 0;

  /*! \brief Begin a removal transaction without incoming archive authority. */
  [[nodiscard]] virtual std::unique_ptr<application_backend_transaction>
  begin_without_incoming_image(
      const application_target_context& target,
      target_mutation_lease& lease) = 0;
};

} // namespace pkgapply
