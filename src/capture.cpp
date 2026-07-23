// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/capture.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

bool
recovery_selected(const application_execution_control& control)
{
  switch (control.recovery()) {
    case application_recovery_requirement::none:
      return false;
    case application_recovery_requirement::best_effort:
    case application_recovery_requirement::exact_prior_state:
      return true;
  }
  throw std::invalid_argument("invalid application recovery requirement");
}

bool
active_effect_may_destroy_old(
    pkgplan::planned_active_outcome outcome,
    const pkgplan::target_path_observation& before)
{
  if (!before.is_present())
    return false;

  switch (outcome) {
    case pkgplan::planned_active_outcome::activate_incoming:
    case pkgplan::planned_active_outcome::remove_observed:
    case pkgplan::planned_active_outcome::remove_directory_if_empty:
      return true;
    case pkgplan::planned_active_outcome::retain_observed:
    case pkgplan::planned_active_outcome::remain_absent:
      return false;
  }
  throw std::invalid_argument("invalid planned active outcome");
}

template<class Plan>
old_object_capture_plan
prepare(const Plan& plan, const application_execution_control& control)
{
  const bool recovery = recovery_selected(control);
  std::vector<old_object_capture_request> requests;

  for (const auto& decision : plan.paths()) {
    const pkgplan::path_precondition* before =
        plan.preconditions().find(decision.path());
    if (before == nullptr)
      throw std::invalid_argument(
          "plan decision lacks its path precondition");

    const bool for_rejected = decision.rejected() ==
        pkgplan::planned_rejected_outcome::stage_old;
    const bool for_recovery = recovery && active_effect_may_destroy_old(
        decision.active(), before->observation());

    if (for_rejected && !before->observation().is_present())
      throw std::invalid_argument(
          "old-object staging requires a present planning observation");
    if (for_rejected || for_recovery)
      requests.emplace_back(
          decision.path(), for_rejected, for_recovery);
  }

  return old_object_capture_plan(std::move(requests));
}

} // namespace

old_object_capture_plan::old_object_capture_plan(
    std::vector<old_object_capture_request> requests)
    : requests_(std::move(requests))
{
  std::sort(requests_.begin(), requests_.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.path() < rhs.path();
            });
  if (std::adjacent_find(
          requests_.begin(), requests_.end(),
          [](const auto& lhs, const auto& rhs) {
            return lhs.path() == rhs.path();
          }) != requests_.end())
  {
    throw std::invalid_argument("duplicate old-object capture path");
  }
}

const std::vector<old_object_capture_request>&
old_object_capture_plan::requests() const noexcept
{
  return requests_;
}

const old_object_capture_request*
old_object_capture_plan::find(
    const pkgplan::package_path& path) const noexcept
{
  const auto item = std::lower_bound(
      requests_.begin(), requests_.end(), path,
      [](const auto& request, const auto& wanted) {
        return request.path() < wanted;
      });
  return item != requests_.end() && item->path() == path
      ? &*item
      : nullptr;
}

old_object_capture_plan
prepare_old_object_captures(
    const pkgplan::installation_plan& plan,
    const application_execution_control& control)
{
  return prepare(plan, control);
}

old_object_capture_plan
prepare_old_object_captures(
    const pkgplan::upgrade_plan& plan,
    const application_execution_control& control)
{
  return prepare(plan, control);
}

old_object_capture_plan
prepare_old_object_captures(
    const pkgplan::removal_plan& plan,
    const application_execution_control& control)
{
  return prepare(plan, control);
}

} // namespace pkgapply
