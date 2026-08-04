// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file object_fact.h
 *  \brief Explicit completed filesystem facts and their provenance.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <libpkgapply/digest.h>
#include <libpkgplan/package_path.h>

namespace pkgapply {

/*! \brief Knowledge state of one explicitly qualified fact. */
enum class fact_state : std::uint8_t {
  known = 1, /*!< A concrete value was established. */
  unknown = 2, /*!< The field is applicable but could not be established. */
  not_applicable = 3, /*!< The field is structurally inapplicable. */
};

/*! \brief One value with explicit known, unknown, or inapplicable state.
 *  \tparam Value Semantic value type carried when state() is fact_state::known.
 */
template<class Value>
class qualified_fact final {
public:
  /*! \brief Construct a known fact.
   *  \param value Established semantic value.
   *  \return Fact in the known state.
   */
  [[nodiscard]] static qualified_fact known(Value value)
  {
    return qualified_fact(fact_state::known, std::move(value));
  }

  /*! \brief Construct an applicable fact whose value is unknown. */
  [[nodiscard]] static qualified_fact unknown()
  {
    return qualified_fact(fact_state::unknown, std::nullopt);
  }

  /*! \brief Construct a structurally inapplicable fact. */
  [[nodiscard]] static qualified_fact not_applicable()
  {
    return qualified_fact(fact_state::not_applicable, std::nullopt);
  }

  /*! \brief Return the explicit knowledge state. */
  [[nodiscard]] fact_state state() const noexcept
  {
    return state_;
  }

  /*! \brief Return the value when known, otherwise an empty optional. */
  [[nodiscard]] const std::optional<Value>& value() const noexcept
  {
    return value_;
  }

  /*! \brief Compare complete qualified facts for equality. */
  friend bool operator==(const qualified_fact& lhs,
                         const qualified_fact& rhs) noexcept
  {
    return lhs.state_ == rhs.state_ && lhs.value_ == rhs.value_;
  }

  /*! \brief Compare complete qualified facts for inequality. */
  friend bool operator!=(const qualified_fact& lhs,
                         const qualified_fact& rhs) noexcept
  {
    return !(lhs == rhs);
  }

private:
  /*! \brief Construct one state/value pair owned by the named factories. */
  qualified_fact(fact_state state, std::optional<Value> value)
      : state_(state), value_(std::move(value))
  {
  }

  fact_state state_;
  std::optional<Value> value_;
};

/*! \brief Semantic object class established by application or observation. */
enum class completed_object_kind : std::uint8_t {
  regular = 1, /*!< Regular file. */
  directory = 2, /*!< Directory. */
  symlink = 3, /*!< Symbolic link. */
  fifo = 4, /*!< FIFO. */
  character_device = 5, /*!< Character device. */
  block_device = 6, /*!< Block device. */
  socket = 7, /*!< Socket. */
  other = 8, /*!< Present object outside the represented classes. */
};

/*! \brief Seconds and normalized nanoseconds established for one object. */
struct completed_object_timestamp final {
  std::int64_t seconds; /*!< Whole seconds in the backend's timestamp domain. */
  std::uint32_t nanoseconds; /*!< Nanoseconds in the range 0 through 999999999. */
};

/*! \brief Compare completed timestamps for equality. */
[[nodiscard]] bool operator==(const completed_object_timestamp& lhs,
                              const completed_object_timestamp& rhs) noexcept;
/*! \brief Compare completed timestamps for inequality. */
[[nodiscard]] bool operator!=(const completed_object_timestamp& lhs,
                              const completed_object_timestamp& rhs) noexcept;

/*! \brief Device identifiers established for one special object. */
struct completed_device_number final {
  std::uint64_t major; /*!< Device major number. */
  std::uint64_t minor; /*!< Device minor number. */
};

/*! \brief Compare completed device numbers for equality. */
[[nodiscard]] bool operator==(const completed_device_number& lhs,
                              const completed_device_number& rhs) noexcept;
/*! \brief Compare completed device numbers for inequality. */
[[nodiscard]] bool operator!=(const completed_device_number& lhs,
                              const completed_device_number& rhs) noexcept;

/*! \brief Established relation from one regular path to a hard-link anchor. */
class completed_hardlink_relation final {
public:
  /*! \brief Construct a hard-link relation.
   *  \param anchor Existing regular path anchoring the relation.
   */
  explicit completed_hardlink_relation(pkgplan::package_path anchor);

