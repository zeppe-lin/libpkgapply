// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/completed_evidence_codec.h>

#include "sha256.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pkgapply {
namespace {

constexpr std::array<std::uint8_t, 8> evidence_magic = {
    'Z', 'L', 'A', 'P', 'E', 'V', 'D', 0,
};
constexpr std::uint64_t maximum_item_count = 1'000'000;
constexpr std::uint64_t maximum_digest_text_size = 128;
constexpr std::uint64_t maximum_text_size =
    maximum_completed_application_evidence_encoding_size;

[[noreturn]] void fail(
    completed_application_evidence_codec_error_code code,
    std::string message)
{
  throw completed_application_evidence_codec_error(
      code, std::move(message));
}

class writer final {
public:
  void append_u8(std::uint8_t value) { bytes_.push_back(value); }

  void append_u16(std::uint16_t value)
  {
    append_u8(static_cast<std::uint8_t>((value >> 8) & 0xffU));
    append_u8(static_cast<std::uint8_t>(value & 0xffU));
  }

  void append_u32(std::uint32_t value)
  {
    for (int shift = 24; shift >= 0; shift -= 8)
      append_u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void append_u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      append_u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void append_i64(std::int64_t value)
  {
    const bool negative = value < 0;
    append_u8(negative ? 1 : 0);
    const std::uint64_t magnitude = negative
        ? static_cast<std::uint64_t>(-(value + 1)) + 1U
        : static_cast<std::uint64_t>(value);
    append_u64(magnitude);
  }

  void append_bytes(const std::uint8_t* data, std::size_t size)
  {
    if (size > maximum_completed_application_evidence_encoding_size -
                   bytes_.size())
    {
      fail(completed_application_evidence_codec_error_code::limit_exceeded,
           "completed-evidence encoding exceeds the size limit");
    }
    bytes_.insert(bytes_.end(), data, data + size);
  }

  void append_string(std::string_view value)
  {
    append_u64(static_cast<std::uint64_t>(value.size()));
    append_bytes(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  [[nodiscard]] completed_application_evidence_encoding finish()
  {
    if (bytes_.size() >
        maximum_completed_application_evidence_encoding_size)
    {
      fail(completed_application_evidence_codec_error_code::limit_exceeded,
           "completed-evidence encoding exceeds the size limit");
    }
    return std::move(bytes_);
  }

private:
  completed_application_evidence_encoding bytes_;
};

class reader final {
public:
  reader(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size)
  {
    if (size > maximum_completed_application_evidence_encoding_size)
      fail(completed_application_evidence_codec_error_code::limit_exceeded,
           "completed-evidence encoding exceeds the size limit");
    if (size != 0 && data == nullptr)
      fail(completed_application_evidence_codec_error_code::truncated,
           "completed-evidence encoding has no storage");
  }

  [[nodiscard]] std::uint8_t read_u8()
  {
    require(1);
    return data_[offset_++];
  }

  [[nodiscard]] std::uint16_t read_u16()
  {
    const auto high = read_u8();
    const auto low = read_u8();
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high) << 8) | low);
  }

  [[nodiscard]] std::uint32_t read_u32()
  {
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index)
      value = (value << 8) | read_u8();
    return value;
  }

  [[nodiscard]] std::uint64_t read_u64()
  {
    std::uint64_t value = 0;
    for (int index = 0; index < 8; ++index)
      value = (value << 8) | read_u8();
    return value;
  }

  [[nodiscard]] std::int64_t read_i64()
  {
    const auto sign = read_u8();
    if (sign > 1)
      fail(completed_application_evidence_codec_error_code::invalid_value,
           "completed-evidence encoding has an invalid integer sign");
    const auto magnitude = read_u64();
    const auto maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (sign == 0) {
      if (magnitude > maximum)
        fail(completed_application_evidence_codec_error_code::invalid_value,
             "completed-evidence integer is out of range");
      return static_cast<std::int64_t>(magnitude);
    }
    if (magnitude > maximum + 1U)
      fail(completed_application_evidence_codec_error_code::invalid_value,
           "completed-evidence integer is out of range");
    if (magnitude == maximum + 1U)
      return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
  }

  [[nodiscard]] bool read_bool()
  {
    const auto value = read_u8();
    if (value > 1)
      fail(completed_application_evidence_codec_error_code::invalid_value,
           "completed-evidence encoding has an invalid boolean");
    return value == 1;
  }

