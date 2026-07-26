// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

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

inline constexpr std::uint16_t application_request_schema_version = 2;

/*! \brief Immutable application request for one accepted installation plan. */
class installation_application_request final {
public:
  [[nodiscard]] static installation_application_request
  make(pkgplan::installation_plan plan,
       incoming_package_authority incoming,
       application_target_context target,
       application_execution_control control);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  [[nodiscard]] const pkgplan::installation_plan& plan() const noexcept;
  [[nodiscard]] const incoming_package_authority& incoming() const noexcept;
  [[nodiscard]] const application_target_context& target() const noexcept;
  [[nodiscard]] const application_execution_control& control() const noexcept;

private:
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

/*! \brief Immutable application request for one accepted upgrade plan. */
class upgrade_application_request final {
public:
  [[nodiscard]] static upgrade_application_request
  make(pkgplan::upgrade_plan plan,
       incoming_package_authority incoming,
       application_target_context target,
       application_execution_control control);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  [[nodiscard]] const pkgplan::upgrade_plan& plan() const noexcept;
  [[nodiscard]] const incoming_package_authority& incoming() const noexcept;
  [[nodiscard]] const application_target_context& target() const noexcept;
  [[nodiscard]] const application_execution_control& control() const noexcept;

private:
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

/*! \brief Immutable application request for one accepted removal plan. */
class removal_application_request final {
public:
  [[nodiscard]] static removal_application_request
  make(pkgplan::removal_plan plan,
       application_target_context target,
       application_execution_control control);

  [[nodiscard]] std::uint16_t schema_version() const noexcept;
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  [[nodiscard]] const pkgplan::removal_plan& plan() const noexcept;
  [[nodiscard]] const application_target_context& target() const noexcept;
  [[nodiscard]] const application_execution_control& control() const noexcept;

private:
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


/*! \brief Closed operation-specific body of one immutable application request. */
using package_application_request_body = std::variant<
    installation_application_request,
    upgrade_application_request,
    removal_application_request>;

/*! \brief One immutable install, upgrade, or removal application request. */
class package_application_request final {
public:
  explicit package_application_request(installation_application_request request);
  explicit package_application_request(upgrade_application_request request);
  explicit package_application_request(removal_application_request request);

  [[nodiscard]] pkgplan::operation_kind kind() const noexcept;
  [[nodiscard]] const application_request_identity& identity() const noexcept;
  [[nodiscard]] const pkgplan::operation_plan_identity& plan() const noexcept;
  [[nodiscard]] const incoming_package_authority* incoming() const noexcept;
  [[nodiscard]] const application_target_context& target() const noexcept;
  [[nodiscard]] const application_execution_control& control() const noexcept;
  [[nodiscard]] const package_application_request_body& body() const noexcept;

  [[nodiscard]] const installation_application_request*
  installation() const noexcept;
  [[nodiscard]] const upgrade_application_request* upgrade() const noexcept;
  [[nodiscard]] const removal_application_request* removal() const noexcept;

private:
  package_application_request_body body_;
};


} // namespace pkgapply
