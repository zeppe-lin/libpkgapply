// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/incoming_package.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <exception>
#include <optional>
#include <string>
#include <utility>

#include <libpkgsource-plan/adapter.h>

namespace pkgapply {
namespace {

pkgimage::entry_type image_type(pkgbuild::payload_entry_type value)
{
  switch (value) {
    case pkgbuild::payload_entry_type::regular:
      return pkgimage::entry_type::regular;
    case pkgbuild::payload_entry_type::directory:
      return pkgimage::entry_type::directory;
    case pkgbuild::payload_entry_type::symlink:
      return pkgimage::entry_type::symlink;
    case pkgbuild::payload_entry_type::hardlink:
      return pkgimage::entry_type::hardlink;
    case pkgbuild::payload_entry_type::fifo:
      return pkgimage::entry_type::fifo;
    case pkgbuild::payload_entry_type::character_device:
      return pkgimage::entry_type::character_device;
    case pkgbuild::payload_entry_type::block_device:
      return pkgimage::entry_type::block_device;
  }
  throw incoming_package_error(
      incoming_package_error_code::payload_mismatch,
      "unknown build payload entry type");
}

void verify_entry(const pkgbuild::payload_entry& expected,
                  const pkgimage::package_entry& observed)
{
  if (expected.path().string() != observed.path.string() ||
      image_type(expected.type()) != observed.type ||
      expected.mode() != observed.mode || expected.uid() != observed.uid ||
      expected.gid() != observed.gid || expected.size() != observed.size ||
      expected.modification_time().seconds != observed.mtime ||
      expected.modification_time().nanoseconds != observed.mtime_nanoseconds)
  {
    throw incoming_package_error(
        incoming_package_error_code::payload_mismatch,
        "inspected artifact metadata differs from build payload at " +
            expected.path().string());
  }

  if (expected.symlink_target() != observed.symlink_target)
  {
    throw incoming_package_error(
        incoming_package_error_code::payload_mismatch,
        "inspected symbolic-link target differs from build payload at " +
            expected.path().string());
  }

  const std::optional<std::string> expected_hardlink =
      expected.hardlink_target()
          ? std::optional<std::string>(expected.hardlink_target()->string())
          : std::nullopt;
  const std::optional<std::string> observed_hardlink =
      observed.hardlink_target
          ? std::optional<std::string>(observed.hardlink_target->string())
          : std::nullopt;
  if (expected_hardlink != observed_hardlink)
  {
    throw incoming_package_error(
        incoming_package_error_code::payload_mismatch,
        "inspected hard-link target differs from build payload at " +
            expected.path().string());
  }

  const std::optional<pkgbuild::device_number> observed_device =
      observed.device
          ? std::optional<pkgbuild::device_number>(pkgbuild::device_number{
                observed.device->major, observed.device->minor})
          : std::nullopt;
  if (expected.device() != observed_device)
  {
    throw incoming_package_error(
        incoming_package_error_code::payload_mismatch,
        "inspected device number differs from build payload at " +
            expected.path().string());
  }

  const std::optional<std::string> expected_content =
      expected.regular_content()
          ? std::optional<std::string>(
                "v1:sha256:" + expected.regular_content()->hex())
          : std::nullopt;
  const std::optional<std::string> observed_content =
      observed.regular_content
          ? std::optional<std::string>(observed.regular_content->string())
          : std::nullopt;
  if (expected_content != observed_content)
  {
    throw incoming_package_error(
        incoming_package_error_code::payload_mismatch,
        "inspected regular content differs from build payload at " +
            expected.path().string());
  }
}

void verify_payload(const pkgbuild::payload_manifest& expected,
                    const pkgimage::package_image& observed)
{
  if (expected.entries().size() != observed.entries().size())
  {
    throw incoming_package_error(
        incoming_package_error_code::payload_mismatch,
        "inspected artifact entry count differs from build payload");
  }
  for (std::size_t index = 0; index < expected.entries().size(); ++index)
    verify_entry(expected.entries()[index], observed.entries()[index]);
}

incoming_package_authority_identity identify(
    const pkgbuild::build_result& build,
    const pkgimage::inspected_package_image& image,
    const pkgplan::candidate_package_fact& candidate)
{
  detail::canonical_record record(
      incoming_package_authority_identity::canonical_domain());
  record.append_u16(incoming_package_authority_schema_version);
  record.append_bytes(build.identity().hex());
  record.append_bytes(build.request().identity().hex());
  record.append_bytes(build.request().source().identity().hex());
  record.append_bytes(build.payload()->identity().hex());
  record.append_bytes(build.artifact()->identity().hex());
  record.append_bytes(build.artifact()->complete_digest().hex());
  record.append_u64(build.artifact()->byte_count());
  record.append_bytes(build.artifact_binding()->hex());
  record.append_bytes(candidate.identity().string());
  record.append_bytes(candidate.release().identity().string());
  record.append_bytes(image.receipt().archive_digest().string());
  record.append_bytes(image.image().identity().string());
  record.append_bytes(image.receipt().identity().string());
  return detail::identity_factory::from_sha256<
      incoming_package_authority_identity>(record.sha256());
}

} // namespace

incoming_package_error::incoming_package_error(
    incoming_package_error_code code, std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

incoming_package_error::~incoming_package_error() = default;

incoming_package_error_code incoming_package_error::code() const noexcept
{
  return code_;
}

incoming_package_authority incoming_package_authority::admit(
    pkgbuild::build_result build,
    pkgimage::inspected_package_image image)
{
  if (build.outcome() != pkgbuild::build_outcome::succeeded ||
      !build.payload() || !build.artifact() || !build.artifact_binding())
  {
    throw incoming_package_error(
        incoming_package_error_code::build_result,
        "application requires a complete successful native build result");
  }

  const std::string expected_archive =
      "v1:sha256:" + build.artifact()->complete_digest().hex();
  if (image.receipt().archive_digest().string() != expected_archive)
  {
    throw incoming_package_error(
        incoming_package_error_code::artifact_binding,
        "inspected archive digest differs from sealed build artifact");
  }

  verify_payload(*build.payload(), image.image());

  pkgplan::candidate_package_fact candidate = [&] {
    try
    {
      return pkgsource::plan_adapter::project_candidate(
          build.request().source()).candidate();
    }
    catch (const std::exception& error)
    {
      throw incoming_package_error(
          incoming_package_error_code::source_projection,
          std::string("cannot project sealed build source into planner control: ") +
              error.what());
    }
  }();

  incoming_package_authority_identity identity =
      identify(build, image, candidate);
  return incoming_package_authority(
      std::move(identity), std::move(build), std::move(image),
      std::move(candidate));
}

incoming_package_authority::incoming_package_authority(
    incoming_package_authority_identity identity,
    pkgbuild::build_result build,
    pkgimage::inspected_package_image image,
    pkgplan::candidate_package_fact candidate)
    : identity_(std::move(identity)), build_(std::move(build)),
      image_(std::move(image)), candidate_(std::move(candidate))
{
}

std::uint16_t incoming_package_authority::schema_version() const noexcept
{
  return schema_version_;
}

const incoming_package_authority_identity&
incoming_package_authority::identity() const noexcept
{
  return identity_;
}

const pkgbuild::build_result& incoming_package_authority::build() const noexcept
{
  return build_;
}

const pkgimage::inspected_package_image&
incoming_package_authority::image() const noexcept
{
  return image_;
}

const pkgplan::candidate_package_fact&
incoming_package_authority::candidate() const noexcept
{
  return candidate_;
}

} // namespace pkgapply
