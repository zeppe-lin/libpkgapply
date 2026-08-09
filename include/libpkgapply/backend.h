// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file backend.h
 *  \brief Semantic backend commands and transaction interfaces.
 */
#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <libpkgapply/attempt.h>
#include <libpkgapply/journal.h>
#include <libpkgapply/mutation_lease.h>
#include <libpkgapply/path_consequence.h>
#include <libpkgapply/request.h>
#include <libpkgapply/result.h>
#include <libpkgimage/entry_selection.h>
#include <libpkgimage/package_image.h>
#include <libpkgimage/payload_sink.h>
#include <libpkgplan/package_path.h>
#include <libpkgplan/plan.h>

/*! \namespace pkgapply
 *  \brief Semantic package-application authority and backend protocol.
 */
namespace pkgapply {

class application_restart_checkpoint;

/*! \brief Mechanism-level completion reported by an application backend.
 *
 *  These values report physical mechanism truth, not application success.
 *  A backend must report indeterminate after any operation that may have
 *  changed visible target state but cannot prove the resulting state. The
 *  semantic engine owns recovery selection and terminal outcome classification.
 */
enum class backend_operation_outcome {
  completed, /*!< Requested physical effect was established. */
  conditional_retained, /*!< Conditional cleanup retained a non-empty object. */
  failed, /*!< Effect was not established and the target is proven unchanged. */
  indeterminate, /*!< Visible state may have changed or cannot be proven. */
};

/*! \brief Immutable result of one backend mechanism operation. */
class backend_operation_result final {
public:
  /*! \brief Validate and construct one mechanism result.
   *  \param outcome Exact physical outcome.
   *  \param evidence Supporting mechanism evidence identities.
   *  \throws std::invalid_argument If outcome is invalid or evidence repeats.
   */
  backend_operation_result(
      backend_operation_outcome outcome,
      std::vector<application_backend_evidence_identity> evidence = {});

  /*!
   * \brief Return the exact physical outcome.
  *  \return The exact physical outcome.
   */
  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  /*!
   * \brief Return supporting evidence in canonical order.
  *  \return Supporting evidence in canonical order.
   */
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

private:
  backend_operation_outcome outcome_;
  std::vector<application_backend_evidence_identity> evidence_;
};

/*! \brief Result of publishing one immutable rejected-object record. */
class rejected_object_publication_result final {
public:
  /*! \brief Validate and construct one rejected-record publication result.
   *  \param outcome Exact publication outcome.
   *  \param record Published record identity, present only when completed.
   *  \param evidence Supporting mechanism evidence identities.
   *  \throws std::invalid_argument If outcome, record applicability, or
   *          evidence uniqueness is invalid.
   */
  rejected_object_publication_result(
      backend_operation_outcome outcome,
      std::optional<rejected_object_record_identity> record,
      std::vector<application_backend_evidence_identity> evidence = {});

  /*!
   * \brief Return the exact publication outcome.
  *  \return The exact publication outcome.
   */
  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  /*!
   * \brief Return the published record identity when publication completed.
  *  \return The published record identity when publication completed.
   */
  [[nodiscard]] const std::optional<rejected_object_record_identity>&
  record() const noexcept;
  /*!
   * \brief Return supporting evidence in canonical order.
  *  \return Supporting evidence in canonical order.
   */
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
  /*! \brief Validate and construct one evidence-publication result.
   *  \param outcome Exact publication outcome.
   *  \param record Published evidence identity, present only when completed.
   *  \param evidence Supporting mechanism evidence identities.
   *  \throws std::invalid_argument If outcome, record applicability, or
   *          evidence uniqueness is invalid.
   */
  completed_evidence_publication_result(
      backend_operation_outcome outcome,
      std::optional<completed_application_evidence_identity> record,
      std::vector<application_backend_evidence_identity> evidence = {});