  [[nodiscard]] std::string read_string(std::uint64_t maximum)
  {
    const auto length = read_u64();
    if (length > maximum ||
        length > static_cast<std::uint64_t>(
                     std::numeric_limits<std::size_t>::max()))
    {
      fail(completed_application_evidence_codec_error_code::limit_exceeded,
           "completed-evidence string exceeds its size limit");
    }
    const auto size = static_cast<std::size_t>(length);
    require(size);
    const auto* begin = reinterpret_cast<const char*>(data_ + offset_);
    std::string value(begin, size);
    offset_ += size;
    return value;
  }

  void expect_magic()
  {
    require(evidence_magic.size());
    if (!std::equal(
            evidence_magic.begin(), evidence_magic.end(), data_ + offset_))
    {
      fail(completed_application_evidence_codec_error_code::invalid_magic,
           "completed-evidence encoding has invalid magic");
    }
    offset_ += evidence_magic.size();
  }

  [[nodiscard]] const std::uint8_t* current() const noexcept
  {
    return data_ + offset_;
  }

  [[nodiscard]] std::size_t remaining() const noexcept
  {
    return size_ - offset_;
  }

  void advance(std::size_t count)
  {
    require(count);
    offset_ += count;
  }

  void require_end() const
  {
    if (offset_ != size_)
      fail(completed_application_evidence_codec_error_code::trailing_data,
           "completed-evidence encoding contains trailing data");
  }

private:
  void require(std::size_t count) const
  {
    if (count > size_ - offset_)
      fail(completed_application_evidence_codec_error_code::truncated,
           "completed-evidence encoding is truncated");
  }

  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t offset_ = 0;
};

template<class Identity>
void append_identity(writer& output, const Identity& identity)
{
  output.append_string(identity.string());
}

template<class Identity>
Identity read_identity(reader& input)
{
  try {
    return Identity::parse(input.read_string(maximum_digest_text_size));
  }
  catch (const std::invalid_argument& error) {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         std::string("completed-evidence identity is invalid: ") +
             error.what());
  }
}

std::size_t read_count(reader& input, std::string_view description)
{
  const auto count = input.read_u64();
  if (count > maximum_item_count ||
      count > static_cast<std::uint64_t>(
                  std::numeric_limits<std::size_t>::max()))
  {
    fail(completed_application_evidence_codec_error_code::limit_exceeded,
         std::string(description) + " exceeds its count limit");
  }
  return static_cast<std::size_t>(count);
}

