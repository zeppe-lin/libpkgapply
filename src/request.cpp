// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/request.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

enum class request_kind : std::uint8_t {
  install = 1,
  upgrade = 2,
  remove = 3,
};

template<class Plan>
application_request_identity identify_incoming_request(
    request_kind kind,
    const Plan& plan,
    const incoming_package_authority& incoming,
    const application_target_context& target,
    const application_execution_control& control)
{
  detail::canonical_record record(application_request_identity::canonical_domain());
  record.append_u16(application_request_schema_version);
  record.append_u8(static_cast<std::uint8_t>(kind));
  record.append_bytes(plan.identity().string());
  record.append_bool(true);
  record.append_digest(incoming.identity());
  record.append_digest(target.identity());
  record.append_digest(control.identity());
  return detail::identity_factory::from_sha256<application_request_identity>(
      record.sha256());
}

template<class Plan>
application_request_identity identify_removal_request(
    request_kind kind,
    const Plan& plan,
    const application_target_context& target,
    const application_execution_control& control)
{
  detail::canonical_record record(application_request_identity::canonical_domain());
  record.append_u16(application_request_schema_version);
  record.append_u8(static_cast<std::uint8_t>(kind));
  record.append_bytes(plan.identity().string());
  record.append_bool(false);
  record.append_digest(target.identity());
  record.append_digest(control.identity());
  return detail::identity_factory::from_sha256<application_request_identity>(
      record.sha256());
}

template<class Plan>
void validate_target(const Plan& plan, const application_target_context& target)
{
  if (plan.preconditions().target() != target.target())
    throw std::invalid_argument("application target does not match accepted plan");
}

void require_plan_binding(bool condition, const char* message)
{
  if (!condition)
    throw incoming_package_error(
        incoming_package_error_code::plan_binding, message);
}

template<class Plan>
void validate_incoming(const Plan& plan,
                       const incoming_package_authority& incoming)
{
  const auto& candidate = incoming.candidate();
  const auto& artifact = incoming.artifact();
  const auto& inputs = plan.inputs();
  const auto& publication = plan.publication();
  const auto& image = incoming.image();

  require_plan_binding(
      inputs.candidate_release() == candidate.release().identity() &&
          inputs.artifact_release() == candidate.release().identity() &&
          plan.release() == candidate.release() &&
          publication.release() == candidate.release(),
      "accepted plan release differs from sealed build source");

  require_plan_binding(
      inputs.candidate() == candidate.identity() &&
          inputs.candidate_control() == candidate.control_projection() &&
          publication.candidate() == candidate.identity() &&
          publication.installed_control() == candidate.control_projection(),
      "accepted plan candidate control differs from sealed build source");

  require_plan_binding(
      inputs.artifact() == artifact.artifact() &&
          publication.artifact() == artifact.artifact() &&
          inputs.artifact_manifest() == artifact.manifest() &&
          publication.artifact_manifest() == artifact.manifest() &&
          inputs.expected_archive() == image.receipt().archive_digest() &&
          inputs.observed_archive() == image.receipt().archive_digest() &&
          inputs.image() == image.image().identity() &&
          inputs.inspection_receipt() == image.receipt().identity(),
      "accepted plan artifact facts differ from verified build image");


  const auto& archive_precondition = plan.preconditions().incoming_archive();
  require_plan_binding(
      archive_precondition.has_value() &&
          archive_precondition->archive() == image.receipt().archive_digest() &&
          archive_precondition->image() == image.image().identity() &&
          archive_precondition->inspection_receipt() ==
              image.receipt().identity(),
      "accepted plan precondition differs from verified build image");

}

} // namespace

installation_application_request installation_application_request::make(
    pkgplan::installation_plan plan,
    incoming_package_authority incoming,
    application_target_context target,
    application_execution_control control)
{
  validate_target(plan, target);
  validate_incoming(plan, incoming);
  application_request_identity identity = identify_incoming_request(
      request_kind::install, plan, incoming, target, control);
  return installation_application_request(
      std::move(identity), std::move(plan), std::move(incoming),
      std::move(target), std::move(control));
}

installation_application_request::installation_application_request(
    application_request_identity identity,
    pkgplan::installation_plan plan,
    incoming_package_authority incoming,
    application_target_context target,
    application_execution_control control)
    : identity_(std::move(identity)), plan_(std::move(plan)),
      incoming_(std::move(incoming)), target_(std::move(target)),
      control_(std::move(control))
{
}

std::uint16_t installation_application_request::schema_version() const noexcept
{
  return schema_version_;
}

const application_request_identity&
installation_application_request::identity() const noexcept
{
  return identity_;
}

const pkgplan::installation_plan&
installation_application_request::plan() const noexcept
{
  return plan_;
}

const incoming_package_authority&
installation_application_request::incoming() const noexcept
{
  return incoming_;
}

const application_target_context&
installation_application_request::target() const noexcept
{
  return target_;
}

const application_execution_control&
installation_application_request::control() const noexcept
{
  return control_;
}

