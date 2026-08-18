// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file restart.h
 *  \brief Validation, owner-derived replay views, and replay of durable attempts.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <libpkgapply/backend.h>
#include <libpkgapply/journal.h>
#include <libpkgapply/journal_transport.h>
#include <libpkgapply/request.h>
#include <libpkgapply/state_projection.h>
#include <libpkgimage/package_archive.h>

namespace pkgapply {

namespace detail {
class application_restart_view_builder;
}

/*! \brief Required controller action for one validated durable journal. */
enum class application_restart_disposition : std::uint8_t {
  resume_forward = 1, /*!< Continue the admitted forward effect graph. */
  resume_recovery = 2, /*!< Resume recovery toward the admitted prior state. */
  terminal = 3, /*!< Journal already contains a terminal receipt. */
  external_resolution_required = 4, /*!< Caller must observe or resolve
                                         outside replay. */
};

/*! \brief Stable reason that a durable attempt cannot be reopened. */
enum class application_restart_error_code : std::uint8_t {
  journal_not_resumable = 1, /*!< Journal requires terminal or external
                                  handling. */
  journal_operation_kind_mismatch = 2, /*!< Journal records another operation kind. */
  journal_request_mismatch = 3, /*!< Journal belongs to another request. */
  journal_plan_mismatch = 4, /*!< Journal belongs to another accepted plan. */
  journal_target_mismatch = 5, /*!< Journal belongs to another target context. */
  journal_control_mismatch = 6, /*!< Journal carries another execution control. */
  journal_backend_mismatch = 7, /*!< Journal belongs to another mutation backend. */
  transaction_attempt_nonce_mismatch = 8, /*!< Reopened transaction names
                                               another attempt. */
};

/*! \brief Invalid restart authority or backend reopen binding. */
class PKGAPPLY_API application_restart_error final : public std::invalid_argument {
public:
  /*! \brief Construct a restart refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  application_restart_error(application_restart_error_code code,
                            std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~application_restart_error() override;

  /*!
   * \brief Return the stable refusal category.
  *  \return The stable refusal category.
   */
  [[nodiscard]] application_restart_error_code code() const noexcept;

private:
  application_restart_error_code code_;
};

/*! \brief Durable old-object capture retained by a reopened attempt. */
class PKGAPPLY_API application_restart_capture final {
public:
  /*! \brief Construct a path-keyed restart capture.
   *  \param result Complete backend capture result.
   */
  explicit application_restart_capture(old_object_capture_result result);
  /*!
   * \brief Return the captured logical path.
  *  \return The captured logical path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the complete capture result.
  *  \return The complete capture result.
   */
  [[nodiscard]] const old_object_capture_result& result() const noexcept;
  /*!
   * \brief Order restart captures by path.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const application_restart_capture& lhs,
                        const application_restart_capture& rhs) noexcept;

private:
  old_object_capture_result result_;
};

/*! \brief Durable rejected-object result retained across process restart. */
class PKGAPPLY_API application_restart_rejected_effect final {
public:
  /*! \brief Construct one path-keyed rejected result.
   *  \param path Logical path governed by the effect.
   *  \param result Durable rejected-object publication result.
   */
  application_restart_rejected_effect(
      pkgplan::package_path path,
      rejected_object_publication_result result);
  /*!
   * \brief Return the governed logical path.
  *  \return The governed logical path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the durable rejected-object result.
  *  \return The durable rejected-object result.
   */
  [[nodiscard]] const rejected_object_publication_result&
  result() const noexcept;
  /*!
   * \brief Order rejected results by path.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const application_restart_rejected_effect& lhs,
                        const application_restart_rejected_effect& rhs) noexcept;

private:
  pkgplan::package_path path_;
  rejected_object_publication_result result_;
};

/*! \brief Durable active-effect result retained across process restart. */
class PKGAPPLY_API application_restart_active_effect final {
public:
  /*! \brief Construct one path-keyed active result.
   *  \param path Logical path governed by the effect.
   *  \param result Durable backend operation result.
   */
  application_restart_active_effect(
      pkgplan::package_path path,
      backend_operation_result result);
  /*!
   * \brief Return the governed logical path.
  *  \return The governed logical path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the durable backend result.
  *  \return The durable backend result.
   */
  [[nodiscard]] const backend_operation_result& result() const noexcept;
  /*!
   * \brief Order active results by path.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const application_restart_active_effect& lhs,
                        const application_restart_active_effect& rhs) noexcept;

private:
  pkgplan::package_path path_;
  backend_operation_result result_;
};

/*! \brief Durable recovery-effect result retained across process restart. */
class PKGAPPLY_API application_restart_recovery_effect final {
public:
  /*! \brief Construct one path-keyed recovery result.
   *  \param path Logical path governed by the recovery effect.
   *  \param result Durable backend operation result.
   */
  application_restart_recovery_effect(
      pkgplan::package_path path,
      backend_operation_result result);
  /*!
   * \brief Return the governed logical path.
  *  \return The governed logical path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the durable backend result.
  *  \return The durable backend result.
   */
  [[nodiscard]] const backend_operation_result& result() const noexcept;
  /*!
   * \brief Order recovery results by path.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const application_restart_recovery_effect& lhs,
                        const application_restart_recovery_effect& rhs) noexcept;

private:
  pkgplan::package_path path_;
  backend_operation_result result_;
};

/*! \brief Exact synchronization result retained across process restart. */
class PKGAPPLY_API application_restart_synchronization final {
public:
  /*! \brief Construct one domain-keyed synchronization result.
   *  \param result Complete durability fact issued by the backend.
   */
  explicit application_restart_synchronization(
      application_durability_fact result);
  /*!
   * \brief Return the synchronized durability domain.
  *  \return The synchronized durability domain.
   */
  [[nodiscard]] application_durability_domain domain() const noexcept;
  /*!
   * \brief Return the complete durability fact.
  *  \return The complete durability fact.
   */
  [[nodiscard]] const application_durability_fact& result() const noexcept;
  /*!
   * \brief Order synchronization results by durability domain.
  *  \param lhs Left operand.
  *  \param rhs Right operand.
  *  \return Whether @p lhs precedes @p rhs in canonical order.
   */
  friend PKGAPPLY_API bool operator<(const application_restart_synchronization& lhs,
                        const application_restart_synchronization& rhs) noexcept;

private:
  application_durability_fact result_;
};

/*! \brief Owner-derived in-memory replay view for one durable attempt.
 *
 *  The view is reconstructed by libpkgapply from one validated journal
 *  declaration and its exact committed step chain. It is never persisted and
 *  is not provider-authored authority. Mutation backends receive this view
 *  only to revalidate subordinate physical evidence before replay continues.
 */
class application_restart_view final {
public:
  /*! \brief Return the exact journal declaration that produced this view.
   *  \return Exact owner-authored declaration identity.
   */
  [[nodiscard]] PKGAPPLY_API const application_journal_declaration_identity&
  declaration() const noexcept;
  /*! \brief Return the exact physical attempt retained by the declaration.
   *  \return Immutable physical application attempt.
   */
  [[nodiscard]] PKGAPPLY_API const application_attempt& attempt() const noexcept;
  /*! \brief Return original admitted observations.
   *  \return Original admitted observations decoded from the declaration seed.
   */
  [[nodiscard]] PKGAPPLY_API const backend_observation_batch&
  admitted_observations() const noexcept;
  /*! \brief Return incoming-payload staging result, when any.
   *  \return Incoming-payload staging result, when any.
   */
  [[nodiscard]] PKGAPPLY_API const std::optional<backend_operation_result>&
  incoming_payload() const noexcept;
  /*! \brief Return old-object captures in canonical path order.
   *  \return Old-object captures in canonical path order.
   */
  [[nodiscard]] PKGAPPLY_API const std::vector<application_restart_capture>&
  captures() const noexcept;
  /*! \brief Return rejected results in canonical path order.
   *  \return Rejected results in canonical path order.
   */
  [[nodiscard]] PKGAPPLY_API const std::vector<application_restart_rejected_effect>&
  rejected_effects() const noexcept;
  /*! \brief Return active results in canonical path order.
   *  \return Active results in canonical path order.
   */
  [[nodiscard]] PKGAPPLY_API const std::vector<application_restart_active_effect>&
  active_effects() const noexcept;
  /*! \brief Return recovery results in canonical path order.
   *  \return Recovery results in canonical path order.
   */
  [[nodiscard]] PKGAPPLY_API const std::vector<application_restart_recovery_effect>&
  recovery_effects() const noexcept;
  /*! \brief Return synchronization results in canonical domain order.
   *  \return Synchronization results in canonical domain order.
   */
  [[nodiscard]] PKGAPPLY_API const std::vector<application_restart_synchronization>&
  synchronizations() const noexcept;
  /*! \brief Return durability derived from retained terminal facts.
   *  \return Complete current durability profile.
   */
  [[nodiscard]] PKGAPPLY_API const application_durability_profile&
  durability() const noexcept;
  /*! \brief Return owner-derived supporting backend evidence.
   *  \return Deduplicated evidence in first-retained owner order.
   */
  [[nodiscard]] PKGAPPLY_API const std::vector<application_backend_evidence_identity>&
  backend_evidence() const noexcept;
  /*! \brief Return completed application evidence, when already produced.
   *  \return Completed application evidence, when already produced.
   */
  [[nodiscard]] PKGAPPLY_API const std::optional<completed_application_evidence>&
  completed_evidence() const noexcept;

