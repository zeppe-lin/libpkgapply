// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/restart_checkpoint_codec.h>

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

constexpr std::array<std::uint8_t, 8> checkpoint_magic = {
    'Z', 'L', 'A', 'P', 'C', 'H', 'K', 0,
};
constexpr std::uint64_t maximum_item_count = 1'000'000;
constexpr std::uint64_t maximum_digest_text_size = 128;
constexpr std::uint64_t maximum_text_size =
    maximum_application_restart_checkpoint_encoding_size;

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
    if (size > maximum_application_restart_checkpoint_encoding_size -
                   bytes_.size())
    {
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::limit_exceeded,
          "application restart checkpoint encoding exceeds the size limit");
    }
    bytes_.insert(bytes_.end(), data, data + size);
  }
  void append_string(std::string_view value)
  {
    append_u64(static_cast<std::uint64_t>(value.size()));
    append_bytes(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }
  [[nodiscard]] application_restart_checkpoint_encoding finish()
  {
    if (bytes_.size() > maximum_application_restart_checkpoint_encoding_size)
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::limit_exceeded,
          "application restart checkpoint encoding exceeds the size limit");
    return std::move(bytes_);
  }
private:
  application_restart_checkpoint_encoding bytes_;
};

class reader final {
public:
  reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size)
  {
    if (size > maximum_application_restart_checkpoint_encoding_size)
      fail(application_restart_checkpoint_codec_error_code::limit_exceeded,
           "application restart checkpoint encoding exceeds the size limit");
    if (size != 0 && data == nullptr)
      fail(application_restart_checkpoint_codec_error_code::truncated,
           "application restart checkpoint encoding has no storage");
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
      fail(application_restart_checkpoint_codec_error_code::invalid_value,
           "application restart checkpoint contains an invalid integer sign");
    const auto magnitude = read_u64();
    const auto maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (sign == 0) {
      if (magnitude > maximum)
        fail(application_restart_checkpoint_codec_error_code::invalid_value,
             "application restart checkpoint integer is out of range");
      return static_cast<std::int64_t>(magnitude);
    }
    if (magnitude > maximum + 1U)
      fail(application_restart_checkpoint_codec_error_code::invalid_value,
           "application restart checkpoint integer is out of range");
    if (magnitude == maximum + 1U)
      return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
  }
  [[nodiscard]] bool read_bool()
  {
    const auto value = read_u8();
    if (value > 1)
      fail(application_restart_checkpoint_codec_error_code::invalid_value,
           "application restart checkpoint contains an invalid boolean");
    return value == 1;
  }
  [[nodiscard]] std::string read_string(std::uint64_t maximum)
  {
    const auto length = read_u64();
    if (length > maximum || length >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
      fail(application_restart_checkpoint_codec_error_code::limit_exceeded,
           "application restart checkpoint string exceeds its size limit");
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
    require(checkpoint_magic.size());
    if (!std::equal(
            checkpoint_magic.begin(), checkpoint_magic.end(), data_ + offset_))
    {
      fail(application_restart_checkpoint_codec_error_code::invalid_magic,
           "application restart checkpoint encoding has invalid magic");
    }
    offset_ += checkpoint_magic.size();
  }
  void require_end() const
  {
    if (offset_ != size_)
      fail(application_restart_checkpoint_codec_error_code::trailing_data,
           "application restart checkpoint encoding contains trailing data");
  }
private:
  [[noreturn]] static void fail(
      application_restart_checkpoint_codec_error_code code,
      std::string message)
  {
    throw application_restart_checkpoint_codec_error(code, std::move(message));
  }
  void require(std::size_t count) const
  {
    if (count > size_ - offset_)
      fail(application_restart_checkpoint_codec_error_code::truncated,
           "application restart checkpoint encoding is truncated");
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::limit_exceeded,
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
  throw application_restart_checkpoint_codec_error(
      application_restart_checkpoint_codec_error_code::invalid_value,
      "application restart checkpoint contains an invalid backend outcome");
}

backend_operation_outcome read_outcome(reader& input)
{
  switch (input.read_u8()) {
    case 1: return backend_operation_outcome::completed;
    case 2: return backend_operation_outcome::conditional_retained;
    case 3: return backend_operation_outcome::failed;
    case 4: return backend_operation_outcome::indeterminate;
  }
  throw application_restart_checkpoint_codec_error(
      application_restart_checkpoint_codec_error_code::invalid_value,
      "application restart checkpoint contains an invalid backend outcome");
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
  throw application_restart_checkpoint_codec_error(
      application_restart_checkpoint_codec_error_code::invalid_value,
      "application restart checkpoint contains an invalid fact state");
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::invalid_value,
        "application restart checkpoint contains an invalid object kind");
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::invalid_value,
        "application restart checkpoint contains invalid object provenance");
  const auto completeness_value = input.read_u8();
  if (completeness_value < 1 || completeness_value > 2)
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::invalid_value,
        "application restart checkpoint contains invalid object completeness");
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
        throw application_restart_checkpoint_codec_error(
            application_restart_checkpoint_codec_error_code::invalid_value,
            "application restart checkpoint observation path disagrees with object");
      return application_path_observation::present(std::move(object));
    }
    case static_cast<std::uint8_t>(fact_state::unknown):
      return application_path_observation::unknown(std::move(path));
    case static_cast<std::uint8_t>(fact_state::not_applicable):
      return application_path_observation::absent(std::move(path));
  }
  throw application_restart_checkpoint_codec_error(
      application_restart_checkpoint_codec_error_code::invalid_value,
      "application restart checkpoint contains an invalid observation state");
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::invalid_value,
        "application restart checkpoint contains invalid durability truth");
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::request_mismatch,
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
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::invalid_value,
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::request_mismatch,
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
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::identity_mismatch,
        "completed evidence identity does not match checkpoint content");
  return result;
}