std::uint8_t encode_kind(pkgplan::operation_kind kind)
{
  switch (kind) {
    case pkgplan::operation_kind::install: return 1;
    case pkgplan::operation_kind::upgrade: return 2;
    case pkgplan::operation_kind::remove: return 3;
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed evidence has an invalid operation kind");
}

pkgplan::operation_kind read_kind(reader& input)
{
  switch (input.read_u8()) {
    case 1: return pkgplan::operation_kind::install;
    case 2: return pkgplan::operation_kind::upgrade;
    case 3: return pkgplan::operation_kind::remove;
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed-evidence encoding has an invalid operation kind");
}

std::uint8_t encode_active(pkgplan::planned_active_outcome value)
{
  switch (value) {
    case pkgplan::planned_active_outcome::activate_incoming: return 1;
    case pkgplan::planned_active_outcome::retain_observed: return 2;
    case pkgplan::planned_active_outcome::remove_observed: return 3;
    case pkgplan::planned_active_outcome::remove_directory_if_empty: return 4;
    case pkgplan::planned_active_outcome::remain_absent: return 5;
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed evidence has an invalid planned active outcome");
}

pkgplan::planned_active_outcome read_active(reader& input)
{
  switch (input.read_u8()) {
    case 1: return pkgplan::planned_active_outcome::activate_incoming;
    case 2: return pkgplan::planned_active_outcome::retain_observed;
    case 3: return pkgplan::planned_active_outcome::remove_observed;
    case 4:
      return pkgplan::planned_active_outcome::remove_directory_if_empty;
    case 5: return pkgplan::planned_active_outcome::remain_absent;
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed-evidence encoding has an invalid active outcome");
}

std::uint8_t encode_rejected(pkgplan::planned_rejected_outcome value)
{
  switch (value) {
    case pkgplan::planned_rejected_outcome::none: return 1;
    case pkgplan::planned_rejected_outcome::stage_incoming: return 2;
    case pkgplan::planned_rejected_outcome::stage_old: return 3;
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed evidence has an invalid planned rejected outcome");
}

pkgplan::planned_rejected_outcome read_rejected(reader& input)
{
  switch (input.read_u8()) {
    case 1: return pkgplan::planned_rejected_outcome::none;
    case 2: return pkgplan::planned_rejected_outcome::stage_incoming;
    case 3: return pkgplan::planned_rejected_outcome::stage_old;
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed-evidence encoding has an invalid rejected outcome");
}

template<class Value, class Append>
void append_fact(
    writer& output,
    const qualified_fact<Value>& fact,
    Append append)
{
  output.append_u8(static_cast<std::uint8_t>(fact.state()));
  if (fact.state() == fact_state::known)
    append(*fact.value());
}

template<class Value, class Read>
qualified_fact<Value> read_fact(reader& input, Read read)
{
  switch (input.read_u8()) {
    case static_cast<std::uint8_t>(fact_state::known):
      return qualified_fact<Value>::known(read());
    case static_cast<std::uint8_t>(fact_state::unknown):
      return qualified_fact<Value>::unknown();
    case static_cast<std::uint8_t>(fact_state::not_applicable):
      return qualified_fact<Value>::not_applicable();
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed-evidence encoding has an invalid fact state");
}

void append_object(writer& output, const completed_object_fact& object)
{
  output.append_string(object.path().string());
  output.append_u8(static_cast<std::uint8_t>(object.kind()));
  append_fact(output, object.mode(), [&](std::uint32_t value) {
    output.append_u32(value);
  });
  append_fact(output, object.uid(), [&](std::uint64_t value) {
    output.append_u64(value);
  });
  append_fact(output, object.gid(), [&](std::uint64_t value) {
    output.append_u64(value);
  });
  append_fact(output, object.size(), [&](std::uint64_t value) {
    output.append_u64(value);
  });
  append_fact(
      output, object.mtime(), [&](const completed_object_timestamp& value) {
        output.append_i64(value.seconds);
        output.append_u32(value.nanoseconds);
      });
  append_fact(
      output, object.regular_content(),
      [&](const completed_regular_content_identity& value) {
        append_identity(output, value);
      });
  append_fact(output, object.symlink_target(), [&](const std::string& value) {
    output.append_string(value);
  });
  append_fact(
      output, object.device(), [&](const completed_device_number& value) {
    output.append_u64(value.major);
    output.append_u64(value.minor);
  });
  append_fact(
      output, object.hardlink(),
      [&](const completed_hardlink_relation& value) {
        output.append_string(value.anchor().string());
      });
  output.append_u8(static_cast<std::uint8_t>(object.provenance()));
  output.append_u8(static_cast<std::uint8_t>(object.completeness()));
}

completed_object_fact read_object(reader& input)
{
  auto path = pkgplan::package_path::parse(
      input.read_string(maximum_text_size));
  const auto kind = input.read_u8();
  if (kind < 1 || kind > 8)
    fail(completed_application_evidence_codec_error_code::invalid_value,
         "completed-evidence encoding has an invalid object kind");
  auto mode = read_fact<std::uint32_t>(input, [&] { return input.read_u32(); });
  auto uid = read_fact<std::uint64_t>(input, [&] { return input.read_u64(); });
  auto gid = read_fact<std::uint64_t>(input, [&] { return input.read_u64(); });
  auto size = read_fact<std::uint64_t>(input, [&] { return input.read_u64(); });
  auto mtime = read_fact<completed_object_timestamp>(input, [&] {
    return completed_object_timestamp{input.read_i64(), input.read_u32()};
  });
  auto regular = read_fact<completed_regular_content_identity>(input, [&] {
    return read_identity<completed_regular_content_identity>(input);
  });
  auto symlink = read_fact<std::string>(input, [&] {
    return input.read_string(maximum_text_size);
  });
  auto device = read_fact<completed_device_number>(input, [&] {
    return completed_device_number{input.read_u64(), input.read_u64()};
  });
  auto hardlink = read_fact<completed_hardlink_relation>(input, [&] {
    return completed_hardlink_relation(
        pkgplan::package_path::parse(input.read_string(maximum_text_size)));
  });
  const auto provenance = input.read_u8();
  const auto completeness = input.read_u8();
  if (provenance < 1 || provenance > 5 ||
      completeness < 1 || completeness > 2)
  {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         "completed-evidence encoding has invalid object qualification");
  }
  return completed_object_fact(
      std::move(path), static_cast<completed_object_kind>(kind),
      std::move(mode), std::move(uid), std::move(gid), std::move(size),
      std::move(mtime), std::move(regular), std::move(symlink),
      std::move(device), std::move(hardlink),
      static_cast<object_fact_provenance>(provenance),
      static_cast<object_fact_completeness>(completeness));
}

void append_observation(
    writer& output,
    const application_path_observation& observation)
{
  output.append_string(observation.path().string());
  output.append_u8(static_cast<std::uint8_t>(observation.state()));
  if (observation.state() == fact_state::known)
    append_object(output, *observation.object());
}

application_path_observation read_observation(reader& input)
{
  auto path = pkgplan::package_path::parse(
      input.read_string(maximum_text_size));
  switch (input.read_u8()) {
    case static_cast<std::uint8_t>(fact_state::known): {
      auto object = read_object(input);
      if (object.path() != path)
        fail(completed_application_evidence_codec_error_code::invalid_value,
             "completed-evidence observation path disagrees with object");
      return application_path_observation::present(std::move(object));
    }
    case static_cast<std::uint8_t>(fact_state::unknown):
      return application_path_observation::unknown(std::move(path));
    case static_cast<std::uint8_t>(fact_state::not_applicable):
      return application_path_observation::absent(std::move(path));
  }
  fail(completed_application_evidence_codec_error_code::invalid_value,
       "completed-evidence encoding has an invalid observation state");
}

void append_owners(
    writer& output,
    const std::vector<pkgplan::installed_package_identity>& owners)
{
  output.append_u64(static_cast<std::uint64_t>(owners.size()));
  for (const auto& owner : owners)
    append_identity(output, owner);
}

std::vector<pkgplan::installed_package_identity> read_owners(reader& input)
{
  const auto count = read_count(input, "completed-evidence owners");
  std::vector<pkgplan::installed_package_identity> owners;
  owners.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
    owners.push_back(read_identity<pkgplan::installed_package_identity>(input));
  return owners;
}

void append_path(writer& output, const application_path_consequence& path)
{
  output.append_string(path.path().string());
  output.append_u8(static_cast<std::uint8_t>(path.role()));
  output.append_u8(encode_active(path.requested_active()));
  output.append_u8(encode_rejected(path.requested_rejected()));
  output.append_u8(path.incoming_entry().has_value() ? 1 : 0);
  if (path.incoming_entry())
    output.append_u64(static_cast<std::uint64_t>(*path.incoming_entry()));
  append_owners(output, path.ownership().before_existing_owners());
  append_owners(output, path.ownership().after_existing_owners());
  output.append_u8(path.ownership().incoming_package_owns_after() ? 1 : 0);
  output.append_u8(static_cast<std::uint8_t>(path.active_status()));
  output.append_u8(static_cast<std::uint8_t>(path.rejected_status()));
  append_observation(output, path.before());
  append_observation(output, path.after());
  output.append_u8(path.rejected_object().has_value() ? 1 : 0);
  if (path.rejected_object())
    append_identity(output, *path.rejected_object());
  output.append_u8(static_cast<std::uint8_t>(path.publication()));
}

application_path_role role_of(pkgplan::installation_path_role role)
{
  switch (role) {
    case pkgplan::installation_path_role::incoming_entry:
      return application_path_role::incoming_entry;
    case pkgplan::installation_path_role::structural_parent:
      return application_path_role::structural_parent;
  }
  fail(completed_application_evidence_codec_error_code::request_mismatch,
       "immutable installation plan has an invalid path role");
}

application_path_role role_of(pkgplan::upgrade_path_role role)
{
  switch (role) {
    case pkgplan::upgrade_path_role::incoming_entry:
      return application_path_role::incoming_entry;
    case pkgplan::upgrade_path_role::obsolete_old_path:
      return application_path_role::obsolete_old_path;
    case pkgplan::upgrade_path_role::structural_parent:
      return application_path_role::structural_parent;
  }
  fail(completed_application_evidence_codec_error_code::request_mismatch,
       "immutable upgrade plan has an invalid path role");
}

application_path_role role_of(const pkgplan::removal_path_decision&)
{
  return application_path_role::installed_owned_path;
}

template<class Decision>
application_path_role decision_role(const Decision& decision)
{
  if constexpr (std::is_same_v<Decision, pkgplan::removal_path_decision>)
    return role_of(decision);
  else
    return role_of(decision.role());
}

template<class Decision>
std::optional<pkgimage::entry_id> decision_entry(const Decision& decision)
{
  if constexpr (std::is_same_v<Decision, pkgplan::removal_path_decision>)
    return std::nullopt;
  else
    return decision.incoming_entry();
}

template<class Decision>
application_path_consequence read_path(reader& input, const Decision& decision)
{
  auto path = pkgplan::package_path::parse(
      input.read_string(maximum_text_size));
  const auto role_value = input.read_u8();
  if (role_value < 1 || role_value > 4)
    fail(completed_application_evidence_codec_error_code::invalid_value,
         "completed-evidence path has an invalid role");
  const auto role = static_cast<application_path_role>(role_value);
  const auto active = read_active(input);
  const auto rejected = read_rejected(input);
  std::optional<pkgimage::entry_id> entry;
  if (input.read_bool()) {
    const auto value = input.read_u64();
    if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<pkgimage::entry_id>::max()))
    {
      fail(completed_application_evidence_codec_error_code::invalid_value,
           "completed-evidence entry identifier is out of range");
    }
    entry = static_cast<pkgimage::entry_id>(value);
  }
  auto before_owners = read_owners(input);
  auto after_owners = read_owners(input);
  const bool incoming_owns = input.read_bool();

  if (path != decision.path() || role != decision_role(decision) ||
      active != decision.active() || rejected != decision.rejected() ||
      entry != decision_entry(decision) ||
      before_owners != decision.ownership().before_existing_owners() ||
      after_owners != decision.ownership().after_existing_owners() ||
      incoming_owns != decision.ownership().incoming_package_owns_after())
  {
    fail(completed_application_evidence_codec_error_code::request_mismatch,
         "completed-evidence path differs from the immutable plan");
  }

  const auto active_status = input.read_u8();
  const auto rejected_status = input.read_u8();
  if (active_status < 1 || active_status > 5 ||
      rejected_status < 1 || rejected_status > 5)
  {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         "completed-evidence path has an invalid effect status");
  }
  auto before = read_observation(input);
  auto after = read_observation(input);
  std::optional<rejected_object_record_identity> rejected_object;
  if (input.read_bool())
    rejected_object = read_identity<rejected_object_record_identity>(input);
  const auto publication = input.read_u8();
  if (publication < 1 || publication > 2)
    fail(completed_application_evidence_codec_error_code::invalid_value,
         "completed-evidence path has an invalid publication status");

  try {
    return application_path_consequence(
        std::move(path), role, active, rejected, entry, decision.ownership(),
        static_cast<application_effect_status>(active_status),
        static_cast<application_effect_status>(rejected_status),
        std::move(before), std::move(after), std::move(rejected_object),
        static_cast<ownership_publication_status>(publication));
  }
  catch (const std::invalid_argument& error) {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         std::string("completed-evidence path is invalid: ") + error.what());
  }
}

void append_durability(
    writer& output,
    const application_durability_profile& durability)
{
  output.append_u64(static_cast<std::uint64_t>(durability.facts().size()));
  for (const auto& fact : durability.facts()) {
    output.append_u8(static_cast<std::uint8_t>(fact.domain()));
    output.append_u8(static_cast<std::uint8_t>(fact.status()));
  }
}

application_durability_profile read_durability(reader& input)
{
  const auto count = read_count(input, "completed-evidence durability facts");
  std::vector<application_durability_fact> facts;
  facts.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const auto domain = input.read_u8();
    const auto status = input.read_u8();
    if (domain < 1 || domain > 6 || status < 1 || status > 5)
      fail(completed_application_evidence_codec_error_code::invalid_value,
           "completed-evidence encoding has invalid durability truth");
    facts.emplace_back(
        static_cast<application_durability_domain>(domain),
        static_cast<application_durability_status>(status));
  }
  try {
    return application_durability_profile(std::move(facts));
  }
  catch (const std::invalid_argument& error) {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         std::string("completed-evidence durability is invalid: ") +
             error.what());
  }
}

