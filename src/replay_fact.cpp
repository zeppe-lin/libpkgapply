// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "replay_fact.h"

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

inline constexpr std::size_t maximum_replay_fact_encoding_size =
    256U * 1024U * 1024U;

enum class replay_codec_error_code : std::uint8_t {
  invalid_magic = 1,
  unsupported_version = 2,
  truncated = 3,
  invalid_value = 4,
  limit_exceeded = 5,
  trailing_data = 6,
  identity_mismatch = 7,
  request_mismatch = 8,
};

class replay_codec_error final : public std::invalid_argument {
public:
  replay_codec_error(replay_codec_error_code, std::string message)
      : std::invalid_argument(std::move(message))
  {
  }
};

constexpr std::uint64_t maximum_item_count = 1'000'000;
constexpr std::uint64_t maximum_digest_text_size = 128;
constexpr std::uint64_t maximum_text_size =
    maximum_replay_fact_encoding_size;

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
    std::uint64_t magnitude = 0;
    if (negative)
      magnitude = static_cast<std::uint64_t>(-(value + 1)) + 1U;
    else
      magnitude = static_cast<std::uint64_t>(value);
    append_u64(magnitude);
  }
  void append_bytes(const std::uint8_t* data, std::size_t size)
  {
    if (size > maximum_replay_fact_encoding_size -
                   bytes_.size())
    {
      throw replay_codec_error(
          replay_codec_error_code::limit_exceeded,
          "application replay fact encoding exceeds the size limit");
    }
    bytes_.insert(bytes_.end(), data, data + size);
  }
  void append_string(std::string_view value)
  {
    append_u64(static_cast<std::uint64_t>(value.size()));
    append_bytes(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }
  [[nodiscard]] std::vector<std::uint8_t> finish()
  {
    if (bytes_.size() > maximum_replay_fact_encoding_size)
      throw replay_codec_error(
          replay_codec_error_code::limit_exceeded,
          "application replay fact encoding exceeds the size limit");
    return std::move(bytes_);
  }
private:
  std::vector<std::uint8_t> bytes_;
};

class reader final {
public:
  reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size)
  {
    if (size > maximum_replay_fact_encoding_size)
      fail(replay_codec_error_code::limit_exceeded,
           "application replay fact encoding exceeds the size limit");
    if (size != 0 && data == nullptr)
      fail(replay_codec_error_code::truncated,
           "application replay fact encoding has no storage");
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
      fail(replay_codec_error_code::invalid_value,
           "application replay fact contains an invalid integer sign");
    const auto magnitude = read_u64();
    const auto maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (sign == 0) {
      if (magnitude > maximum)
        fail(replay_codec_error_code::invalid_value,
             "application replay fact integer is out of range");
      return static_cast<std::int64_t>(magnitude);
    }
    if (magnitude > maximum + 1U)
      fail(replay_codec_error_code::invalid_value,
           "application replay fact integer is out of range");
    if (magnitude == maximum + 1U)
      return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
  }
  [[nodiscard]] bool read_bool()
  {
    const auto value = read_u8();
    if (value > 1)
      fail(replay_codec_error_code::invalid_value,
           "application replay fact contains an invalid boolean");
    return value == 1;
  }
  [[nodiscard]] std::string read_string(std::uint64_t maximum)
  {
    const auto length = read_u64();
    if (length > maximum || length >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
      fail(replay_codec_error_code::limit_exceeded,
           "application replay fact string exceeds its size limit");
    }
    const auto size = static_cast<std::size_t>(length);
    require(size);
    const auto* begin = reinterpret_cast<const char*>(data_ + offset_);
    std::string value(begin, size);
    offset_ += size;
    return value;
  }
  void require_end() const
  {
    if (offset_ != size_)
      fail(replay_codec_error_code::trailing_data,
           "application replay fact encoding contains trailing data");
  }
private:
  [[noreturn]] static void fail(
      replay_codec_error_code code,
      std::string message)
  {
    throw replay_codec_error(code, std::move(message));
  }
  void require(std::size_t count) const
  {
    if (count > size_ - offset_)
      fail(replay_codec_error_code::truncated,
           "application replay fact encoding is truncated");
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
  return Identity::parse(input.read_string(maximum_digest_text_size));
}

