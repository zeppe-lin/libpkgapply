// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/incoming_package.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <utility>

namespace pkgapply {
namespace {

incoming_package_authority_identity identify(
    const pkgbuild::plan_adapter::artifact_projection& projection)
{
  detail::canonical_record record(
      incoming_package_authority_identity::canonical_domain());
  record.append_u16(incoming_package_authority_schema_version);
  record.append_bytes(projection.authority().identity().hex());
  record.append_bytes(projection.candidate().source_identity().hex());
  record.append_bytes(projection.candidate().candidate().identity().string());
  record.append_bytes(projection.artifact().artifact().string());
  record.append_bytes(projection.artifact().manifest().string());
  record.append_bytes(projection.artifact().release().identity().string());
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

struct incoming_package_authority::impl final {
  impl(incoming_package_authority_identity identity_value,
       pkgbuild::plan_adapter::artifact_projection projection_value)
      : identity(std::move(identity_value)),
        projection(std::move(projection_value))
  {
  }

  incoming_package_authority_identity identity;
  pkgbuild::plan_adapter::artifact_projection projection;
};

incoming_package_authority incoming_package_authority::admit(
    pkgbuild::plan_adapter::artifact_projection projection)
{
  auto identity = identify(projection);
  return incoming_package_authority(std::make_shared<const impl>(
      std::move(identity), std::move(projection)));
}

incoming_package_authority::incoming_package_authority(
    std::shared_ptr<const impl> value)
    : impl_(std::move(value))
{
}

incoming_package_authority::incoming_package_authority(
    const incoming_package_authority&) noexcept = default;
incoming_package_authority::incoming_package_authority(
    incoming_package_authority&&) noexcept = default;
incoming_package_authority& incoming_package_authority::operator=(
    const incoming_package_authority&) noexcept = default;
incoming_package_authority& incoming_package_authority::operator=(
    incoming_package_authority&&) noexcept = default;
incoming_package_authority::~incoming_package_authority() = default;

std::uint16_t incoming_package_authority::schema_version() const noexcept
{
  return incoming_package_authority_schema_version;
}

const incoming_package_authority_identity&
incoming_package_authority::identity() const noexcept
{
  return impl_->identity;
}

const pkgbuild::plan_adapter::artifact_projection&
incoming_package_authority::projection() const noexcept
{
  return impl_->projection;
}

const pkgbuild::image_adapter::build_image_authority&
incoming_package_authority::authority() const noexcept
{
  return impl_->projection.authority();
}

const pkgbuild::build_result& incoming_package_authority::build() const noexcept
{
  return authority().build();
}

const pkgimage::inspected_package_image&
incoming_package_authority::image() const noexcept
{
  return authority().image();
}

const pkgplan::candidate_package_fact&
incoming_package_authority::candidate() const noexcept
{
  return impl_->projection.candidate().candidate();
}

const pkgplan::artifact_package_fact&
incoming_package_authority::artifact() const noexcept
{
  return impl_->projection.artifact();
}

} // namespace pkgapply
