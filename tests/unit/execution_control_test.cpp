// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/execution_control.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template<class Function>
void require_invalid(Function&& function, std::string_view message)
{
  bool rejected = false;
  try {
    function();
  }
  catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, message);
}

} // namespace

int main()
{
  using namespace pkgapply;

  const auto baseline = application_execution_control::make(
      application_recovery_requirement::best_effort,
      application_durability_requirement::journal_and_recovery,
      application_cancellation_policy::recover_after_target_mutation,
      4096,
      8192);
  require(baseline.schema_version() == application_execution_control_schema_version &&
              baseline.recovery() == application_recovery_requirement::best_effort &&
              baseline.durability() ==
                  application_durability_requirement::journal_and_recovery &&
              baseline.cancellation() ==
                  application_cancellation_policy::recover_after_target_mutation &&
              baseline.maximum_staging_bytes() == 4096 &&
              baseline.maximum_recovery_bytes() == 8192,
          "execution control lost normalized actuator authority");

  const auto same = application_execution_control::make(
      application_recovery_requirement::best_effort,
      application_durability_requirement::journal_and_recovery,
      application_cancellation_policy::recover_after_target_mutation,
      4096,
      8192);
  require(same == baseline && same.identity() == baseline.identity(),
          "equal execution controls do not have equal identity");

  const auto different = application_execution_control::make(
      application_recovery_requirement::none,
      application_durability_requirement::visibility_only,
      application_cancellation_policy::before_target_mutation_only);
  require(different != baseline && different.identity() != baseline.identity(),
          "execution-control identity ignored policy authority");

  require_invalid(
      [] {
        static_cast<void>(application_execution_control::make(
            application_recovery_requirement::best_effort,
            application_durability_requirement::journal_and_recovery,
            application_cancellation_policy::recover_after_target_mutation,
            0));
      },
      "zero staging limit was admitted");
  require_invalid(
      [] {
        static_cast<void>(application_execution_control::make(
            application_recovery_requirement::best_effort,
            application_durability_requirement::journal_and_recovery,
            application_cancellation_policy::recover_after_target_mutation,
            std::nullopt,
            0));
      },
      "zero recovery limit was admitted");
  require_invalid(
      [] {
        static_cast<void>(application_execution_control::make(
            application_recovery_requirement::exact_prior_state,
            application_durability_requirement::all_application_domains,
            application_cancellation_policy::before_target_mutation_only));
      },
      "exact recovery accepted a pre-mutation-only cancellation policy");

  return 0;
}
