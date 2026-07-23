// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <variant>

#include <libpkgapply/admission.h>
#include <libpkgapply/attempt.h>
#include <libpkgapply/precondition.h>
#include <libpkgapply/result.h>

namespace pkgapply::detail {

/*! \brief One admitted, freshly revalidated backend transaction.
 *
 * This is an internal engine value.  It keeps the exact transaction alive
 * after static authority admission and fresh precondition observation, so the
 * next engine phase cannot reopen the backend or obtain another attempt nonce.
 */
class admitted_application final {
public:
  admitted_application(
      application_attempt attempt,
      application_precondition_check preconditions,
      std::unique_ptr<application_backend_transaction> transaction);

  admitted_application(const admitted_application&) = delete;
  admitted_application& operator=(const admitted_application&) = delete;
  admitted_application(admitted_application&&) noexcept = default;
  admitted_application& operator=(admitted_application&&) noexcept = default;

  [[nodiscard]] const application_attempt& attempt() const noexcept;
  [[nodiscard]] const application_precondition_check&
  preconditions() const noexcept;
  [[nodiscard]] application_backend_transaction& transaction() noexcept;
  [[nodiscard]] const application_backend_transaction&
  transaction() const noexcept;

private:
  application_attempt attempt_;
  application_precondition_check preconditions_;
  std::unique_ptr<application_backend_transaction> transaction_;
};

/*! \brief Refusal or one transaction admitted to the mutation engine. */
class application_engine_admission final {
public:
  [[nodiscard]] static application_engine_admission
  refused(application_receipt receipt);

  [[nodiscard]] static application_engine_admission
  admitted(application_attempt attempt,
           application_precondition_check preconditions,
           std::unique_ptr<application_backend_transaction> transaction);

  application_engine_admission(const application_engine_admission&) = delete;
  application_engine_admission& operator=(
      const application_engine_admission&) = delete;
  application_engine_admission(
      application_engine_admission&&) noexcept = default;
  application_engine_admission& operator=(
      application_engine_admission&&) noexcept = default;

  [[nodiscard]] bool is_admitted() const noexcept;
  [[nodiscard]] const application_receipt* refusal() const noexcept;
  [[nodiscard]] admitted_application* admitted() noexcept;
  [[nodiscard]] const admitted_application* admitted() const noexcept;

private:
  using value_type = std::variant<application_receipt, admitted_application>;
  explicit application_engine_admission(value_type value);
  value_type value_;
};

/*! \brief Admit and freshly revalidate one installation transaction. */
[[nodiscard]] application_engine_admission
admit_application_engine(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Admit and freshly revalidate one upgrade transaction. */
[[nodiscard]] application_engine_admission
admit_application_engine(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend,
    const pkgimage::package_archive& archive);

/*! \brief Admit and freshly revalidate one removal transaction. */
[[nodiscard]] application_engine_admission
admit_application_engine(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    target_mutation_lease& lease,
    application_backend& backend);

} // namespace pkgapply::detail