std::size_t read_count(reader& input, std::string_view description)
{
  const auto count = input.read_u64();
  if (count > maximum_item_count || count >
          static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
  {
    throw replay_codec_error(
        replay_codec_error_code::limit_exceeded,
        std::string(description) + " exceeds its count limit");
  }
  return static_cast<std::size_t>(count);
}

void append_evidence(
    writer& output,
    const std::vector<application_backend_evidence_identity>& evidence)
{
  output.append_u64(static_cast<std::uint64_t>(evidence.size()));
  for (const auto& item : evidence)
    append_identity(output, item);
}

std::vector<application_backend_evidence_identity> read_evidence(reader& input)
{
  const auto count = read_count(input, "backend evidence");
  std::vector<application_backend_evidence_identity> result;
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
    result.push_back(read_identity<application_backend_evidence_identity>(input));
  return result;
}

std::uint8_t encode_outcome(backend_operation_outcome outcome)
{
  switch (outcome) {
    case backend_operation_outcome::completed: return 1;
    case backend_operation_outcome::conditional_retained: return 2;
    case backend_operation_outcome::failed: return 3;
    case backend_operation_outcome::indeterminate: return 4;
  }
  throw replay_codec_error(
      replay_codec_error_code::invalid_value,
      "application replay fact contains an invalid backend outcome");
}

backend_operation_outcome read_outcome(reader& input)
{
  switch (input.read_u8()) {
    case 1: return backend_operation_outcome::completed;
    case 2: return backend_operation_outcome::conditional_retained;
    case 3: return backend_operation_outcome::failed;
    case 4: return backend_operation_outcome::indeterminate;
  }
  throw replay_codec_error(
      replay_codec_error_code::invalid_value,
      "application replay fact contains an invalid backend outcome");
}

void append_operation_result(writer& output, const backend_operation_result& value)
{
  output.append_u8(encode_outcome(value.outcome()));
  append_evidence(output, value.evidence());
}

backend_operation_result read_operation_result(reader& input)
{
  const auto outcome = read_outcome(input);
  return backend_operation_result(outcome, read_evidence(input));
}

template<class Value, class Append>
void append_fact(writer& output, const qualified_fact<Value>& fact, Append append)
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
  throw replay_codec_error(
      replay_codec_error_code::invalid_value,
      "application replay fact contains an invalid fact state");
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
  append_fact(output, object.mtime(), [&](const completed_object_timestamp& value) {
    output.append_i64(value.seconds);
    output.append_u32(value.nanoseconds);
  });
  append_fact(output, object.regular_content(),
              [&](const completed_regular_content_identity& value) {
                append_identity(output, value);
              });
  append_fact(output, object.symlink_target(), [&](const std::string& value) {
    output.append_string(value);
  });
  append_fact(output, object.device(), [&](const completed_device_number& value) {
    output.append_u64(value.major);
    output.append_u64(value.minor);
  });
  append_fact(output, object.hardlink(),
              [&](const completed_hardlink_relation& value) {
                output.append_string(value.anchor().string());
              });
  output.append_u8(static_cast<std::uint8_t>(object.provenance()));
  output.append_u8(static_cast<std::uint8_t>(object.completeness()));
}

