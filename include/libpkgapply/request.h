// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file request.h
 *  \brief Immutable operation-specific package application requests.
 */
#pragma once

#include <cstdint>
#include <variant>

#include <libpkgapply/digest.h>
#include <libpkgapply/execution_control.h>
#include <libpkgapply/incoming_package.h>
#include <libpkgapply/target_context.h>
#include <libpkgplan/install.h>
#include <libpkgplan/remove.h>
#include <libpkgplan/upgrade.h>

namespace pkgapply {

/*! \brief Schema version shared by all application request bodies. */
inline constexpr std::uint16_t application_request_schema_version = 1;

/*! \brief Application request for one accepted installation plan. */
class installation_application_request final {
public:
  /*! \brief Validate, identify, and construct an installation request.
   *  \param plan Accepted pure installation plan.
   *  \param incoming Admitted incoming package authority.
   *  \param target Exact application target context.
   *  \param control Actuator-level execution requirements.
   *  \return Immutable installation request.
   *  \throws std::invalid_argument If plan and target identities differ.
   *  \throws incoming_package_error If plan inputs, publication authority, or
   *          archive preconditions differ from `incoming`.
   */
  [[nodiscard]] static installation_application_request
  make(pkgplan::installation_plan plan,
       incoming_package_authority incoming,
       application_target_context target,
       application_execution_control control);

  /*!
   * \brief Return the request schema version.
  *  \return The request schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical request identity.
  *  \return The canonical request identity.
   */
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  /*!
   * \brief Return the retained accepted installation plan.
  *  \return The retained accepted installation plan.
   */
  [[nodiscard]] const pkgplan::installation_plan& plan() const noexcept;
  /*!
   * \brief Return the retained incoming package authority.
  *  \return The retained incoming package authority.
   */
  [[nodiscard]] const incoming_package_authority& incoming() const noexcept;
  /*!
   * \brief Return the retained target context.
  *  \return The retained target context.
   */
  [[nodiscard]] const application_target_context& target() const noexcept;
  /*!
   * \brief Return the retained execution control.
  *  \return The retained execution control.
   */
  [[nodiscard]] const application_execution_control& control() const noexcept;

private:
  /*! \brief Construct a validated request already identified by make(). */
  installation_application_request(
      application_request_identity identity,
      pkgplan::installation_plan plan,
      incoming_package_authority incoming,
      application_target_context target,
      application_execution_control control);

  std::uint16_t schema_version_ = application_request_schema_version;
  application_request_identity identity_;
  pkgplan::installation_plan plan_;
  incoming_package_authority incoming_;
  application_target_context target_;
  application_execution_control control_;
};

/*! \brief Application request for one accepted upgrade plan. */
class upgrade_application_request final {
public:
  /*! \brief Validate, identify, and construct an upgrade request.
   *  \param plan Accepted pure upgrade plan.
   *  \param incoming Admitted incoming package authority.
   *  \param target Exact application target context.
   *  \param control Actuator-level execution requirements.
   *  \return Immutable upgrade request.
   *  \throws std::invalid_argument If plan and target identities differ.
   *  \throws incoming_package_error If plan inputs, publication authority, or
   *          archive preconditions differ from `incoming`.
   */
  [[nodiscard]] static upgrade_application_request
  make(pkgplan::upgrade_plan plan,
       incoming_package_authority incoming,
       application_target_context target,
       application_execution_control control);

  /*!
   * \brief Return the request schema version.
  *  \return The request schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical request identity.
  *  \return The canonical request identity.
   */
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  /*!
   * \brief Return the retained accepted upgrade plan.
  *  \return The retained accepted upgrade plan.
   */
  [[nodiscard]] const pkgplan::upgrade_plan& plan() const noexcept;
  /*!
   * \brief Return the retained incoming package authority.
  *  \return The retained incoming package authority.
   */
  [[nodiscard]] const incoming_package_authority& incoming() const noexcept;
  /*!
   * \brief Return the retained target context.
  *  \return The retained target context.
   */
  [[nodiscard]] const application_target_context& target() const noexcept;
  /*!
   * \brief Return the retained execution control.
  *  \return The retained execution control.
   */
  [[nodiscard]] const application_execution_control& control() const noexcept;

private:
  /*! \brief Construct a validated request already identified by make(). */
  upgrade_application_request(
      application_request_identity identity,
      pkgplan::upgrade_plan plan,
      incoming_package_authority incoming,
      application_target_context target,
      application_execution_control control);