void append_backend_evidence(
    writer& output,
    const std::vector<application_backend_evidence_identity>& evidence)
{
  output.append_u64(static_cast<std::uint64_t>(evidence.size()));
  for (const auto& item : evidence)
    append_identity(output, item);
}

std::vector<application_backend_evidence_identity>
read_backend_evidence(reader& input)
{
  const auto count = read_count(input, "completed backend evidence");
  std::vector<application_backend_evidence_identity> result;
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
    result.push_back(
        read_identity<application_backend_evidence_identity>(input));
  return result;
}

void append_body(
    writer& output,
    const completed_application_evidence& evidence)
{
  output.append_u16(evidence.schema_version());
  append_identity(output, evidence.identity());
  output.append_u8(encode_kind(evidence.kind()));
  append_identity(output, evidence.request());
  output.append_string(evidence.plan().string());
  append_identity(output, evidence.attempt());
  append_identity(output, evidence.target());
  append_identity(output, evidence.control());
  append_identity(output, evidence.state_projection());
  append_identity(output, evidence.journal());
  output.append_u64(static_cast<std::uint64_t>(evidence.paths().size()));
  for (const auto& path : evidence.paths())
    append_path(output, path);
  append_durability(output, evidence.durability());
  append_backend_evidence(output, evidence.backend_evidence());
}