  /*!
   * \brief Return the exact publication outcome.
  *  \return The exact publication outcome.
   */
  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  /*!
   * \brief Return the published evidence identity when completed.
  *  \return The published evidence identity when completed.
   */
  [[nodiscard]] const std::optional<completed_application_evidence_identity>&
  record() const noexcept;
  /*!
   * \brief Return supporting evidence in canonical order.
  *  \return Supporting evidence in canonical order.
   */
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
  /*! \brief Normalize and construct a complete observation closure.
   *  \param requested Exact requested paths.
   *  \param observations One observation for every requested path.
   *  \param evidence Supporting observation evidence identities.
   *  \return Canonical complete observation batch.
   *  \throws std::invalid_argument If paths or evidence repeat, or if the
   *          observation set is incomplete or contains another path.
   */
  [[nodiscard]] static backend_observation_batch make(
      std::vector<pkgplan::package_path> requested,
      std::vector<application_path_observation> observations,
      std::vector<application_backend_evidence_identity> evidence = {});

  /*!
   * \brief Return requested paths in canonical order.
  *  \return Requested paths in canonical order.
   */
  [[nodiscard]] const std::vector<pkgplan::package_path>&
  requested() const noexcept;
  /*!
   * \brief Return matching observations in canonical path order.
  *  \return Matching observations in canonical path order.
   */
  [[nodiscard]] const std::vector<application_path_observation>&
  observations() const noexcept;
  /*!
   * \brief Return supporting evidence in canonical order.
  *  \return Supporting evidence in canonical order.
   */
  [[nodiscard]] const std::vector<application_backend_evidence_identity>&
  evidence() const noexcept;

  /*! \brief Find the observation for one exact requested path.
   *  \param path Logical path to query.
   *  \return Pointer valid for this batch's lifetime, or `nullptr`.
   */
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

/*! \brief Exact reason to capture one existing object before mutation. */
class old_object_capture_request final {
public:
  /*! \brief Construct one purposeful capture request.
   *  \param path Logical target path.
   *  \param for_rejected_object Capture supplies rejected-object publication.
   *  \param for_recovery Capture supplies exact active-state recovery.
   *  \throws std::invalid_argument If neither purpose is requested.
   */
  old_object_capture_request(pkgplan::package_path path,
                             bool for_rejected_object,
                             bool for_recovery);

  /*!
   * \brief Return the logical target path.
  *  \return The logical target path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return whether capture feeds rejected-object publication.
  *  \return Whether capture feeds rejected-object publication.
   */
  [[nodiscard]] bool for_rejected_object() const noexcept;
  /*!
   * \brief Return whether capture feeds exact recovery.
  *  \return Whether capture feeds exact recovery.
   */
  [[nodiscard]] bool for_recovery() const noexcept;

private:
  pkgplan::package_path path_;
  bool for_rejected_object_;
  bool for_recovery_;
};

/*! \brief Backend evidence established while capturing one old object. */
class old_object_capture_result final {
public:
  /*! \brief Validate and construct one capture result.
   *  \param outcome Exact physical capture outcome.
   *  \param captured Observation of the captured logical path.
   *  \param exact_recovery_possible Whether captured authority can restore the
   *          exact admitted prior state.
   *  \param evidence Supporting mechanism evidence identities.
   *  \throws std::invalid_argument If a completed capture is not known,
   *          failed capture claims exact recovery, or evidence repeats.
   */
  old_object_capture_result(
      backend_operation_outcome outcome,
      application_path_observation captured,
      bool exact_recovery_possible,
      std::vector<application_backend_evidence_identity> evidence = {});