void append_checkpoint_body(writer& output, const application_restart_checkpoint& value)
{
  append_identity(output, value.journal());
  append_observation_batch(output, value.admitted_observations());

  output.append_u8(value.incoming_payload().has_value() ? 1 : 0);
  if (value.incoming_payload())
    append_operation_result(output, *value.incoming_payload());

  output.append_u64(static_cast<std::uint64_t>(value.captures().size()));
  for (const auto& item : value.captures()) {
    output.append_u8(encode_outcome(item.result().outcome()));
    append_observation(output, item.result().captured());
    output.append_u8(item.result().exact_recovery_possible() ? 1 : 0);
    append_evidence(output, item.result().evidence());
  }

  output.append_u64(static_cast<std::uint64_t>(value.rejected_effects().size()));
  for (const auto& item : value.rejected_effects()) {
    output.append_string(item.path().string());
    output.append_u8(encode_outcome(item.result().outcome()));
    output.append_u8(item.result().record().has_value() ? 1 : 0);
    if (item.result().record())
      append_identity(output, *item.result().record());
    append_evidence(output, item.result().evidence());
  }

  auto append_effects = [&](const auto& effects) {
    output.append_u64(static_cast<std::uint64_t>(effects.size()));
    for (const auto& item : effects) {
      output.append_string(item.path().string());
      append_operation_result(output, item.result());
    }
  };
  append_effects(value.active_effects());
  append_effects(value.recovery_effects());

  output.append_u64(static_cast<std::uint64_t>(value.synchronizations().size()));
  for (const auto& item : value.synchronizations())
    append_durability_fact(output, item.result());
  append_durability(output, value.durability());
  append_evidence(output, value.backend_evidence());

  output.append_u8(value.completed_evidence().has_value() ? 1 : 0);
  if (value.completed_evidence())
    append_completed_evidence(output, *value.completed_evidence());
}

