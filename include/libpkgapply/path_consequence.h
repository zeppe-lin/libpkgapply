// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file path_consequence.h
 *  \brief Requested and observed consequences for operated package paths.
 */
#pragma once

#include <cstdint>
#include <optional>

#include <libpkgapply/digest.h>
#include <libpkgapply/object_fact.h>
#include <libpkgimage/package_entry.h>
#include <libpkgplan/plan.h>

namespace pkgapply {

/*! \brief Semantic role retained in application evidence. */
enum class application_path_role : std::uint8_t {
  incoming_entry = 1, /*!< Path corresponds to an incoming image entry. */
  obsolete_old_path = 2, /*!< Path exists only in the installed old package. */
  structural_parent = 3, /*!< Path is an operated structural parent. */
  installed_owned_path = 4, /*!< Path is retained from installed ownership. */
};

/*! \brief Actual completion state of one active or rejected effect. */
enum class application_effect_status : std::uint8_t {
  not_attempted = 1, /*!< Mechanism step was never attempted. */
  completed = 2, /*!< Requested effect completed. */
  conditional_retained = 3, /*!< Conditional directory removal retained it. */
  failed = 4, /*!< Backend reported a determinate failure. */
  indeterminate = 5, /*!< Completion could not be established. */
};

/*! \brief Whether a completed consequence may publish ownership. */
enum class ownership_publication_status : std::uint8_t {
  ineligible = 1, /*!< Consequence cannot become installed ownership. */
  eligible = 2, /*!< Consequence is complete enough for publication. */
};

/*! \brief Explicit present, absent, or unknown path observation. */
class application_path_observation final {
public:
  /*!
   * \brief Construct a present observation from complete object evidence.
  *  \param object Object authority to inspect or transform.
  *  \return A present observation retaining @p object.
   */
  [[nodiscard]] static application_path_observation
  present(completed_object_fact object);
  /*!
   * \brief Construct a known-absent observation.
  *  \param path Logical package path associated with the operation.
  *  \return A known-absent observation for @p path.
   */
  [[nodiscard]] static application_path_observation
  absent(pkgplan::package_path path);
  /*!
   * \brief Construct an observation whose presence is unknown.
  *  \param path Logical package path associated with the operation.
  *  \return An unknown-presence observation for @p path.
   */
  [[nodiscard]] static application_path_observation
  unknown(pkgplan::package_path path);

  /*!
   * \brief Return the exact observed path.
  *  \return The exact observed path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return known, not-applicable-for-absence, or unknown state.
  *  \return Known, not-applicable-for-absence, or unknown state.
   */
  [[nodiscard]] fact_state state() const noexcept;
  /*!
   * \brief Return object evidence only for a present observation.
  *  \return Object evidence only for a present observation.
   */
  [[nodiscard]] const std::optional<completed_object_fact>&
  object() const noexcept;

private:
  /*! \brief Construct a validated observation state. */
  application_path_observation(pkgplan::package_path path,
                               fact_state state,
                               std::optional<completed_object_fact> object);

  pkgplan::package_path path_;
  fact_state state_;
  std::optional<completed_object_fact> object_;
};

/*! \brief Planned and actual consequences for one operated logical path. */
class application_path_consequence final {
public:
  /*! \brief Validate and construct one path consequence.
   *  \param path Exact operated path.
   *  \param role Semantic role in the accepted plan.
   *  \param requested_active Planned active namespace outcome.
   *  \param requested_rejected Planned rejected-store outcome.
   *  \param incoming_entry Incoming image entry when role requires one.
   *  \param ownership Planner-owned ownership transition.
   *  \param active_status Actual active-effect status.
   *  \param rejected_status Actual rejected-effect status.
   *  \param before Fresh admitted pre-mutation observation.
   *  \param after Fresh resulting observation.
   *  \param rejected_object Published rejected-object record when completed.
   *  \param publication Ownership publication eligibility.
   *  \throws std::invalid_argument If paths, entry applicability, rejected
   *          evidence, conditional retention, or publication eligibility are
   *          internally inconsistent.
   */
  application_path_consequence(
      pkgplan::package_path path,
      application_path_role role,
      pkgplan::planned_active_outcome requested_active,
      pkgplan::planned_rejected_outcome requested_rejected,
      std::optional<pkgimage::entry_id> incoming_entry,
      pkgplan::path_ownership_transition ownership,
      application_effect_status active_status,
      application_effect_status rejected_status,
      application_path_observation before,
      application_path_observation after,
      std::optional<rejected_object_record_identity> rejected_object,
      ownership_publication_status publication);

  /*!
   * \brief Return the exact operated path.
  *  \return The exact operated path.
   */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*!
   * \brief Return the path's semantic role.
  *  \return The path's semantic role.
   */
  [[nodiscard]] application_path_role role() const noexcept;
  /*!
   * \brief Return the planned active outcome.
  *  \return The planned active outcome.
   */
  [[nodiscard]] pkgplan::planned_active_outcome requested_active() const noexcept;
  /*!
   * \brief Return the planned rejected-store outcome.
  *  \return The planned rejected-store outcome.
   */
  [[nodiscard]] pkgplan::planned_rejected_outcome
  requested_rejected() const noexcept;
  /*!
   * \brief Return the incoming image entry when applicable.
  *  \return The incoming image entry when applicable.
   */
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;
  /*!
   * \brief Return the planner-owned ownership transition.
  *  \return The planner-owned ownership transition.
   */
  [[nodiscard]] const pkgplan::path_ownership_transition&
  ownership() const noexcept;
  /*!
   * \brief Return actual active-effect completion status.
  *  \return Actual active-effect completion status.
   */
  [[nodiscard]] application_effect_status active_status() const noexcept;
  /*!
   * \brief Return actual rejected-effect completion status.
  *  \return Actual rejected-effect completion status.
   */
  [[nodiscard]] application_effect_status rejected_status() const noexcept;
  /*!
   * \brief Return fresh admitted pre-mutation observation.
  *  \return Fresh admitted pre-mutation observation.
   */
  [[nodiscard]] const application_path_observation& before() const noexcept;
  /*!
   * \brief Return fresh resulting observation.
  *  \return Fresh resulting observation.
   */
  [[nodiscard]] const application_path_observation& after() const noexcept;
  /*!
   * \brief Return the rejected-object record identity when published.
  *  \return The rejected-object record identity when published.
   */
  [[nodiscard]] const std::optional<rejected_object_record_identity>&
  rejected_object() const noexcept;
  /*!
   * \brief Return ownership publication eligibility.
  *  \return Ownership publication eligibility.
   */
  [[nodiscard]] ownership_publication_status publication() const noexcept;

private:
  pkgplan::package_path path_;
  application_path_role role_;
  pkgplan::planned_active_outcome requested_active_;
  pkgplan::planned_rejected_outcome requested_rejected_;
  std::optional<pkgimage::entry_id> incoming_entry_;
  pkgplan::path_ownership_transition ownership_;
  application_effect_status active_status_;
  application_effect_status rejected_status_;
  application_path_observation before_;
  application_path_observation after_;
  std::optional<rejected_object_record_identity> rejected_object_;
  ownership_publication_status publication_;
};

} // namespace pkgapply