  /*!
   * \brief Return the exact physical capture outcome.
  *  \return The exact physical capture outcome.
   */
  [[nodiscard]] backend_operation_outcome outcome() const noexcept;
  /*!
   * \brief Return the observation retained by the capture.
  *  \return The observation retained by the capture.
   */
  [[nodiscard]] const application_path_observation& captured() const noexcept;
  /*!
   * \brief Return whether exact admitted-state recovery is possible.
  *  \return Whether exact admitted-state recovery is possible.
   */
  [[nodiscard]] bool exact_recovery_possible() const noexcept;
  /*!
   * \brief Return supporting evidence in canonical order.
  *  \return Supporting evidence in canonical order.
   */
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
  /*! \brief Validate and construct one active-namespace command.
   *  \param path Logical target path.
   *  \param outcome Planner-owned desired active outcome.
   *  \param incoming_entry Incoming entry required only for activation.
   *  \return Exact backend command without policy reinterpretation.
   *  \throws std::invalid_argument If outcome is invalid or incoming-entry
   *          applicability contradicts it.
   */
  [[nodiscard]] static backend_active_effect_request make(
      pkgplan::package_path path,
      pkgplan::planned_active_outcome outcome,
      std::optional<pkgimage::entry_id> incoming_entry = std::nullopt);

  /*!
   * \brief Return the logical target path.
  *  \return The logical target path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the exact planner-owned active outcome.
  *  \return The exact planner-owned active outcome.
   */
  [[nodiscard]] pkgplan::planned_active_outcome outcome() const noexcept;
  /*!
   * \brief Return the incoming entry used for activation, when any.
  *  \return The incoming entry used for activation, when any.
   */
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

/*! \brief Exact rejected-object command copied from one accepted plan path. */
class backend_rejected_effect_request final {
public:
  /*! \brief Copy complete structured staging intent from planner authority.
   *  \param plan Exact planner-owned rejected-object plan.
   *  \return Backend command retaining all source provenance.
   *  \throws std::invalid_argument If the plan has no complete structured
   *          incoming or old-installed source provenance.
   */
  [[nodiscard]] static backend_rejected_effect_request
  from_plan(const pkgplan::rejected_object_plan& plan);

  /*!
   * \brief Return the logical target path.
  *  \return The logical target path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the staging outcome implied by source side.
  *  \return The staging outcome implied by source side.
   */
  [[nodiscard]] pkgplan::planned_rejected_outcome outcome() const noexcept;
  /*!
   * \brief Return whether rejected bytes are incoming or previously installed.
  *  \return Whether rejected bytes are incoming or previously installed.
   */
  [[nodiscard]] pkgplan::rejected_object_source_side source_side() const noexcept;
  /*!
   * \brief Return the planner-owned rejection reason.
  *  \return The planner-owned rejection reason.
   */
  [[nodiscard]] pkgplan::rejected_object_reason reason() const noexcept;
  /*!
   * \brief Return package-release authority for the rejected object.
  *  \return Package-release authority for the rejected object.
   */
  [[nodiscard]] const pkgplan::package_release_identity& release() const noexcept;
  /*!
   * \brief Return incoming artifact identity when source side is incoming.
  *  \return Incoming artifact identity when source side is incoming.
   */
  [[nodiscard]] const std::optional<pkgplan::artifact_identity>&
  artifact() const noexcept;
  /*!
   * \brief Return incoming manifest identity when source side is incoming.
  *  \return Incoming manifest identity when source side is incoming.
   */
  [[nodiscard]] const std::optional<pkgplan::artifact_manifest_identity>&
  artifact_manifest() const noexcept;
  /*!
   * \brief Return incoming image identity when source side is incoming.
  *  \return Incoming image identity when source side is incoming.
   */
  [[nodiscard]] const std::optional<pkgimage::package_image_identity>&
  image() const noexcept;
  /*!
   * \brief Return incoming image entry when source side is incoming.
  *  \return Incoming image entry when source side is incoming.
   */
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;
  /*!
   * \brief Return installed package identity when source side is old.
  *  \return Installed package identity when source side is old.
   */
  [[nodiscard]] const std::optional<pkgplan::installed_package_identity>&
  installed_package() const noexcept;
  /*!
   * \brief Return installed control identity when source side is old.
  *  \return Installed control identity when source side is old.
   */
  [[nodiscard]] const std::optional<pkgplan::installed_control_identity>&
  installed_control() const noexcept;
  /*!
   * \brief Return observations that justified rejected-object staging.
  *  \return Observations that justified rejected-object staging.
   */
  [[nodiscard]] const pkgplan::observation_set_identity&
  observations() const noexcept;

private:
  backend_rejected_effect_request(
      pkgplan::package_path path,
      pkgplan::rejected_object_source_side source_side,
      pkgplan::rejected_object_reason reason,
      pkgplan::package_release_identity release,
      std::optional<pkgplan::artifact_identity> artifact,
      std::optional<pkgplan::artifact_manifest_identity> artifact_manifest,
      std::optional<pkgimage::package_image_identity> image,
      std::optional<pkgimage::entry_id> incoming_entry,
      std::optional<pkgplan::installed_package_identity> installed_package,
      std::optional<pkgplan::installed_control_identity> installed_control,
      pkgplan::observation_set_identity observations);