  /*! \brief Find a capture result by exact path.
   *  \param path Logical path to query.
   *  \return Pointer valid for this view's lifetime, or `nullptr`.
   */
  [[nodiscard]] PKGAPPLY_API const application_restart_capture*
  find_capture(const pkgplan::package_path& path) const noexcept;
  /*! \brief Find a rejected publication result by exact path.
   *  \param path Logical path to query.
   *  \return Pointer valid for this view's lifetime, or `nullptr`.
   */
  [[nodiscard]] PKGAPPLY_API const application_restart_rejected_effect*
  find_rejected_effect(const pkgplan::package_path& path) const noexcept;
  /*! \brief Find an active effect result by exact path.
   *  \param path Logical path to query.
   *  \return Pointer valid for this view's lifetime, or `nullptr`.
   */
  [[nodiscard]] PKGAPPLY_API const application_restart_active_effect*
  find_active_effect(const pkgplan::package_path& path) const noexcept;
  /*! \brief Find a recovery effect result by exact path.
   *  \param path Logical path to query.
   *  \return Pointer valid for this view's lifetime, or `nullptr`.
   */
  [[nodiscard]] PKGAPPLY_API const application_restart_recovery_effect*
  find_recovery_effect(const pkgplan::package_path& path) const noexcept;
  /*! \brief Find a synchronization result by durability domain.
   *  \param domain Durability domain to query.
   *  \return Pointer valid for this view's lifetime, or `nullptr`.
   */
  [[nodiscard]] PKGAPPLY_API const application_restart_synchronization*
  find_synchronization(application_durability_domain domain) const noexcept;

private:
  friend class detail::application_restart_view_builder;