template<class Request>
completed_application_evidence read_body(reader& input, const Request& request)
{
  if (input.read_u16() != completed_application_evidence_schema_version)
    fail(completed_application_evidence_codec_error_code::unsupported_version,
         "completed application evidence schema version is unsupported");
  const auto expected =
      read_identity<completed_application_evidence_identity>(input);
  const auto kind = read_kind(input);
  const auto request_identity =
      read_identity<application_request_identity>(input);
  const auto plan = pkgplan::operation_plan_identity::parse(
      input.read_string(maximum_digest_text_size));
  auto attempt = read_identity<application_attempt_identity>(input);
  const auto target = read_identity<application_target_context_identity>(input);
  const auto control =
      read_identity<application_execution_control_identity>(input);
  auto state = read_identity<lease_bound_state_projection_identity>(input);
  auto journal = read_identity<application_journal_identity>(input);

  if (kind != request.plan().kind() || request_identity != request.identity() ||
      plan != request.plan().identity() ||
      target != request.target().identity() ||
      control != request.control().identity())
  {
    fail(completed_application_evidence_codec_error_code::request_mismatch,
         "completed evidence differs from the immutable request");
  }

  const auto path_count = read_count(input, "completed-evidence paths");
  const auto& decisions = request.plan().paths();
  if (path_count != decisions.size())
    fail(completed_application_evidence_codec_error_code::request_mismatch,
         "completed-evidence path universe differs from the immutable plan");
  std::vector<application_path_consequence> paths;
  paths.reserve(path_count);
  for (std::size_t index = 0; index < path_count; ++index)
    paths.push_back(read_path(input, decisions[index]));
  auto durability = read_durability(input);
  auto backend_evidence = read_backend_evidence(input);

  try {
    completed_application_evidence result = [&]() {
      if constexpr (
          std::is_same_v<Request, installation_application_request>) {
        return completed_application_evidence::installation(
            request, std::move(attempt), std::move(state),
            std::move(journal), std::move(paths), std::move(durability),
            std::move(backend_evidence));
      }
      else if constexpr (
          std::is_same_v<Request, upgrade_application_request>) {
        return completed_application_evidence::upgrade(
            request, std::move(attempt), std::move(state),
            std::move(journal), std::move(paths), std::move(durability),
            std::move(backend_evidence));
      }
      else {
        return completed_application_evidence::removal(
            request, std::move(attempt), std::move(state),
            std::move(journal), std::move(paths), std::move(durability),
            std::move(backend_evidence));
      }
    }();
    if (result.identity() != expected)
      fail(completed_application_evidence_codec_error_code::identity_mismatch,
           "completed-evidence identity does not match its content");
    return result;
  }
  catch (const completed_application_evidence_codec_error&) {
    throw;
  }
  catch (const std::invalid_argument& error) {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         std::string("completed evidence is invalid: ") + error.what());
  }
}