  pkgplan::package_path path_;
  pkgplan::rejected_object_source_side source_side_;
  pkgplan::rejected_object_reason reason_;
  pkgplan::package_release_identity release_;
  std::optional<pkgplan::artifact_identity> artifact_;
  std::optional<pkgplan::artifact_manifest_identity> artifact_manifest_;
  std::optional<pkgimage::package_image_identity> image_;
  std::optional<pkgimage::entry_id> incoming_entry_;
  std::optional<pkgplan::installed_package_identity> installed_package_;
  std::optional<pkgplan::installed_control_identity> installed_control_;
  pkgplan::observation_set_identity observations_;
};

/*! \brief Backend-owned sink for one exact incoming regular-payload closure. */
class incoming_payload_stage : public pkgimage::payload_sink {
public:
  /*! \brief Destroy the polymorphic payload stage. */
  ~incoming_payload_stage() override;

  /*! \brief Seal all consumed payloads into private transaction staging.
   *  \return Exact physical sealing result.
   */
  [[nodiscard]] virtual backend_operation_result seal() = 0;

  /*! \brief Discard unsealed resources without claiming target rollback. */
  virtual void abandon() noexcept = 0;

  /*!
   * \brief Return whether seal() completed successfully.
  *  \return Whether seal() completed successfully.
   */
  [[nodiscard]] virtual bool sealed() const noexcept = 0;
};

/*! \brief One provider transaction bound to an already-held outer lease.
 *
 *  The transaction owns mechanism state for one attempt. It may observe and
 *  mutate only the target, lease, request, incoming image, and attempt nonce to
 *  which it reports itself bound. It cannot reinterpret planner policy.
 */
class application_backend_transaction {
public:
  /*! \brief Construct an unbound implementation base. */
  application_backend_transaction() = default;
  /*! \brief Interface objects are not copy-constructible. */
  application_backend_transaction(const application_backend_transaction&) = delete;
  /*! \brief Interface objects are not copy-assignable. */
  application_backend_transaction& operator=(const application_backend_transaction&) = delete;
  /*! \brief Interface objects are not move-constructible. */
  application_backend_transaction(application_backend_transaction&&) = delete;
  /*! \brief Interface objects are not move-assignable. */
  application_backend_transaction& operator=(application_backend_transaction&&) = delete;
  /*! \brief Destroy the polymorphic transaction. */
  virtual ~application_backend_transaction();

  /*!
   * \brief Return mutation-backend identity.
  *  \return Mutation-backend identity.
   */
  [[nodiscard]] virtual const mutation_backend_identity&
  backend() const noexcept = 0;
  /*!
   * \brief Return observation-backend identity.
  *  \return Observation-backend identity.
   */
  [[nodiscard]] virtual const observation_backend_identity&
  observation_backend() const noexcept = 0;
  /*!
   * \brief Return exact execution capability profile.
  *  \return Exact execution capability profile.
   */
  [[nodiscard]] virtual const execution_capability_profile_identity&
  capabilities() const noexcept = 0;
  /*!
   * \brief Return exact target-context identity.
  *  \return Exact target-context identity.
   */
  [[nodiscard]] virtual const application_target_context_identity&
  target() const noexcept = 0;
  /*!
   * \brief Return exact outer mutation-lease identity.
  *  \return Exact outer mutation-lease identity.
   */
  [[nodiscard]] virtual const mutation_lease_instance_identity&
  lease() const noexcept = 0;
  /*!
   * \brief Return provider-issued physical attempt nonce.
  *  \return Provider-issued physical attempt nonce.
   */
  [[nodiscard]] virtual const application_attempt_nonce&
  attempt_nonce() const noexcept = 0;

