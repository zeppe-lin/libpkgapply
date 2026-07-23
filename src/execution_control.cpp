// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/execution_control.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

std::uint8_t
canonical_recovery(application_recovery_requirement value)
{
  switch (value) {
    case application_recovery_requirement::none:
      return 1;
    case application_recovery_requirement::best_effort:
      return 2;
    case application_recovery_requirement::exact_prior_state:
      return 3;
  }
  throw std::invalid_argument("invalid application recovery requirement");
}

std::uint8_t
canonical_durability(application_durability_requirement value)
{
  switch (value) {
    case application_durability_requirement::visibility_only:
      return 1;
    case application_durability_requirement::journal_and_recovery:
      return 2;
    case application_durability_requirement::all_application_domains:
      return 3;
  }
  throw std::invalid_argument("invalid application durability requirement");
}

std::uint8_t
canonical_cancellation(application_cancellation_policy value)
{
  switch (value) {
    case application_cancellation_policy::before_target_mutation_only:
      return 1;
    case application_cancellation_policy::recover_after_target_mutation:
      return 2;
  }
  throw std::invalid_argument("invalid application cancellation policy");
}

application_execution_control_identity
identify_control(
    application_recovery_requirement recovery,
    application_durability_requirement durability,
    application_cancellation_policy cancellation,
    const std::optional<std::uint64_t>& maximum_staging_bytes,
    const std::optional<std::uint64_t>& maximum_recovery_bytes)
{
  detail::canonical_record record(
      application_execution_control_identity::canonical_domain());
  record.append_u16(application_execution_control_schema_version);
  record.append_u8(canonical_recovery(recovery));
  record.append_u8(canonical_durability(durability));
  record.append_u8(canonical_cancellation(cancellation));
  record.append_bool(maximum_staging_bytes.has_value());
  if (maximum_staging_bytes)
    record.append_u64(*maximum_staging_bytes);
  record.append_bool(maximum_recovery_bytes.has_value());
  if (maximum_recovery_bytes)
    record.append_u64(*maximum_recovery_bytes);

  return detail::identity_factory::from_sha256<
      application_execution_control_identity>(record.sha256());
}

} // namespace

application_execution_control
application_execution_control::make(
    application_recovery_requirement recovery,
    application_durability_requirement durability,
    application_cancellation_policy cancellation,
    std::optional<std::uint64_t> maximum_staging_bytes,
    std::optional<std::uint64_t> maximum_recovery_bytes)
{
  if (maximum_staging_bytes && *maximum_staging_bytes == 0)
    throw std::invalid_argument("maximum staging bytes must be nonzero");
  if (maximum_recovery_bytes && *maximum_recovery_bytes == 0)
    throw std::invalid_argument("maximum recovery bytes must be nonzero");

  if (recovery == application_recovery_requirement::exact_prior_state &&
      cancellation ==
          application_cancellation_policy::before_target_mutation_only)
  {
    throw std::invalid_argument(
        "exact recovery requires post-mutation recovery cancellation policy");
  }

  application_execution_control_identity identity = identify_control(
      recovery,
      durability,
      cancellation,
      maximum_staging_bytes,
      maximum_recovery_bytes);

  return application_execution_control(
      std::move(identity),
      recovery,
      durability,
      cancellation,
      maximum_staging_bytes,
      maximum_recovery_bytes);
}

application_execution_control::application_execution_control(
    application_execution_control_identity identity,
    application_recovery_requirement recovery,
    application_durability_requirement durability,
    application_cancellation_policy cancellation,
    std::optional<std::uint64_t> maximum_staging_bytes,
    std::optional<std::uint64_t> maximum_recovery_bytes)
    : identity_(std::move(identity)),
      recovery_(recovery),
      durability_(durability),
      cancellation_(cancellation),
      maximum_staging_bytes_(maximum_staging_bytes),
      maximum_recovery_bytes_(maximum_recovery_bytes)
{
}

std::uint16_t
application_execution_control::schema_version() const noexcept
{
  return schema_version_;
}

const application_execution_control_identity&
application_execution_control::identity() const noexcept
{
  return identity_;
}

application_recovery_requirement
application_execution_control::recovery() const noexcept
{
  return recovery_;
}

application_durability_requirement
application_execution_control::durability() const noexcept
{
  return durability_;
}

application_cancellation_policy
application_execution_control::cancellation() const noexcept
{
  return cancellation_;
}

const std::optional<std::uint64_t>&
application_execution_control::maximum_staging_bytes() const noexcept
{
  return maximum_staging_bytes_;
}

const std::optional<std::uint64_t>&
application_execution_control::maximum_recovery_bytes() const noexcept
{
  return maximum_recovery_bytes_;
}

bool
operator==(const application_execution_control& lhs,
           const application_execution_control& rhs) noexcept
{
  return lhs.identity_ == rhs.identity_ && lhs.recovery_ == rhs.recovery_ &&
         lhs.durability_ == rhs.durability_ &&
         lhs.cancellation_ == rhs.cancellation_ &&
         lhs.maximum_staging_bytes_ == rhs.maximum_staging_bytes_ &&
         lhs.maximum_recovery_bytes_ == rhs.maximum_recovery_bytes_;
}

bool
operator!=(const application_execution_control& lhs,
           const application_execution_control& rhs) noexcept
{
  return !(lhs == rhs);
}

} // namespace pkgapply