template<class Request>
completed_application_evidence decode(
    const std::uint8_t* data,
    std::size_t size,
    const Request& request)
{
  reader input(data, size);
  input.expect_magic();
  if (input.read_u16() != completed_application_evidence_encoding_version)
    fail(completed_application_evidence_codec_error_code::unsupported_version,
         "completed-evidence encoding version is unsupported");
  const auto declared_body_size = input.read_u64();
  if (declared_body_size >
      maximum_completed_application_evidence_encoding_size)
  {
    fail(completed_application_evidence_codec_error_code::limit_exceeded,
         "completed-evidence body exceeds the size limit");
  }
  std::array<std::uint8_t, 32> expected_checksum{};
  for (auto& byte : expected_checksum)
    byte = input.read_u8();
  if (declared_body_size > input.remaining())
    fail(completed_application_evidence_codec_error_code::truncated,
         "completed-evidence body is truncated");
  if (declared_body_size < input.remaining())
    fail(completed_application_evidence_codec_error_code::trailing_data,
         "completed-evidence encoding contains trailing data");
  const auto body_size = static_cast<std::size_t>(declared_body_size);
  const auto actual_checksum = detail::sha256(
      reinterpret_cast<const std::byte*>(input.current()), body_size);
  if (actual_checksum != expected_checksum)
    fail(completed_application_evidence_codec_error_code::checksum_mismatch,
         "completed-evidence checksum does not match its content");
  auto result = read_body(input, request);
  input.require_end();
  return result;
}