  std::uint16_t schema_version_ = application_request_schema_version;
  application_request_identity identity_;
  pkgplan::upgrade_plan plan_;
  incoming_package_authority incoming_;
  application_target_context target_;
  application_execution_control control_;
};

/*! \brief Application request for one accepted removal plan. */
class removal_application_request final {
public:
  /*! \brief Validate, identify, and construct a removal request.
   *  \param plan Accepted pure removal plan.
   *  \param target Exact application target context.
   *  \param control Actuator-level execution requirements.
   *  \return Immutable removal request.
   *  \throws std::invalid_argument If plan and target differ or the removal
   *          plan unexpectedly carries incoming archive authority.
   */
  [[nodiscard]] static removal_application_request
  make(pkgplan::removal_plan plan,
       application_target_context target,
       application_execution_control control);

  /*!
   * \brief Return the request schema version.
  *  \return The request schema version.
   */
  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  /*!
   * \brief Return the canonical request identity.
  *  \return The canonical request identity.
   */
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  /*!
   * \brief Return the retained accepted removal plan.
  *  \return The retained accepted removal plan.
   */
  [[nodiscard]] const pkgplan::removal_plan& plan() const noexcept;
  /*!
   * \brief Return the retained target context.
  *  \return The retained target context.
   */
  [[nodiscard]] const application_target_context& target() const noexcept;
  /*!
   * \brief Return the retained execution control.
  *  \return The retained execution control.
   */
  [[nodiscard]] const application_execution_control& control() const noexcept;

private:
  /*! \brief Construct a validated request already identified by make(). */
  removal_application_request(
      application_request_identity identity,
      pkgplan::removal_plan plan,
      application_target_context target,
      application_execution_control control);

  std::uint16_t schema_version_ = application_request_schema_version;
  application_request_identity identity_;
  pkgplan::removal_plan plan_;
  application_target_context target_;
  application_execution_control control_;
};

/*! \brief Closed operation-specific body of an application request. */
using package_application_request_body = std::variant<
    installation_application_request,
    upgrade_application_request,
    removal_application_request>;

/*! \brief One immutable installation, upgrade, or removal request. */
class package_application_request final {
public:
  /*!
   * \brief Construct from an installation request body.
  *  \param request Operation-specific request retained by this value.
   */
  explicit package_application_request(installation_application_request request);
  /*!
   * \brief Construct from an upgrade request body.
  *  \param request Operation-specific request retained by this value.
   */
  explicit package_application_request(upgrade_application_request request);
  /*!
   * \brief Construct from a removal request body.
  *  \param request Operation-specific request retained by this value.
   */
  explicit package_application_request(removal_application_request request);

  /*!
   * \brief Return the operation kind of the active request body.
  *  \return The operation kind of the active request body.
   */
  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  /*!
   * \brief Return the canonical application-request identity.
  *  \return The canonical application-request identity.
   */
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  /*!
   * \brief Return the accepted operation-plan identity.
  *  \return The accepted operation-plan identity.
   */
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  /*!
   * \brief Return incoming authority, or `nullptr` for removal.
  *  \return Incoming authority, or `nullptr` for removal.
   */
  [[nodiscard]] const incoming_package_authority* incoming() const noexcept;
  /*!
   * \brief Return the exact target context.
  *  \return The exact target context.
   */
  [[nodiscard]] const application_target_context& target() const noexcept;
  /*!
   * \brief Return actuator-level execution control.
  *  \return Actuator-level execution control.
   */
  [[nodiscard]] const application_execution_control& control() const noexcept;
  /*!
   * \brief Return the active operation-specific variant.
  *  \return The active operation-specific variant.
   */
  [[nodiscard]] const package_application_request_body& body() const noexcept;

  /*!
   * \brief Return installation body when active, otherwise `nullptr`.
  *  \return Installation body when active, otherwise `nullptr`.
   */
  [[nodiscard]] const installation_application_request*
  installation() const noexcept;
  /*!
   * \brief Return upgrade body when active, otherwise `nullptr`.
  *  \return Upgrade body when active, otherwise `nullptr`.
   */
  [[nodiscard]] const upgrade_application_request* upgrade() const noexcept;
  /*!
   * \brief Return removal body when active, otherwise `nullptr`.
  *  \return Removal body when active, otherwise `nullptr`.
   */
  [[nodiscard]] const removal_application_request* removal() const noexcept;

private:
  package_application_request_body body_;
};

} // namespace pkgapply
