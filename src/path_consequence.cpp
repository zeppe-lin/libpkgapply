// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/path_consequence.h>

#include <stdexcept>
#include <utility>

namespace pkgapply {

application_path_observation
application_path_observation::present(completed_object_fact object)
{
  pkgplan::package_path path = object.path();
  return application_path_observation(
      std::move(path), fact_state::known, std::move(object));
}

application_path_observation
application_path_observation::absent(pkgplan::package_path path)
{
  return application_path_observation(
      std::move(path), fact_state::not_applicable, std::nullopt);
}

application_path_observation
application_path_observation::unknown(pkgplan::package_path path)
{
  return application_path_observation(
      std::move(path), fact_state::unknown, std::nullopt);
}

application_path_observation::application_path_observation(
    pkgplan::package_path path,
    fact_state state,
    std::optional<completed_object_fact> object)
    : path_(std::move(path)), state_(state), object_(std::move(object))
{
  if ((state_ == fact_state::known) != object_.has_value())
    throw std::invalid_argument("application path observation has invalid state");
  if (object_ && object_->path() != path_)
    throw std::invalid_argument("application path observation path mismatch");
}

const pkgplan::package_path&
application_path_observation::path() const noexcept
{ return path_; }

fact_state
application_path_observation::state() const noexcept
{ return state_; }

const std::optional<completed_object_fact>&
application_path_observation::object() const noexcept
{ return object_; }

application_path_consequence::application_path_consequence(
    pkgplan::package_path path,
    application_path_role role,
    pkgplan::planned_active_outcome requested_active,
    pkgplan::planned_rejected_outcome requested_rejected,
    pkgplan::path_ownership_transition ownership,
    application_effect_status active_status,
    application_effect_status rejected_status,
    application_path_observation before,
    application_path_observation after,
    std::optional<rejected_object_record_identity> rejected_object,
    ownership_publication_status publication)
    : path_(std::move(path)),
      role_(role),
      requested_active_(requested_active),
      requested_rejected_(requested_rejected),
      ownership_(std::move(ownership)),
      active_status_(active_status),
      rejected_status_(rejected_status),
      before_(std::move(before)),
      after_(std::move(after)),
      rejected_object_(std::move(rejected_object)),
      publication_(publication)
{
  if (before_.path() != path_ || after_.path() != path_)
    throw std::invalid_argument("application consequence observation path mismatch");

  const bool rejected_requested =
      requested_rejected_ != pkgplan::planned_rejected_outcome::none;
  if (!rejected_requested && rejected_object_)
    throw std::invalid_argument("unplanned rejected object identity");

  if (publication_ == ownership_publication_status::eligible &&
      (active_status_ != application_effect_status::completed &&
       active_status_ != application_effect_status::conditional_retained))
  {
    throw std::invalid_argument(
        "ownership publication requires a completed active consequence");
  }
}

const pkgplan::package_path& application_path_consequence::path() const noexcept
{ return path_; }
application_path_role application_path_consequence::role() const noexcept
{ return role_; }
pkgplan::planned_active_outcome
application_path_consequence::requested_active() const noexcept
{ return requested_active_; }
pkgplan::planned_rejected_outcome
application_path_consequence::requested_rejected() const noexcept
{ return requested_rejected_; }
const pkgplan::path_ownership_transition&
application_path_consequence::ownership() const noexcept
{ return ownership_; }
application_effect_status
application_path_consequence::active_status() const noexcept
{ return active_status_; }
application_effect_status
application_path_consequence::rejected_status() const noexcept
{ return rejected_status_; }
const application_path_observation&
application_path_consequence::before() const noexcept
{ return before_; }
const application_path_observation&
application_path_consequence::after() const noexcept
{ return after_; }
const std::optional<rejected_object_record_identity>&
application_path_consequence::rejected_object() const noexcept
{ return rejected_object_; }
ownership_publication_status
application_path_consequence::publication() const noexcept
{ return publication_; }

} // namespace pkgapply