  /*! \brief Return the durable journal reopened by this transaction.
   *  \return Reopened journal identity, or empty for a fresh transaction.
   */
  [[nodiscard]] virtual std::optional<application_journal_record_identity>
  resumed_journal() const noexcept;

  /*! \brief Read exact durable replay material for a reopened journal.
   *  \param journal Exact durable journal snapshot.
   *  \return Closed semantic checkpoint for deterministic replay.
   *  \throws std::logic_error By default when restart is unsupported.
   */
  [[nodiscard]] virtual application_restart_checkpoint restart_checkpoint(
      const application_journal_record& journal);

  /*! \brief Observe one exact complete path set.
   *  \param paths Canonical logical paths requested by the semantic engine.
   *  \return One complete observation for every requested path.
   */
  [[nodiscard]] virtual backend_observation_batch observe(
      const std::vector<pkgplan::package_path>& paths) = 0;

  /*! \brief Begin private staging for selected incoming regular payloads.
   *  \param image Exact admitted normalized package image.
   *  \param selection Exact regular-entry closure to consume.
   *  \return Unique backend-owned payload sink.
   */
  [[nodiscard]] virtual std::unique_ptr<incoming_payload_stage>
  begin_payload_stage(const pkgimage::package_image& image,
                      const pkgimage::entry_selection& selection) = 0;

  /*! \brief Capture one exact old object before mutation.
   *  \param request Core-derived capture purpose and path.
   *  \return Exact physical capture result and evidence.
   */
  [[nodiscard]] virtual old_object_capture_result capture_old(
      const old_object_capture_request& request) = 0;

  /*! \brief Execute one exact core-derived active-namespace command.
   *
   *  Implementations must not request replacement authority or reinterpret the
   *  accepted plan. Failed proves the logical target unchanged; uncertainty
   *  after a potentially visible mutation is indeterminate.
   *
   *  \param request Exact planner-derived active command.
   *  \return Exact physical mechanism result.
   */
  [[nodiscard]] virtual backend_operation_result execute_active(
      const backend_active_effect_request& request) = 0;

  /*! \brief Publish one exact planner-derived rejected object.
   *  \param request Exact staging command and source provenance.
   *  \return Publication result and immutable record identity when completed.
   */
  [[nodiscard]] virtual rejected_object_publication_result execute_rejected(
      const backend_rejected_effect_request& request) = 0;

  /*! \brief Publish completed evidence before terminal receipt sealing.
   *
   *  Publication is immutable and idempotent by evidence identity. Restart may
   *  invoke this operation again after an older completed-evidence record is
   *  already durable when the current outer lease requires the same completed
   *  application truth to be rebound to a new lease-bound state projection.
   *  Providers validate request, attempt, target, and journal authority, but
   *  must not force the evidence projection to equal the historical journal
   *  header projection.
   *
   *  \param evidence Exact semantic evidence to publish unchanged.
   *  \return Publication result and record identity when completed.
   */
  [[nodiscard]] virtual completed_evidence_publication_result
  publish_completed_evidence(
      const completed_application_evidence& evidence) = 0;

  /*! \brief Restore one core-selected active path from transaction authority.
   *
   *  Completed proves the admitted prior state was restored. Failed proves the
   *  recovery attempt left the current target unchanged. Ambiguous or partial
   *  restoration is indeterminate. The core selects paths and reverse order.
   *
   *  \param path Exact logical path selected for recovery.
   *  \return Exact physical recovery result.
   */
  [[nodiscard]] virtual backend_operation_result recover(
      const pkgplan::package_path& path) = 0;

