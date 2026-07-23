// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/precondition.h>

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pkgapply {
namespace {

using field = application_precondition_field;
using failure_kind = application_precondition_failure_kind;

void
add_failure(std::vector<application_precondition_failure>& failures,
            const pkgplan::package_path& path,
            field failed_field,
            failure_kind kind)
{
  failures.emplace_back(path, failed_field, kind);
}

completed_object_kind
completed_kind(pkgplan::filesystem_object_kind kind)
{
  switch (kind) {
    case pkgplan::filesystem_object_kind::regular:
      return completed_object_kind::regular;
    case pkgplan::filesystem_object_kind::directory:
      return completed_object_kind::directory;
    case pkgplan::filesystem_object_kind::symlink:
      return completed_object_kind::symlink;
    case pkgplan::filesystem_object_kind::fifo:
      return completed_object_kind::fifo;
    case pkgplan::filesystem_object_kind::character_device:
      return completed_object_kind::character_device;
    case pkgplan::filesystem_object_kind::block_device:
      return completed_object_kind::block_device;
    case pkgplan::filesystem_object_kind::socket:
      return completed_object_kind::socket;
    case pkgplan::filesystem_object_kind::other:
      return completed_object_kind::other;
  }
  throw std::invalid_argument("invalid planning filesystem object kind");
}

template<class Value>
void
compare_required(const pkgplan::package_path& path,
                 field compared_field,
                 const qualified_fact<Value>& observed,
                 const Value& expected,
                 std::vector<application_precondition_failure>& failures)
{
  if (observed.state() != fact_state::known) {
    add_failure(failures, path, compared_field, failure_kind::unknown);
    return;
  }
  if (*observed.value() != expected)
    add_failure(failures, path, compared_field, failure_kind::mismatch);
}

bool
equal_regular_content(
    const completed_regular_content_identity& observed,
    const pkgplan::filesystem_regular_content_identity& expected) noexcept
{
  return observed.bytes().size() == expected.bytes().size() &&
      std::equal(observed.bytes().begin(), observed.bytes().end(),
                 expected.bytes().begin());
}

void
compare_regular_content(
    const pkgplan::package_path& path,
    const qualified_fact<completed_regular_content_identity>& observed,
    const pkgplan::filesystem_regular_content_identity& expected,
    std::vector<application_precondition_failure>& failures)
{
  if (observed.state() != fact_state::known) {
    add_failure(failures, path, field::regular_content,
                failure_kind::unknown);
    return;
  }
  if (!equal_regular_content(*observed.value(), expected))
    add_failure(failures, path, field::regular_content,
                failure_kind::mismatch);
}

void
compare_object(
    const pkgplan::filesystem_object_metadata& expected,
    const completed_object_fact& observed,
    std::vector<application_precondition_failure>& failures)
{
  const pkgplan::package_path& path = observed.path();
  if (completed_kind(expected.kind()) != observed.kind()) {
    add_failure(failures, path, field::object_kind, failure_kind::mismatch);
    return;
  }

  compare_required(path, field::mode, observed.mode(), expected.mode(), failures);
  compare_required(path, field::uid, observed.uid(), expected.uid(), failures);
  compare_required(path, field::gid, observed.gid(), expected.gid(), failures);

  if (expected.size())
    compare_required(path, field::size, observed.size(), *expected.size(), failures);

  if (expected.mtime()) {
    const completed_object_timestamp timestamp{
        expected.mtime()->seconds(), expected.mtime()->nanoseconds()};
    compare_required(path, field::mtime, observed.mtime(), timestamp, failures);
  }

  if (expected.regular_content())
    compare_regular_content(path, observed.regular_content(),
                            *expected.regular_content(), failures);

  if (expected.symlink_target())
    compare_required(path, field::symlink_target, observed.symlink_target(),
                     *expected.symlink_target(), failures);

  if (expected.device()) {
    const completed_device_number device{
        expected.device()->major(), expected.device()->minor()};
    compare_required(path, field::device_number, observed.device(), device,
                     failures);
  }
}

void
compare_path(const pkgplan::path_precondition& expected,
             const application_path_observation& observed,
             std::vector<application_precondition_failure>& failures)
{
  const pkgplan::target_path_observation& planned = expected.observation();
  if (observed.state() == fact_state::unknown) {
    add_failure(failures, expected.path(), field::presence,
                failure_kind::unknown);
    return;
  }

  const bool observed_present = observed.state() == fact_state::known;
  if (planned.is_present() != observed_present) {
    add_failure(failures, expected.path(), field::presence,
                failure_kind::mismatch);
    return;
  }

  if (!planned.is_present())
    return;
  if (!observed.object())
    throw std::logic_error("known application observation lacks object fact");
  if (planned.object() == nullptr)
    throw std::logic_error("present planning observation lacks object metadata");
  compare_object(*planned.object(), *observed.object(), failures);
}

} // namespace

application_precondition_failure::application_precondition_failure(
    pkgplan::package_path path,
    application_precondition_field field,
    application_precondition_failure_kind kind)
    : path_(std::move(path)), field_(field), kind_(kind)
{
  const auto field_value = static_cast<std::uint8_t>(field_);
  const auto kind_value = static_cast<std::uint8_t>(kind_);
  if (field_value < 1 || field_value > 10)
    throw std::invalid_argument("invalid application precondition field");
  if (kind_value < 1 || kind_value > 2)
    throw std::invalid_argument("invalid application precondition failure kind");
}

const pkgplan::package_path&
application_precondition_failure::path() const noexcept
{
  return path_;
}

application_precondition_field
application_precondition_failure::field() const noexcept
{
  return field_;
}

application_precondition_failure_kind
application_precondition_failure::kind() const noexcept
{
  return kind_;
}

bool
operator<(const application_precondition_failure& lhs,
          const application_precondition_failure& rhs) noexcept
{
  return std::tie(lhs.path_, lhs.field_, lhs.kind_) <
      std::tie(rhs.path_, rhs.field_, rhs.kind_);
}

application_precondition_check
application_precondition_check::make(
    const pkgplan::operation_preconditions& expected,
    backend_observation_batch observed)
{
  const auto& paths = expected.paths();
  if (paths.size() != observed.requested().size())
    throw std::invalid_argument(
        "fresh observation request does not match plan path universe");
  for (std::size_t index = 0; index < paths.size(); ++index) {
    if (paths[index].path() != observed.requested()[index])
      throw std::invalid_argument(
          "fresh observation request path differs from plan precondition");
  }

  std::vector<application_precondition_failure> failures;
  for (const auto& path : paths) {
    const application_path_observation* current = observed.find(path.path());
    if (current == nullptr)
      throw std::logic_error("exact backend observation closure lost a path");
    compare_path(path, *current, failures);
  }
  std::sort(failures.begin(), failures.end());
  return application_precondition_check(
      std::move(observed), std::move(failures));
}

application_precondition_check::application_precondition_check(
    backend_observation_batch observations,
    std::vector<application_precondition_failure> failures)
    : observations_(std::move(observations)), failures_(std::move(failures))
{
}

bool
application_precondition_check::satisfied() const noexcept
{
  return failures_.empty();
}

const backend_observation_batch&
application_precondition_check::observations() const noexcept
{
  return observations_;
}

const std::vector<application_precondition_failure>&
application_precondition_check::failures() const noexcept
{
  return failures_;
}

} // namespace pkgapply