completed_object_fact read_object(reader& input)
{
  auto path = pkgplan::package_path::parse(input.read_string(maximum_text_size));
  const auto kind_value = input.read_u8();
  if (kind_value < 1 || kind_value > 8)
    throw replay_codec_error(
        replay_codec_error_code::invalid_value,
        "application replay fact contains an invalid object kind");
  const auto mode = read_fact<std::uint32_t>(input, [&] { return input.read_u32(); });
  const auto uid = read_fact<std::uint64_t>(input, [&] { return input.read_u64(); });
  const auto gid = read_fact<std::uint64_t>(input, [&] { return input.read_u64(); });
  const auto size = read_fact<std::uint64_t>(input, [&] { return input.read_u64(); });
  const auto mtime = read_fact<completed_object_timestamp>(input, [&] {
    return completed_object_timestamp{input.read_i64(), input.read_u32()};
  });
  const auto regular = read_fact<completed_regular_content_identity>(input, [&] {
    return read_identity<completed_regular_content_identity>(input);
  });
  const auto symlink = read_fact<std::string>(input, [&] {
    return input.read_string(maximum_text_size);
  });
  const auto device = read_fact<completed_device_number>(input, [&] {
    return completed_device_number{input.read_u64(), input.read_u64()};
  });
  const auto hardlink = read_fact<completed_hardlink_relation>(input, [&] {
    return completed_hardlink_relation(
        pkgplan::package_path::parse(input.read_string(maximum_text_size)));
  });
  const auto provenance_value = input.read_u8();
  if (provenance_value < 1 || provenance_value > 5)
    throw replay_codec_error(
        replay_codec_error_code::invalid_value,
        "application replay fact contains invalid object provenance");
  const auto completeness_value = input.read_u8();
  if (completeness_value < 1 || completeness_value > 2)
    throw replay_codec_error(
        replay_codec_error_code::invalid_value,
        "application replay fact contains invalid object completeness");
  return completed_object_fact(
      std::move(path), static_cast<completed_object_kind>(kind_value), mode, uid,
      gid, size, mtime, regular, symlink, device, hardlink,
      static_cast<object_fact_provenance>(provenance_value),
      static_cast<object_fact_completeness>(completeness_value));
}

void append_observation(writer& output, const application_path_observation& value)
{
  output.append_string(value.path().string());
  output.append_u8(static_cast<std::uint8_t>(value.state()));
  if (value.state() == fact_state::known)
    append_object(output, *value.object());
}

application_path_observation read_observation(reader& input)
{
  auto path = pkgplan::package_path::parse(input.read_string(maximum_text_size));
  switch (input.read_u8()) {
    case static_cast<std::uint8_t>(fact_state::known): {
      auto object = read_object(input);
      if (object.path() != path)
        throw replay_codec_error(
            replay_codec_error_code::invalid_value,
            "application replay fact observation path disagrees with object");
      return application_path_observation::present(std::move(object));
    }
    case static_cast<std::uint8_t>(fact_state::unknown):
      return application_path_observation::unknown(std::move(path));
    case static_cast<std::uint8_t>(fact_state::not_applicable):
      return application_path_observation::absent(std::move(path));
  }
  throw replay_codec_error(
      replay_codec_error_code::invalid_value,
      "application replay fact contains an invalid observation state");
}

void append_observation_batch(writer& output, const backend_observation_batch& batch)
{
  output.append_u64(static_cast<std::uint64_t>(batch.requested().size()));
  for (const auto& path : batch.requested())
    output.append_string(path.string());
  output.append_u64(static_cast<std::uint64_t>(batch.observations().size()));
  for (const auto& observation : batch.observations())
    append_observation(output, observation);
  append_evidence(output, batch.evidence());
}

backend_observation_batch read_observation_batch(reader& input)
{
  const auto requested_count = read_count(input, "observation request paths");
  std::vector<pkgplan::package_path> requested;
  requested.reserve(requested_count);
  for (std::size_t index = 0; index < requested_count; ++index)
    requested.push_back(pkgplan::package_path::parse(
        input.read_string(maximum_text_size)));
  const auto observation_count = read_count(input, "observations");
  std::vector<application_path_observation> observations;
  observations.reserve(observation_count);
  for (std::size_t index = 0; index < observation_count; ++index)
    observations.push_back(read_observation(input));
  return backend_observation_batch::make(
      std::move(requested), std::move(observations), read_evidence(input));
}

void append_durability_fact(writer& output, const application_durability_fact& fact)
{
  output.append_u8(static_cast<std::uint8_t>(fact.domain()));
  output.append_u8(static_cast<std::uint8_t>(fact.status()));
}