template<class Request>
completed_application_evidence decode_checked(
    const std::uint8_t* data,
    std::size_t size,
    const Request& request)
{
  try {
    return decode(data, size, request);
  }
  catch (const completed_application_evidence_codec_error&) {
    throw;
  }
  catch (const std::invalid_argument& error) {
    fail(completed_application_evidence_codec_error_code::invalid_value,
         std::string("completed-evidence encoding is invalid: ") +
             error.what());
  }
}


} // namespace

completed_application_evidence_codec_error::
completed_application_evidence_codec_error(
    completed_application_evidence_codec_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

completed_application_evidence_codec_error::
~completed_application_evidence_codec_error() = default;

completed_application_evidence_codec_error_code
completed_application_evidence_codec_error::code() const noexcept
{
  return code_;
}

completed_application_evidence_encoding
encode_completed_application_evidence(
    const completed_application_evidence& evidence)
{
  writer body;
  append_body(body, evidence);
  auto body_bytes = body.finish();
  const auto checksum = detail::sha256(
      reinterpret_cast<const std::byte*>(body_bytes.data()),
      body_bytes.size());

  writer envelope;
  envelope.append_bytes(evidence_magic.data(), evidence_magic.size());
  envelope.append_u16(completed_application_evidence_encoding_version);
  envelope.append_u64(static_cast<std::uint64_t>(body_bytes.size()));
  envelope.append_bytes(checksum.data(), checksum.size());
  envelope.append_bytes(body_bytes.data(), body_bytes.size());
  return envelope.finish();
}

completed_application_evidence
decode_completed_application_evidence(
    const std::uint8_t* data,
    std::size_t size,
    const installation_application_request& request)
{
  return decode_checked(data, size, request);
}

completed_application_evidence
decode_completed_application_evidence(
    const std::uint8_t* data,
    std::size_t size,
    const upgrade_application_request& request)
{
  return decode_checked(data, size, request);
}

completed_application_evidence
decode_completed_application_evidence(
    const std::uint8_t* data,
    std::size_t size,
    const removal_application_request& request)
{
  return decode_checked(data, size, request);
}

completed_application_evidence
decode_completed_application_evidence(
    const completed_application_evidence_encoding& encoding,
    const installation_application_request& request)
{
  return decode_checked(encoding.data(), encoding.size(), request);
}

completed_application_evidence
decode_completed_application_evidence(
    const completed_application_evidence_encoding& encoding,
    const upgrade_application_request& request)
{
  return decode_checked(encoding.data(), encoding.size(), request);
}

completed_application_evidence
decode_completed_application_evidence(
    const completed_application_evidence_encoding& encoding,
    const removal_application_request& request)
{
  return decode_checked(encoding.data(), encoding.size(), request);
}

} // namespace pkgapply