  /*! \brief Synchronize one exact durability domain.
   *  \param domain Domain selected by semantic execution control.
   *  \return Backend-issued durability fact.
   */
  [[nodiscard]] virtual application_durability_fact synchronize(
      application_durability_domain domain) = 0;

  /*! \brief Durably replace the journal with one validated snapshot.
   *  \param record Candidate monotonic journal successor.
   *  \return Exact snapshot durably retained by the provider.
   */
  [[nodiscard]] virtual application_journal_record publish_journal(
      const application_journal_record& record) = 0;
};

/*! \brief Provider factory selected by the transaction controller.
 *
 *  The backend supplies physical mechanisms. It does not acquire the outer
 *  target lease, choose a plan, construct installed state, publish package
 *  state, or classify terminal semantic success.
 */
class application_backend {
public:
  /*! \brief Construct an unbound implementation base. */
  application_backend() = default;
  /*! \brief Interface objects are not copy-constructible. */
  application_backend(const application_backend&) = delete;
  /*! \brief Interface objects are not copy-assignable. */
  application_backend& operator=(const application_backend&) = delete;
  /*! \brief Interface objects are not move-constructible. */
  application_backend(application_backend&&) = delete;
  /*! \brief Interface objects are not move-assignable. */
  application_backend& operator=(application_backend&&) = delete;
  /*! \brief Destroy the polymorphic backend. */
  virtual ~application_backend();

  /*!
   * \brief Return stable mutation-backend identity.
  *  \return Stable mutation-backend identity.
   */
  [[nodiscard]] virtual const mutation_backend_identity&
  identity() const noexcept = 0;
  /*!
   * \brief Return stable observation-backend identity.
  *  \return Stable observation-backend identity.
   */
  [[nodiscard]] virtual const observation_backend_identity&
  observation_identity() const noexcept = 0;
  /*!
   * \brief Return exact execution capability profile.
  *  \return Exact execution capability profile.
   */
  [[nodiscard]] virtual const execution_capability_profile_identity&
  capabilities() const noexcept = 0;

  /*! \brief Begin an install or upgrade transaction with incoming image truth.
   *  \param request Immutable application authority.
   *  \param lease Mutable borrowed caller-held outer lease.
   *  \param incoming_image Exact admitted normalized package image.
   *  \return Unique transaction bound to request, target, lease, and image.
   */
  [[nodiscard]] virtual std::unique_ptr<application_backend_transaction>
  begin_with_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease,
      const pkgimage::package_image& incoming_image) = 0;

  /*! \brief Begin a removal transaction without incoming image authority.
   *  \param request Immutable removal application authority.
   *  \param lease Mutable borrowed caller-held outer lease.
   *  \return Unique transaction bound to request, target, and lease.
   */
  [[nodiscard]] virtual std::unique_ptr<application_backend_transaction>
  begin_without_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease) = 0;

  /*! \brief Reopen one durable install or upgrade attempt under a new lease.
   *  \param request Immutable application authority.
   *  \param lease Mutable borrowed caller-held outer lease.
   *  \param journal Durable journal snapshot to reopen.
   *  \param incoming_image Exact admitted normalized package image.
   *  \return Unique reopened transaction.
   *  \throws std::logic_error By default when restart is unsupported.
   */
  [[nodiscard]] virtual std::unique_ptr<application_backend_transaction>
  resume_with_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease,
      const application_journal_record& journal,
      const pkgimage::package_image& incoming_image);

  /*! \brief Reopen one durable removal attempt under a new lease.
   *  \param request Immutable removal application authority.
   *  \param lease Mutable borrowed caller-held outer lease.
   *  \param journal Durable journal snapshot to reopen.
   *  \return Unique reopened transaction.
   *  \throws std::logic_error By default when restart is unsupported.
   */
  [[nodiscard]] virtual std::unique_ptr<application_backend_transaction>
  resume_without_incoming_image(
      const package_application_request& request,
      target_mutation_lease& lease,
      const application_journal_record& journal);
};

} // namespace pkgapply