  application_restart_view(
      application_journal_declaration_identity declaration,
      application_attempt attempt,
      backend_observation_batch admitted_observations,
      std::optional<backend_operation_result> incoming_payload,
      std::vector<application_restart_capture> captures,
      std::vector<application_restart_rejected_effect> rejected_effects,
      std::vector<application_restart_active_effect> active_effects,
      std::vector<application_restart_recovery_effect> recovery_effects,
      std::vector<application_restart_synchronization> synchronizations,
      application_durability_profile durability,
      std::vector<application_backend_evidence_identity> backend_evidence,
      std::optional<completed_application_evidence> completed_evidence);

  application_journal_declaration_identity declaration_;
  application_attempt attempt_;
  backend_observation_batch admitted_observations_;
  std::optional<backend_operation_result> incoming_payload_;
  std::vector<application_restart_capture> captures_;
  std::vector<application_restart_rejected_effect> rejected_effects_;
  std::vector<application_restart_active_effect> active_effects_;
  std::vector<application_restart_recovery_effect> recovery_effects_;
  std::vector<application_restart_synchronization> synchronizations_;
  application_durability_profile durability_;
  std::vector<application_backend_evidence_identity> backend_evidence_;
  std::optional<completed_application_evidence> completed_evidence_;
};

/*! \brief Pure classification of one durable application journal. */
class PKGAPPLY_API application_restart_assessment final {
public:
  /*! \brief Construct one journal restart assessment.
   *  \param journal Assessed journal-record identity.
   *  \param state Durable journal lifecycle state.
   *  \param disposition Required controller action.
   */
  application_restart_assessment(
      application_journal_record_identity journal,
      application_journal_state state,
      application_restart_disposition disposition);