application_durability_fact read_durability_fact(reader& input)
{
  const auto domain = input.read_u8();
  const auto status = input.read_u8();
  if (domain < 1 || domain > 6 || status < 1 || status > 5)
    throw replay_codec_error(
        replay_codec_error_code::invalid_value,
        "application replay fact contains invalid durability truth");
  return application_durability_fact(
      static_cast<application_durability_domain>(domain),
      static_cast<application_durability_status>(status));
}

void append_durability(writer& output, const application_durability_profile& value)
{
  output.append_u64(static_cast<std::uint64_t>(value.facts().size()));
  for (const auto& fact : value.facts())
    append_durability_fact(output, fact);
}

application_durability_profile read_durability(reader& input)
{
  const auto count = read_count(input, "durability facts");
  std::vector<application_durability_fact> facts;
  facts.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
    facts.push_back(read_durability_fact(input));
  return application_durability_profile(std::move(facts));
}

void append_path_dynamic(writer& output, const application_path_consequence& path)
{
  output.append_string(path.path().string());
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
  throw std::invalid_argument("invalid installation path role");
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
  throw std::invalid_argument("invalid upgrade path role");
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
application_path_consequence read_path_dynamic(
    reader& input,
    const Decision& decision)
{
  auto path = pkgplan::package_path::parse(input.read_string(maximum_text_size));
  if (path != decision.path())
    throw replay_codec_error(
        replay_codec_error_code::request_mismatch,
        "completed evidence path order differs from the immutable plan");
  const auto active = input.read_u8();
  const auto rejected = input.read_u8();
  const auto publication = [&]() {
    auto before = read_observation(input);
    auto after = read_observation(input);
    std::optional<rejected_object_record_identity> rejected_object;
    if (input.read_bool())
      rejected_object = read_identity<rejected_object_record_identity>(input);
    const auto publication_value = input.read_u8();
    if (active < 1 || active > 5 || rejected < 1 || rejected > 5 ||
        publication_value < 1 || publication_value > 2)
    {
      throw replay_codec_error(
          replay_codec_error_code::invalid_value,
          "completed evidence path contains invalid status values");
    }
    return application_path_consequence(
        std::move(path), decision_role(decision), decision.active(),
        decision.rejected(), decision_entry(decision), decision.ownership(),
        static_cast<application_effect_status>(active),
        static_cast<application_effect_status>(rejected), std::move(before),
        std::move(after), std::move(rejected_object),
        static_cast<ownership_publication_status>(publication_value));
  }();
  return publication;
}

void append_completed_evidence(
    writer& output,
    const completed_application_evidence& evidence)
{
  append_identity(output, evidence.identity());
  append_identity(output, evidence.attempt());
  append_identity(output, evidence.state_projection());
  append_identity(output, evidence.journal());
  output.append_u64(static_cast<std::uint64_t>(evidence.paths().size()));
  for (const auto& path : evidence.paths())
    append_path_dynamic(output, path);
  append_durability(output, evidence.durability());
  append_evidence(output, evidence.backend_evidence());
}

template<class Request>
completed_application_evidence read_completed_evidence(
    reader& input,
    const Request& request)
{
  const auto expected =
      read_identity<completed_application_evidence_identity>(input);
  auto attempt = read_identity<application_attempt_identity>(input);
  auto state = read_identity<lease_bound_state_projection_identity>(input);
  auto journal = read_identity<application_journal_identity>(input);
  const auto path_count = read_count(input, "completed evidence paths");
  const auto& decisions = request.plan().paths();
  if (path_count != decisions.size())
    throw replay_codec_error(
        replay_codec_error_code::request_mismatch,
        "completed evidence path universe differs from the immutable plan");
  std::vector<application_path_consequence> paths;
  paths.reserve(path_count);
  for (std::size_t index = 0; index < path_count; ++index)
    paths.push_back(read_path_dynamic(input, decisions[index]));
  auto durability = read_durability(input);
  auto evidence = read_evidence(input);

  completed_application_evidence result = [&]() {
    if constexpr (std::is_same_v<Request, installation_application_request>) {
      return completed_application_evidence::installation(
          request, std::move(attempt), std::move(state), std::move(journal),
          std::move(paths), std::move(durability), std::move(evidence));
    }
    else if constexpr (std::is_same_v<Request, upgrade_application_request>) {
      return completed_application_evidence::upgrade(
          request, std::move(attempt), std::move(state), std::move(journal),
          std::move(paths), std::move(durability), std::move(evidence));
    }
    else {
      return completed_application_evidence::removal(
          request, std::move(attempt), std::move(state), std::move(journal),
          std::move(paths), std::move(durability), std::move(evidence));
    }
  }();
  if (result.identity() != expected)
    throw replay_codec_error(
        replay_codec_error_code::identity_mismatch,
        "completed evidence identity does not match replay fact content");
  return result;
}

constexpr std::array<std::uint8_t, 8> replay_fact_magic = {
    'Z', 'L', 'A', 'P', 'F', 'A', 'C', 0,
};
constexpr std::uint16_t replay_fact_version = 1;

enum class replay_fact_tag : std::uint8_t {
  seed = 1,
  incoming_payload = 2,
  capture = 3,
  rejected = 4,
  active = 5,
  recovery = 6,
  synchronization = 7,
  completed_evidence = 8,
};

writer replay_writer(replay_fact_tag tag)
{
  writer output;
  for (const auto byte : replay_fact_magic)
    output.append_u8(byte);
  output.append_u16(replay_fact_version);
  output.append_u8(static_cast<std::uint8_t>(tag));
  return output;
}

replay_fact_tag replay_reader_header(reader& input)
{
  for (const auto byte : replay_fact_magic) {
    if (input.read_u8() != byte)
      throw replay_codec_error(
          replay_codec_error_code::invalid_magic,
          "application replay fact encoding has invalid magic");
  }
  if (input.read_u16() != replay_fact_version)
    throw replay_codec_error(
        replay_codec_error_code::unsupported_version,
        "application replay fact encoding version is unsupported");
  const auto tag = input.read_u8();
  if (tag < static_cast<std::uint8_t>(replay_fact_tag::seed) ||
      tag > static_cast<std::uint8_t>(replay_fact_tag::completed_evidence))
  {
    throw replay_codec_error(
        replay_codec_error_code::invalid_value,
        "application replay fact encoding contains an invalid tag");
  }
  return static_cast<replay_fact_tag>(tag);
}

template<class Request>
detail::application_replay_fact decode_fact(
    const application_journal_replay_encoding& encoding,
    const Request& request)
{
  reader input(
      reinterpret_cast<const std::uint8_t*>(encoding.data()), encoding.size());
  const auto tag = replay_reader_header(input);
  switch (tag) {
    case replay_fact_tag::incoming_payload: {
      auto value = read_operation_result(input);
      input.require_end();
      return detail::application_replay_fact(std::move(value));
    }
    case replay_fact_tag::capture: {
      const auto outcome = read_outcome(input);
      auto observation = read_observation(input);
      const bool exact = input.read_bool();
      auto value = application_restart_capture(old_object_capture_result(
          outcome, std::move(observation), exact, read_evidence(input)));
      input.require_end();
      return detail::application_replay_fact(std::move(value));
    }
    case replay_fact_tag::rejected: {
      auto path = pkgplan::package_path::parse(
          input.read_string(maximum_text_size));
      const auto outcome = read_outcome(input);
      std::optional<rejected_object_record_identity> record;
      if (input.read_bool())
        record = read_identity<rejected_object_record_identity>(input);
      auto value = application_restart_rejected_effect(
          std::move(path), rejected_object_publication_result(
              outcome, std::move(record), read_evidence(input)));
      input.require_end();
      return detail::application_replay_fact(std::move(value));
    }
    case replay_fact_tag::active:
    case replay_fact_tag::recovery: {
      auto path = pkgplan::package_path::parse(
          input.read_string(maximum_text_size));
      auto result = read_operation_result(input);
      input.require_end();
      if (tag == replay_fact_tag::active)
        return detail::application_replay_fact(
            application_restart_active_effect(std::move(path), std::move(result)));
      return detail::application_replay_fact(
          application_restart_recovery_effect(std::move(path), std::move(result)));
    }
    case replay_fact_tag::synchronization: {
      auto value = application_restart_synchronization(read_durability_fact(input));
      input.require_end();
      return detail::application_replay_fact(std::move(value));
    }
    case replay_fact_tag::completed_evidence: {
      auto value = read_completed_evidence(input, request);
      input.require_end();
      return detail::application_replay_fact(std::move(value));
    }
    case replay_fact_tag::seed:
      throw replay_codec_error(
          replay_codec_error_code::invalid_value,
          "application replay seed was used as a transition fact");
  }
  throw replay_codec_error(
      replay_codec_error_code::invalid_value,
      "application replay fact tag is invalid");
}
} // namespace