  /*! \brief Return the anchor path. */
  [[nodiscard]] const pkgplan::package_path& anchor() const noexcept;

  /*! \brief Compare hard-link relations for equality. */
  friend bool operator==(const completed_hardlink_relation& lhs,
                         const completed_hardlink_relation& rhs) noexcept;
  /*! \brief Compare hard-link relations for inequality. */
  friend bool operator!=(const completed_hardlink_relation& lhs,
                         const completed_hardlink_relation& rhs) noexcept;

private:
  pkgplan::package_path anchor_;
};

/*! \brief Authority that established one completed object fact. */
enum class object_fact_provenance : std::uint8_t {
  incoming_image = 1, /*!< Sealed incoming package image. */
  planning_observation = 2, /*!< Planner-admitted target observation. */
  application_observation = 3, /*!< Fresh post-effect backend observation. */
  recovery_capture = 4, /*!< Old-object material captured for recovery. */
  rejected_capture = 5, /*!< Material retained in the rejected-object store. */
};

/*! \brief Whether every object field required by its kind was established. */
enum class object_fact_completeness : std::uint8_t {
  complete = 1, /*!< Every required fact is known. */
  partial = 2, /*!< At least one applicable fact remains unknown. */
};

/*! \brief Completed object evidence independent of installed-state authority. */
class completed_object_fact final {
public:
  /*! \brief Validate and construct one completed object fact.
   *  \param path Exact logical package path.
   *  \param kind Established semantic object kind.
   *  \param mode Qualified permission and type bits.
   *  \param uid Qualified numeric owner.
   *  \param gid Qualified numeric group.
   *  \param size Qualified regular-file size.
   *  \param mtime Qualified modification timestamp.
   *  \param regular_content Qualified decoded regular-content identity.
   *  \param symlink_target Qualified symbolic-link target.
   *  \param device Qualified special-device number.
   *  \param hardlink Qualified hard-link relation.
   *  \param provenance Authority that established the fact.
   *  \param completeness Whether every required field is known.
   *  \throws std::invalid_argument If field applicability contradicts `kind`
   *          or known nanoseconds are outside the normalized range.
   */
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

  /*! \brief Return the exact logical path. */
  [[nodiscard]] const pkgplan::package_path& path() const noexcept;
  /*! \brief Return the established object kind. */
  [[nodiscard]] completed_object_kind kind() const noexcept;
  /*! \brief Return the qualified mode fact. */
  [[nodiscard]] const qualified_fact<std::uint32_t>& mode() const noexcept;
  /*! \brief Return the qualified user-owner fact. */
  [[nodiscard]] const qualified_fact<std::uint64_t>& uid() const noexcept;
  /*! \brief Return the qualified group-owner fact. */
  [[nodiscard]] const qualified_fact<std::uint64_t>& gid() const noexcept;
  /*! \brief Return the qualified regular-file size fact. */
  [[nodiscard]] const qualified_fact<std::uint64_t>& size() const noexcept;
  /*! \brief Return the qualified modification-time fact. */
  [[nodiscard]] const qualified_fact<completed_object_timestamp>&
  mtime() const noexcept;
  /*! \brief Return the qualified regular-content identity fact. */
  [[nodiscard]] const qualified_fact<completed_regular_content_identity>&
  regular_content() const noexcept;
  /*! \brief Return the qualified symbolic-link target fact. */
  [[nodiscard]] const qualified_fact<std::string>&
  symlink_target() const noexcept;
  /*! \brief Return the qualified special-device number fact. */
  [[nodiscard]] const qualified_fact<completed_device_number>&
  device() const noexcept;
  /*! \brief Return the qualified hard-link relation fact. */
  [[nodiscard]] const qualified_fact<completed_hardlink_relation>&
  hardlink() const noexcept;
  /*! \brief Return the authority that established this fact. */
  [[nodiscard]] object_fact_provenance provenance() const noexcept;
  /*! \brief Return whether all required fields were established. */
  [[nodiscard]] object_fact_completeness completeness() const noexcept;

  /*! \brief Compare complete object facts for equality. */
  friend bool operator==(const completed_object_fact& lhs,
                         const completed_object_fact& rhs) noexcept;
  /*! \brief Compare complete object facts for inequality. */
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
