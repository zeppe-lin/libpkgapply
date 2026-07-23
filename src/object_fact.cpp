// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/object_fact.h>

#include <stdexcept>
#include <tuple>
#include <utility>

namespace pkgapply {
namespace {

template<class Value>
bool
is_not_applicable(const qualified_fact<Value>& value)
{
  return value.state() == fact_state::not_applicable;
}

template<class Value>
bool
is_applicable(const qualified_fact<Value>& value)
{
  return !is_not_applicable(value);
}

void
validate_kind_fields(
    completed_object_kind kind,
    const qualified_fact<std::uint64_t>& size,
    const qualified_fact<completed_regular_content_identity>& regular_content,
    const qualified_fact<std::string>& symlink_target,
    const qualified_fact<completed_device_number>& device,
    const qualified_fact<completed_hardlink_relation>& hardlink)
{
  const bool regular = kind == completed_object_kind::regular;
  const bool symlink = kind == completed_object_kind::symlink;
  const bool device_object =
      kind == completed_object_kind::character_device ||
      kind == completed_object_kind::block_device;

  if (regular != is_applicable(size) ||
      regular != is_applicable(regular_content) ||
      regular != is_applicable(hardlink))
  {
    throw std::invalid_argument(
        "regular size, content, and hard-link facts have invalid applicability");
  }

  if (symlink != is_applicable(symlink_target))
    throw std::invalid_argument("symlink target has invalid applicability");

  if (device_object != is_applicable(device))
    throw std::invalid_argument("device number has invalid applicability");
}

} // namespace

bool
operator==(const completed_object_timestamp& lhs,
           const completed_object_timestamp& rhs) noexcept
{
  return lhs.seconds == rhs.seconds && lhs.nanoseconds == rhs.nanoseconds;
}

bool
operator!=(const completed_object_timestamp& lhs,
           const completed_object_timestamp& rhs) noexcept
{
  return !(lhs == rhs);
}

bool
operator==(const completed_device_number& lhs,
           const completed_device_number& rhs) noexcept
{
  return lhs.major == rhs.major && lhs.minor == rhs.minor;
}

bool
operator!=(const completed_device_number& lhs,
           const completed_device_number& rhs) noexcept
{
  return !(lhs == rhs);
}

completed_hardlink_relation::completed_hardlink_relation(
    pkgplan::package_path anchor)
    : anchor_(std::move(anchor))
{
}

const pkgplan::package_path&
completed_hardlink_relation::anchor() const noexcept
{
  return anchor_;
}

bool
operator==(const completed_hardlink_relation& lhs,
           const completed_hardlink_relation& rhs) noexcept
{
  return lhs.anchor_ == rhs.anchor_;
}

bool
operator!=(const completed_hardlink_relation& lhs,
           const completed_hardlink_relation& rhs) noexcept
{
  return !(lhs == rhs);
}

completed_object_fact::completed_object_fact(
    pkgplan::package_path path,
    completed_object_kind kind,
    qualified_fact<std::uint32_t> mode,
    qualified_fact<std::uint64_t> uid,
    qualified_fact<std::uint64_t> gid,
    qualified_fact<std::uint64_t> size,
    qualified_fact<completed_object_timestamp> mtime,
    qualified_fact<completed_regular_content_identity> regular_content,
    qualified_fact<std::string> symlink_target,
    qualified_fact<completed_device_number> device,
    qualified_fact<completed_hardlink_relation> hardlink,
    object_fact_provenance provenance,
    object_fact_completeness completeness)
    : path_(std::move(path)),
      kind_(kind),
      mode_(std::move(mode)),
      uid_(std::move(uid)),
      gid_(std::move(gid)),
      size_(std::move(size)),
      mtime_(std::move(mtime)),
      regular_content_(std::move(regular_content)),
      symlink_target_(std::move(symlink_target)),
      device_(std::move(device)),
      hardlink_(std::move(hardlink)),
      provenance_(provenance),
      completeness_(completeness)
{
  if (mtime_.state() == fact_state::known &&
      mtime_.value()->nanoseconds >= 1000000000U)
  {
    throw std::invalid_argument("completed object nanoseconds are out of range");
  }
  validate_kind_fields(kind_,
                       size_,
                       regular_content_,
                       symlink_target_,
                       device_,
                       hardlink_);
}

const pkgplan::package_path& completed_object_fact::path() const noexcept
{ return path_; }
completed_object_kind completed_object_fact::kind() const noexcept
{ return kind_; }
const qualified_fact<std::uint32_t>& completed_object_fact::mode() const noexcept
{ return mode_; }
const qualified_fact<std::uint64_t>& completed_object_fact::uid() const noexcept
{ return uid_; }
const qualified_fact<std::uint64_t>& completed_object_fact::gid() const noexcept
{ return gid_; }
const qualified_fact<std::uint64_t>& completed_object_fact::size() const noexcept
{ return size_; }
const qualified_fact<completed_object_timestamp>&
completed_object_fact::mtime() const noexcept
{ return mtime_; }
const qualified_fact<completed_regular_content_identity>&
completed_object_fact::regular_content() const noexcept
{ return regular_content_; }
const qualified_fact<std::string>&
completed_object_fact::symlink_target() const noexcept
{ return symlink_target_; }
const qualified_fact<completed_device_number>&
completed_object_fact::device() const noexcept
{ return device_; }
const qualified_fact<completed_hardlink_relation>&
completed_object_fact::hardlink() const noexcept
{ return hardlink_; }
object_fact_provenance completed_object_fact::provenance() const noexcept
{ return provenance_; }
object_fact_completeness completed_object_fact::completeness() const noexcept
{ return completeness_; }

bool
operator==(const completed_object_fact& lhs,
           const completed_object_fact& rhs) noexcept
{
  return std::tie(lhs.path_, lhs.kind_, lhs.mode_, lhs.uid_, lhs.gid_,
                  lhs.size_, lhs.mtime_, lhs.regular_content_,
                  lhs.symlink_target_, lhs.device_, lhs.hardlink_,
                  lhs.provenance_, lhs.completeness_) ==
         std::tie(rhs.path_, rhs.kind_, rhs.mode_, rhs.uid_, rhs.gid_,
                  rhs.size_, rhs.mtime_, rhs.regular_content_,
                  rhs.symlink_target_, rhs.device_, rhs.hardlink_,
                  rhs.provenance_, rhs.completeness_);
}

bool
operator!=(const completed_object_fact& lhs,
           const completed_object_fact& rhs) noexcept
{
  return !(lhs == rhs);
}

} // namespace pkgapply