upgrade_application_request upgrade_application_request::make(
    pkgplan::upgrade_plan plan,
    incoming_package_authority incoming,
    application_target_context target,
    application_execution_control control)
{
  validate_target(plan, target);
  validate_incoming(plan, incoming);
  application_request_identity identity = identify_incoming_request(
      request_kind::upgrade, plan, incoming, target, control);
  return upgrade_application_request(
      std::move(identity), std::move(plan), std::move(incoming),
      std::move(target), std::move(control));
}

upgrade_application_request::upgrade_application_request(
    application_request_identity identity,
    pkgplan::upgrade_plan plan,
    incoming_package_authority incoming,
    application_target_context target,
    application_execution_control control)
    : identity_(std::move(identity)), plan_(std::move(plan)),
      incoming_(std::move(incoming)), target_(std::move(target)),
      control_(std::move(control))
{
}

std::uint16_t upgrade_application_request::schema_version() const noexcept
{
  return schema_version_;
}

const application_request_identity&
upgrade_application_request::identity() const noexcept
{
  return identity_;
}

const pkgplan::upgrade_plan& upgrade_application_request::plan() const noexcept
{
  return plan_;
}

const incoming_package_authority&
upgrade_application_request::incoming() const noexcept
{
  return incoming_;
}

const application_target_context&
upgrade_application_request::target() const noexcept
{
  return target_;
}

const application_execution_control&
upgrade_application_request::control() const noexcept
{
  return control_;
}

removal_application_request removal_application_request::make(
    pkgplan::removal_plan plan,
    application_target_context target,
    application_execution_control control)
{
  validate_target(plan, target);
  if (plan.preconditions().incoming_archive())
    throw std::invalid_argument("removal plan carries incoming archive authority");
  application_request_identity identity = identify_removal_request(
      request_kind::remove, plan, target, control);
  return removal_application_request(
      std::move(identity), std::move(plan), std::move(target),
      std::move(control));
}

removal_application_request::removal_application_request(
    application_request_identity identity,
    pkgplan::removal_plan plan,
    application_target_context target,
    application_execution_control control)
    : identity_(std::move(identity)), plan_(std::move(plan)),
      target_(std::move(target)), control_(std::move(control))
{
}

std::uint16_t removal_application_request::schema_version() const noexcept
{
  return schema_version_;
}

const application_request_identity&
removal_application_request::identity() const noexcept
{
  return identity_;
}

const pkgplan::removal_plan& removal_application_request::plan() const noexcept
{
  return plan_;
}

const application_target_context&
removal_application_request::target() const noexcept
{
  return target_;
}

const application_execution_control&
removal_application_request::control() const noexcept
{
  return control_;
}

namespace {

template<typename Result, typename Visitor>
const Result& visit_request_result(const package_application_request_body& body,
                                   Visitor visitor)
{
  return std::visit(
      [&visitor](const auto& request) -> const Result& {
        return visitor(request);
      },
      body);
}

} // namespace

package_application_request::package_application_request(
    installation_application_request request)
    : body_(std::move(request))
{
}

package_application_request::package_application_request(
    upgrade_application_request request)
    : body_(std::move(request))
{
}

package_application_request::package_application_request(
    removal_application_request request)
    : body_(std::move(request))
{
}

pkgplan::operation_kind package_application_request::kind() const noexcept
{
  return std::visit(
      [](const auto& request) { return request.plan().kind(); }, body_);
}

const application_request_identity&
package_application_request::identity() const noexcept
{
  return visit_request_result<application_request_identity>(
      body_, [](const auto& request) -> const application_request_identity& {
        return request.identity();
      });
}

const pkgplan::operation_plan_identity&
package_application_request::plan() const noexcept
{
  return visit_request_result<pkgplan::operation_plan_identity>(
      body_, [](const auto& request) -> const pkgplan::operation_plan_identity& {
        return request.plan().identity();
      });
}

const incoming_package_authority*
package_application_request::incoming() const noexcept
{
  if (const auto* install = installation())
    return &install->incoming();
  if (const auto* replacement = upgrade())
    return &replacement->incoming();
  return nullptr;
}

const application_target_context&
package_application_request::target() const noexcept
{
  return visit_request_result<application_target_context>(
      body_, [](const auto& request) -> const application_target_context& {
        return request.target();
      });
}

const application_execution_control&
package_application_request::control() const noexcept
{
  return visit_request_result<application_execution_control>(
      body_, [](const auto& request) -> const application_execution_control& {
        return request.control();
      });
}

const package_application_request_body&
package_application_request::body() const noexcept
{
  return body_;
}

const installation_application_request*
package_application_request::installation() const noexcept
{
  return std::get_if<installation_application_request>(&body_);
}

const upgrade_application_request*
package_application_request::upgrade() const noexcept
{
  return std::get_if<upgrade_application_request>(&body_);
}

const removal_application_request*
package_application_request::removal() const noexcept
{
  return std::get_if<removal_application_request>(&body_);
}

} // namespace pkgapply
