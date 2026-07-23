// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

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
  incoming_entry = 1,
  obsolete_old_path = 2,
  structural_parent = 3,
  installed_owned_path = 4,
};

/*! \brief Actual completion state of one active or rejected effect. */
enum class application_effect_status : std::uint8_t {
  not_attempted = 1,
  completed = 2,
  conditional_retained = 3,
  failed = 4,
  indeterminate = 5,
};

/*! \brief Whether completed effects are eligible for ownership publication. */
enum class ownership_publication_status : std::uint8_t {
  ineligible = 1,
  eligible = 2,
};

/*! \brief Explicit present, absent, or unknown application observation. */
class application_path_observation final {
public:
  [[nodiscard]] static application_path_observation
  present(completed_object_fact object);
  [[nodiscard]] static application_path_observation
  absent(pkgplan::package_path path);
  [[nodiscard]] static application_path_observation
  unknown(pkgplan::package_path path);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] fact_state state() const noexcept;
  [[nodiscard]] const std::optional<completed_object_fact>& object() const noexcept;

private:
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

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] application_path_role role() const noexcept;
  [[nodiscard]] pkgplan::planned_active_outcome requested_active() const noexcept;
  [[nodiscard]] pkgplan::planned_rejected_outcome requested_rejected() const noexcept;
  [[nodiscard]] const std::optional<pkgimage::entry_id>&
  incoming_entry() const noexcept;
  [[nodiscard]] const pkgplan::path_ownership_transition& ownership() const noexcept;
  [[nodiscard]] application_effect_status active_status() const noexcept;
  [[nodiscard]] application_effect_status rejected_status() const noexcept;
  [[nodiscard]] const application_path_observation& before() const noexcept;
  [[nodiscard]] const application_path_observation& after() const noexcept;
  [[nodiscard]] const std::optional<rejected_object_record_identity>&
  rejected_object() const noexcept;
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