  /*!
   * \brief Return the assessed journal-record identity.
  *  \return The assessed journal-record identity.
   */
  [[nodiscard]] const application_journal_record_identity&
  journal() const noexcept;
  /*!
   * \brief Return the durable journal lifecycle state.
  *  \return The durable journal lifecycle state.
   */
  [[nodiscard]] application_journal_state state() const noexcept;
  /*!
   * \brief Return the required controller action.
  *  \return The required controller action.
   */
  [[nodiscard]] application_restart_disposition disposition() const noexcept;
  /*!
   * \brief Return whether automatic forward or recovery replay is allowed.
  *  \return Whether automatic forward or recovery replay is allowed.
   */
  [[nodiscard]] bool resumable() const noexcept;

private:
  application_journal_record_identity journal_;
  application_journal_state state_;
  application_restart_disposition disposition_;
};

/*! \brief Classify restart handling without touching backend or target.
 *  \param journal Validated immutable journal snapshot.
 *  \return Pure disposition derived from state, terminal evidence, and effect
 *          progress.
 */
[[nodiscard]] PKGAPPLY_API application_restart_assessment
assess_application_restart(const application_journal_record& journal);

/*! \brief Validate installation restart authority before reopening a backend.
 *  \param request Exact immutable installation request.
 *  \param state Current lease-bound state projection.
 *  \param lease Borrowed caller-held mutation lease.
 *  \param backend Backend expected by the durable journal.
 *  \param journal Validated immutable journal snapshot.
 *  \param archive Exact incoming archive retained by the caller.
 *  \throws application_restart_error For nonresumable or cross-bound journal.
 *  \throws application_admission_error For stale authority or archive facts.
 *  \throws mutation_lease_error If the lease is stale or cross-bound.
 */
PKGAPPLY_API void validate_application_restart(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive);

/*! \brief Validate upgrade restart authority before reopening a backend.
 *  \param request Exact immutable upgrade request.
 *  \param state Current lease-bound state projection.
 *  \param lease Borrowed caller-held mutation lease.
 *  \param backend Backend expected by the durable journal.
 *  \param journal Validated immutable journal snapshot.
 *  \param archive Exact incoming archive retained by the caller.
 *  \throws application_restart_error For nonresumable or cross-bound journal.
 *  \throws application_admission_error For stale authority or archive facts.
 *  \throws mutation_lease_error If the lease is stale or cross-bound.
 */
PKGAPPLY_API void validate_application_restart(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal,
    const pkgimage::package_archive& archive);

/*! \brief Validate removal restart authority before reopening a backend.
 *  \param request Exact immutable removal request.
 *  \param state Current lease-bound state projection.
 *  \param lease Borrowed caller-held mutation lease.
 *  \param backend Backend expected by the durable journal.
 *  \param journal Validated immutable journal snapshot.
 *  \throws application_restart_error For nonresumable or cross-bound journal.
 *  \throws application_admission_error For stale authority facts.
 *  \throws mutation_lease_error If the lease is stale or cross-bound.
 */
PKGAPPLY_API void validate_application_restart(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_journal_record& journal);

/*! \brief Resume one durable installation attempt to a terminal receipt.
 *  \param request Exact immutable installation request.
 *  \param state Current lease-bound state projection.
 *  \param lease Mutable borrowed caller-held mutation lease.
 *  \param backend Backend owning the durable attempt.
 *  \param journal_store Store owning the append-only semantic history.
 *  \param declaration Exact immutable declaration identity to resume.
 *  \param archive Exact incoming archive retained by the caller.
 *  \return Truthful terminal application receipt bound to @p state.
 *
 *  The journal declaration retains the exact immutable projection body and
 *  owner-authored replay seed admitted by the original process. A restart
 *  occurs under a newly acquired lease and therefore a new current projection;
 *  terminal receipt and completed evidence bind to that current projection
 *  after restart validation succeeds.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
resume_application(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    application_journal_store& journal_store,
    const application_journal_declaration_identity& declaration,
    const pkgimage::package_archive& archive);

/*! \brief Resume one durable upgrade attempt to a terminal receipt.
 *  \param request Exact immutable upgrade request.
 *  \param state Current lease-bound state projection.
 *  \param lease Mutable borrowed caller-held mutation lease.
 *  \param backend Backend owning the durable attempt.
 *  \param journal_store Store owning the append-only semantic history.
 *  \param declaration Exact immutable declaration identity to resume.
 *  \param archive Exact incoming archive retained by the caller.
 *  \return Truthful terminal application receipt bound to @p state.
 *
 *  The exact declaration projection body remains historical admission
 *  evidence; successful continuation binds terminal evidence to the current
 *  lease-bound projection supplied for this restart.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
resume_application(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    application_journal_store& journal_store,
    const application_journal_declaration_identity& declaration,
    const pkgimage::package_archive& archive);

/*! \brief Resume one durable removal attempt to a terminal receipt.
 *  \param request Exact immutable removal request.
 *  \param state Current lease-bound state projection.
 *  \param lease Mutable borrowed caller-held mutation lease.
 *  \param backend Backend owning the durable attempt.
 *  \param journal_store Store owning the append-only semantic history.
 *  \param declaration Exact immutable declaration identity to resume.
 *  \return Truthful terminal application receipt bound to @p state.
 *
 *  The exact declaration projection body remains historical admission
 *  evidence; successful continuation binds terminal evidence to the current
 *  lease-bound projection supplied for this restart.
 */
[[nodiscard]] PKGAPPLY_API application_receipt
resume_application(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    application_journal_store& journal_store,
    const application_journal_declaration_identity& declaration);

/*! \brief Validate a transaction reopened for one exact durable attempt.
 *  \param target Exact target context.
 *  \param lease Borrowed caller-held mutation lease.
 *  \param backend Backend that reopened the transaction.
 *  \param attempt Owner-authored durable attempt authority.
 *  \param transaction Reopened backend transaction.
 *  \throws application_admission_error For provider, target, or lease mismatch.
 *  \throws application_restart_error If the attempt nonce differs.
 */
PKGAPPLY_API void validate_restarted_backend_transaction(
    const application_target_context& target,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_attempt& attempt,
    const application_backend_transaction& transaction);

} // namespace pkgapply