template<class Request>
application_restart_checkpoint decode_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& expected_journal,
    const Request& request)
{
  try {
    reader input(data, size);
    input.expect_magic();
    if (input.read_u16() != application_restart_checkpoint_encoding_version)
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::unsupported_version,
          "application restart checkpoint encoding version is unsupported");
    std::array<std::uint8_t, 32> expected_checksum{};
    for (auto& byte : expected_checksum)
      byte = input.read_u8();
    constexpr std::size_t envelope_size = checkpoint_magic.size() + 2U + 32U;
    const auto actual_checksum = detail::sha256(
        reinterpret_cast<const std::byte*>(data + envelope_size),
        size - envelope_size);
    if (actual_checksum != expected_checksum)
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::identity_mismatch,
          "application restart checkpoint checksum does not match its content");

    auto journal = read_identity<application_journal_record_identity>(input);
    if (journal != expected_journal.identity())
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::identity_mismatch,
          "restart checkpoint does not belong to the supplied journal snapshot");
    const auto& header = expected_journal.header();
    if (header.kind() != request.plan().kind() ||
        header.request() != request.identity() ||
        header.plan() != request.plan().identity() ||
        header.target() != request.target().identity() ||
        header.control() != request.control().identity())
    {
      throw application_restart_checkpoint_codec_error(
          application_restart_checkpoint_codec_error_code::request_mismatch,
          "restart checkpoint journal differs from the immutable request");
    }
    auto admitted = read_observation_batch(input);
    std::optional<backend_operation_result> incoming;
    if (input.read_bool())
      incoming = read_operation_result(input);

    const auto capture_count = read_count(input, "restart captures");
    std::vector<application_restart_capture> captures;
    captures.reserve(capture_count);
    for (std::size_t index = 0; index < capture_count; ++index) {
      const auto outcome = read_outcome(input);
      auto observation = read_observation(input);
      const bool exact = input.read_bool();
      captures.emplace_back(old_object_capture_result(
          outcome, std::move(observation), exact, read_evidence(input)));
    }

    const auto rejected_count = read_count(input, "restart rejected effects");
    std::vector<application_restart_rejected_effect> rejected;
    rejected.reserve(rejected_count);
    for (std::size_t index = 0; index < rejected_count; ++index) {
      auto path = pkgplan::package_path::parse(
          input.read_string(maximum_text_size));
      const auto outcome = read_outcome(input);
      std::optional<rejected_object_record_identity> record;
      if (input.read_bool())
        record = read_identity<rejected_object_record_identity>(input);
      rejected.emplace_back(
          std::move(path), rejected_object_publication_result(
              outcome, std::move(record), read_evidence(input)));
    }

    const auto read_effects = [&](auto maker, std::string_view description) {
      using Value = decltype(maker(
          pkgplan::package_path::parse("x"),
          backend_operation_result(backend_operation_outcome::failed)));
      const auto count = read_count(input, description);
      std::vector<Value> values;
      values.reserve(count);
      for (std::size_t index = 0; index < count; ++index) {
        auto path = pkgplan::package_path::parse(
            input.read_string(maximum_text_size));
        values.push_back(maker(std::move(path), read_operation_result(input)));
      }
      return values;
    };
    auto active = read_effects(
        [](pkgplan::package_path path, backend_operation_result result) {
          return application_restart_active_effect(
              std::move(path), std::move(result));
        }, "restart active effects");
    auto recovery = read_effects(
        [](pkgplan::package_path path, backend_operation_result result) {
          return application_restart_recovery_effect(
              std::move(path), std::move(result));
        }, "restart recovery effects");

    const auto sync_count = read_count(input, "restart synchronizations");
    std::vector<application_restart_synchronization> synchronizations;
    synchronizations.reserve(sync_count);
    for (std::size_t index = 0; index < sync_count; ++index)
      synchronizations.emplace_back(read_durability_fact(input));
    auto durability = read_durability(input);
    auto backend_evidence = read_evidence(input);
    std::optional<completed_application_evidence> completed;
    if (input.read_bool())
      completed = read_completed_evidence(input, request);
    input.require_end();

    return application_restart_checkpoint::make(
        std::move(journal), std::move(admitted), std::move(incoming),
        std::move(captures), std::move(rejected), std::move(active),
        std::move(recovery), std::move(synchronizations),
        std::move(durability), std::move(backend_evidence),
        std::move(completed));
  }
  catch (const application_restart_checkpoint_codec_error&) {
    throw;
  }
  catch (const std::invalid_argument& error) {
    throw application_restart_checkpoint_codec_error(
        application_restart_checkpoint_codec_error_code::invalid_value,
        std::string("application restart checkpoint encoding is invalid: ") +
            error.what());
  }
}

} // namespace

application_restart_checkpoint_codec_error::
application_restart_checkpoint_codec_error(
    application_restart_checkpoint_codec_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

application_restart_checkpoint_codec_error_code
application_restart_checkpoint_codec_error::code() const noexcept
{
  return code_;
}

application_restart_checkpoint_encoding
encode_application_restart_checkpoint(
    const application_restart_checkpoint& checkpoint)
{
  writer body;
  append_checkpoint_body(body, checkpoint);
  auto body_bytes = body.finish();
  const auto checksum = detail::sha256(
      reinterpret_cast<const std::byte*>(body_bytes.data()),
      body_bytes.size());

  writer output;
  for (const auto byte : checkpoint_magic)
    output.append_u8(byte);
  output.append_u16(application_restart_checkpoint_encoding_version);
  output.append_bytes(checksum.data(), checksum.size());
  output.append_bytes(body_bytes.data(), body_bytes.size());
  return output.finish();
}

application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const installation_application_request& request)
{
  return decode_checkpoint(data, size, journal, request);
}

application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const upgrade_application_request& request)
{
  return decode_checkpoint(data, size, journal, request);
}

application_restart_checkpoint
decode_application_restart_checkpoint(
    const std::uint8_t* data,
    std::size_t size,
    const application_journal_record& journal,
    const removal_application_request& request)
{
  return decode_checkpoint(data, size, journal, request);
}

application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const installation_application_request& request)
{
  return decode_checkpoint(encoding.data(), encoding.size(), journal, request);
}

application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const upgrade_application_request& request)
{
  return decode_checkpoint(encoding.data(), encoding.size(), journal, request);
}

application_restart_checkpoint
decode_application_restart_checkpoint(
    const application_restart_checkpoint_encoding& encoding,
    const application_journal_record& journal,
    const removal_application_request& request)
{
  return decode_checkpoint(encoding.data(), encoding.size(), journal, request);
}

} // namespace pkgapply