namespace detail {

application_journal_replay_encoding encode_replay_seed(
    const backend_observation_batch& observations)
{
  writer output = replay_writer(replay_fact_tag::seed);
  append_observation_batch(output, observations);
  auto bytes = output.finish();
  return application_journal_replay_encoding(
      reinterpret_cast<const std::byte*>(bytes.data()),
      reinterpret_cast<const std::byte*>(bytes.data() + bytes.size()));
}

backend_observation_batch decode_replay_seed(
    const application_journal_replay_encoding& encoding)
{
  reader input(
      reinterpret_cast<const std::uint8_t*>(encoding.data()), encoding.size());
  if (replay_reader_header(input) != replay_fact_tag::seed)
    throw replay_codec_error(
        replay_codec_error_code::invalid_value,
        "application replay seed has a transition-fact tag");
  auto value = read_observation_batch(input);
  input.require_end();
  return value;
}

application_journal_replay_encoding encode_replay_fact(
    const application_replay_fact& fact)
{
  writer output;
  std::visit([&](const auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, backend_operation_result>) {
      output = replay_writer(replay_fact_tag::incoming_payload);
      append_operation_result(output, value);
    } else if constexpr (std::is_same_v<T, application_restart_capture>) {
      output = replay_writer(replay_fact_tag::capture);
      output.append_u8(encode_outcome(value.result().outcome()));
      append_observation(output, value.result().captured());
      output.append_u8(value.result().exact_recovery_possible() ? 1 : 0);
      append_evidence(output, value.result().evidence());
    } else if constexpr (std::is_same_v<T, application_restart_rejected_effect>) {
      output = replay_writer(replay_fact_tag::rejected);
      output.append_string(value.path().string());
      output.append_u8(encode_outcome(value.result().outcome()));
      output.append_u8(value.result().record().has_value() ? 1 : 0);
      if (value.result().record())
        append_identity(output, *value.result().record());
      append_evidence(output, value.result().evidence());
    } else if constexpr (std::is_same_v<T, application_restart_active_effect>) {
      output = replay_writer(replay_fact_tag::active);
      output.append_string(value.path().string());
      append_operation_result(output, value.result());
    } else if constexpr (std::is_same_v<T, application_restart_recovery_effect>) {
      output = replay_writer(replay_fact_tag::recovery);
      output.append_string(value.path().string());
      append_operation_result(output, value.result());
    } else if constexpr (std::is_same_v<T, application_restart_synchronization>) {
      output = replay_writer(replay_fact_tag::synchronization);
      append_durability_fact(output, value.result());
    } else if constexpr (std::is_same_v<T, completed_application_evidence>) {
      output = replay_writer(replay_fact_tag::completed_evidence);
      append_completed_evidence(output, value);
    }
  }, fact);
  auto bytes = output.finish();
  return application_journal_replay_encoding(
      reinterpret_cast<const std::byte*>(bytes.data()),
      reinterpret_cast<const std::byte*>(bytes.data() + bytes.size()));
}

application_replay_fact decode_replay_fact(
    const application_journal_replay_encoding& encoding,
    const installation_application_request& request)
{
  return decode_fact(encoding, request);
}
application_replay_fact decode_replay_fact(
    const application_journal_replay_encoding& encoding,
    const upgrade_application_request& request)
{
  return decode_fact(encoding, request);
}
application_replay_fact decode_replay_fact(
    const application_journal_replay_encoding& encoding,
    const removal_application_request& request)
{
  return decode_fact(encoding, request);
}

} // namespace detail

} // namespace pkgapply
