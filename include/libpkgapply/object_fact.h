// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <libpkgapply/digest.h>
#include <libpkgplan/package_path.h>

namespace pkgapply {

/*! \brief Whether one field is known, unknown, or structurally inapplicable. */
enum class fact_state : std::uint8_t {
  known = 1,
  unknown = 2,
  not_applicable = 3,
};

/*! \brief One explicitly qualified application fact. */
template<class Value>
class qualified_fact final {
public:
  [[nodiscard]] static qualified_fact known(Value value)
  {
    return qualified_fact(fact_state::known, std::move(value));
  }

  [[nodiscard]] static qualified_fact unknown()
  {
    return qualified_fact(fact_state::unknown, std::nullopt);
  }

  [[nodiscard]] static qualified_fact not_applicable()
  {
    return qualified_fact(fact_state::not_applicable, std::nullopt);
  }

  [[nodiscard]] fact_state state() const noexcept
  {
    return state_;
  }

  [[nodiscard]] const std::optional<Value>& value() const noexcept
  {
    return value_;
  }

  friend bool operator==(const qualified_fact& lhs,
                         const qualified_fact& rhs) noexcept
  {
    return lhs.state_ == rhs.state_ && lhs.value_ == rhs.value_;
  }

  friend bool operator!=(const qualified_fact& lhs,
                         const qualified_fact& rhs) noexcept
  {
    return !(lhs == rhs);
  }

private:
  qualified_fact(fact_state state, std::optional<Value> value)
      : state_(state), value_(std::move(value))
  {
  }

  fact_state state_;
  std::optional<Value> value_;
};

/*! \brief Semantic object class established by application or observation. */
enum class completed_object_kind : std::uint8_t {
  regular = 1,
  directory = 2,
  symlink = 3,
  fifo = 4,
  character_device = 5,
  block_device = 6,
  socket = 7,
  other = 8,
};

/*! \brief Seconds and normalized nanoseconds established for one object. */
struct completed_object_timestamp final {
  std::int64_t seconds;
  std::uint32_t nanoseconds;
};

[[nodiscard]] bool operator==(const completed_object_timestamp& lhs,
                              const completed_object_timestamp& rhs) noexcept;
[[nodiscard]] bool operator!=(const completed_object_timestamp& lhs,
                              const completed_object_timestamp& rhs) noexcept;

/*! \brief Device identifiers established for one special object. */
struct completed_device_number final {
  std::uint64_t major;
  std::uint64_t minor;
};

[[nodiscard]] bool operator==(const completed_device_number& lhs,
                              const completed_device_number& rhs) noexcept;
[[nodiscard]] bool operator!=(const completed_device_number& lhs,
                              const completed_device_number& rhs) noexcept;

/*! \brief Established relation from one regular path to its hard-link anchor. */
class completed_hardlink_relation final {
public:
  explicit completed_hardlink_relation(pkgplan::package_path anchor);
  [[nodiscard]] const pkgplan::package_path& anchor() const noexcept;

  friend bool operator==(const completed_hardlink_relation& lhs,
                         const completed_hardlink_relation& rhs) noexcept;
  friend bool operator!=(const completed_hardlink_relation& lhs,
                         const completed_hardlink_relation& rhs) noexcept;

private:
  pkgplan::package_path anchor_;
};

/*! \brief Authority that established one completed object fact. */
enum class object_fact_provenance : std::uint8_t {
  incoming_image = 1,
  planning_observation = 2,
  application_observation = 3,
  recovery_capture = 4,
  rejected_capture = 5,
};

/*! \brief Whether every required field was established. */
enum class object_fact_completeness : std::uint8_t {
  complete = 1,
  partial = 2,
};

/*! \brief Rich completed object evidence independent of installed state. */
class completed_object_fact final {
public:
  completed_object_fact(
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
      object_fact_completeness completeness);

  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  [[nodiscard]] completed_object_kind kind() const noexcept;
  [[nodiscard]] const qualified_fact<std::uint32_t>& mode() const noexcept;
  [[nodiscard]] const qualified_fact<std::uint64_t>& uid() const noexcept;
  [[nodiscard]] const qualified_fact<std::uint64_t>& gid() const noexcept;
  [[nodiscard]] const qualified_fact<std::uint64_t>& size() const noexcept;
  [[nodiscard]] const qualified_fact<completed_object_timestamp>&
  mtime() const noexcept;
  [[nodiscard]] const qualified_fact<completed_regular_content_identity>&
  regular_content() const noexcept;
  [[nodiscard]] const qualified_fact<std::string>&
  symlink_target() const noexcept;
  [[nodiscard]] const qualified_fact<completed_device_number>&
  device() const noexcept;
  [[nodiscard]] const qualified_fact<completed_hardlink_relation>&
  hardlink() const noexcept;
  [[nodiscard]] object_fact_provenance provenance() const noexcept;
  [[nodiscard]] object_fact_completeness completeness() const noexcept;

  friend bool operator==(const completed_object_fact& lhs,
                         const completed_object_fact& rhs) noexcept;
  friend bool operator!=(const completed_object_fact& lhs,
                         const completed_object_fact& rhs) noexcept;

private:
  pkgplan::package_path path_;
  completed_object_kind kind_;
  qualified_fact<std::uint32_t> mode_;
  qualified_fact<std::uint64_t> uid_;
  qualified_fact<std::uint64_t> gid_;
  qualified_fact<std::uint64_t> size_;
  qualified_fact<completed_object_timestamp> mtime_;
  qualified_fact<completed_regular_content_identity> regular_content_;
  qualified_fact<std::string> symlink_target_;
  qualified_fact<completed_device_number> device_;
  qualified_fact<completed_hardlink_relation> hardlink_;
  object_fact_provenance provenance_;
  object_fact_completeness completeness_;
};

} // namespace pkgapply
