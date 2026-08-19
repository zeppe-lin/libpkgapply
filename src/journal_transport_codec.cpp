// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal_transport_codec.h>

#include "journal_transport_access.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pkgapply {
namespace {

constexpr std::array<std::uint8_t, 8> declaration_magic = {
  'Z', 'L', 'A', 'P', 'J', 'D', 'C', 0,
};
constexpr std::array<std::uint8_t, 8> step_magic = {
  'Z', 'L', 'A', 'P', 'J', 'S', 'T', 0,
};
constexpr std::array<std::uint8_t, 8> cursor_magic = {
  'Z', 'L', 'A', 'P', 'J', 'C', 'U', 0,
};
constexpr std::uint64_t maximum_digest_text_size = 128;
constexpr std::uint64_t maximum_evidence_count = 1'000'000;
constexpr std::uint64_t maximum_effect_count = 1'000'000;
constexpr std::uint64_t maximum_projection_path_count = 1'000'000;
constexpr std::uint64_t maximum_projection_owner_count = 1'000'000;
constexpr std::uint64_t maximum_path_text_size =
    maximum_application_journal_transport_encoding_size;
constexpr std::uint64_t maximum_replay_size =
    maximum_application_journal_transport_encoding_size;

class writer final {
public:
  void append_u8(std::uint8_t value)
  {
    require_space(1);
    bytes_.push_back(value);
  }

  void append_u16(std::uint16_t value)
  {
    append_u8(static_cast<std::uint8_t>((value >> 8) & 0xffU));
    append_u8(static_cast<std::uint8_t>(value & 0xffU));
  }

  void append_u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      append_u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void append_bytes(const std::uint8_t* data, std::size_t size)
  {
    if (size == 0)
      return;
    require_space(size);
    bytes_.insert(bytes_.end(), data, data + size);
  }

  void append_string(std::string_view value)
  {
    append_u64(static_cast<std::uint64_t>(value.size()));
    append_bytes(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  void append_blob(const application_journal_replay_encoding& value)
  {
    append_u64(static_cast<std::uint64_t>(value.size()));
    require_space(value.size());
    bytes_.reserve(bytes_.size() + value.size());
    for (const auto byte : value)
      bytes_.push_back(std::to_integer<std::uint8_t>(byte));
  }

  template<std::size_t Size>
  void append_magic(const std::array<std::uint8_t, Size>& value)
  {
    append_bytes(value.data(), value.size());
  }

  [[nodiscard]] application_journal_transport_encoding finish()
  {
    if (bytes_.size() > maximum_application_journal_transport_encoding_size)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::limit_exceeded,
          "application journal transport encoding exceeds the size limit");
    return std::move(bytes_);
  }

private:
  void require_space(std::size_t size) const
  {
    if (bytes_.size() > maximum_application_journal_transport_encoding_size ||
        size > maximum_application_journal_transport_encoding_size -
                   bytes_.size())
    {
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::limit_exceeded,
          "application journal transport encoding exceeds the size limit");
    }
  }

  application_journal_transport_encoding bytes_;
};

class reader final {
public:
  reader(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size)
  {
    if (size > maximum_application_journal_transport_encoding_size)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::limit_exceeded,
          "application journal transport encoding exceeds the size limit");
    if (size != 0 && data == nullptr)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::truncated,
          "application journal transport encoding has no storage");
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
        (static_cast<std::uint16_t>(high) << 8) |
        static_cast<std::uint16_t>(low));
  }

  [[nodiscard]] std::uint64_t read_u64()
  {
    std::uint64_t value = 0;
    for (int index = 0; index < 8; ++index)
      value = (value << 8) | read_u8();
    return value;
  }

  [[nodiscard]] bool read_bool()
  {
    const auto value = read_u8();
    if (value > 1)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::invalid_value,
          "application journal transport encoding contains an invalid boolean");
    return value == 1;
  }

  [[nodiscard]] std::string read_string(std::uint64_t maximum_size)
  {
    const auto length = read_u64();
    if (length > maximum_size ||
        length > static_cast<std::uint64_t>(
                     std::numeric_limits<std::size_t>::max()))
    {
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::limit_exceeded,
          "application journal transport string exceeds its size limit");
    }
    const auto size = static_cast<std::size_t>(length);
    require(size);
    if (size == 0)
      return {};
    const auto* begin = reinterpret_cast<const char*>(data_ + offset_);
    std::string value(begin, size);
    offset_ += size;
    return value;
  }

  [[nodiscard]] application_journal_replay_encoding read_blob()
  {
    const auto length = read_u64();
    if (length > maximum_replay_size ||
        length > static_cast<std::uint64_t>(
                     std::numeric_limits<std::size_t>::max()))
    {
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::limit_exceeded,
          "application journal replay payload exceeds its size limit");
    }
    const auto size = static_cast<std::size_t>(length);
    require(size);
    application_journal_replay_encoding value;
    value.reserve(size);
    for (std::size_t index = 0; index < size; ++index)
      value.push_back(static_cast<std::byte>(data_[offset_ + index]));
    offset_ += size;
    return value;
  }


  template<std::size_t Size>
  void expect_magic(const std::array<std::uint8_t, Size>& magic)
  {
    require(magic.size());
    if (!std::equal(magic.begin(), magic.end(), data_ + offset_))
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::invalid_magic,
          "application journal transport encoding has invalid magic");
    offset_ += magic.size();
  }

  void require_end() const
  {
    if (offset_ != size_)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::trailing_data,
          "application journal transport encoding contains trailing data");
  }

private:
  void require(std::size_t count) const
  {
    if (count > size_ - offset_)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::truncated,
          "application journal transport encoding is truncated");
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

std::uint64_t read_count(
    reader& input, std::uint64_t maximum, std::string_view description)
{
  const auto count = input.read_u64();
  if (count > maximum ||
      count > static_cast<std::uint64_t>(
                  std::numeric_limits<std::size_t>::max()))
  {
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::limit_exceeded,
        std::string(description) + " exceeds its count limit");
  }
  return count;
}

void append_count(
    writer& output, std::size_t count, std::uint64_t maximum,
    std::string_view description)
{
  if (static_cast<std::uint64_t>(count) > maximum)
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::limit_exceeded,
        std::string(description) + " exceeds its count limit");
  output.append_u64(static_cast<std::uint64_t>(count));
}

std::uint8_t encode_operation_kind(pkgplan::operation_kind kind)
{
  switch (kind) {
    case pkgplan::operation_kind::install: return 1;
    case pkgplan::operation_kind::upgrade: return 2;
    case pkgplan::operation_kind::remove: return 3;
  }
  throw application_journal_transport_codec_error(
      application_journal_transport_codec_error_code::invalid_value,
      "application journal declaration contains an invalid operation kind");
}

pkgplan::operation_kind read_operation_kind(reader& input)
{
  switch (input.read_u8()) {
    case 1: return pkgplan::operation_kind::install;
    case 2: return pkgplan::operation_kind::upgrade;
    case 3: return pkgplan::operation_kind::remove;
  }
  throw application_journal_transport_codec_error(
      application_journal_transport_codec_error_code::invalid_value,
      "application journal declaration encoding contains an invalid operation kind");
}

std::uint8_t encode_projection_completeness(
    state_projection_completeness completeness)
{
  switch (completeness) {
    case state_projection_completeness::complete: return 1;
    case state_projection_completeness::incomplete: return 2;
  }
  throw application_journal_transport_codec_error(
      application_journal_transport_codec_error_code::invalid_value,
      "application journal declaration contains invalid projection completeness");
}

state_projection_completeness read_projection_completeness(reader& input)
{
  switch (input.read_u8()) {
    case 1: return state_projection_completeness::complete;
    case 2: return state_projection_completeness::incomplete;
  }
  throw application_journal_transport_codec_error(
      application_journal_transport_codec_error_code::invalid_value,
      "application journal declaration encoding contains invalid projection completeness");
}

void append_state_projection(
    writer& output, const lease_bound_state_projection& projection)
{
  append_identity(output, projection.identity());
  output.append_u16(projection.schema_version());
  append_identity(output, projection.lease());
  append_identity(output, projection.snapshot());
  append_identity(output, projection.ownership_inventory());
  output.append_u8(encode_projection_completeness(projection.completeness()));
  append_count(
      output, projection.paths().size(), maximum_projection_path_count,
      "application journal declaration projection paths");
  for (const auto& path : projection.paths()) {
    output.append_string(path.path().string());
    append_count(
        output, path.owners().size(), maximum_projection_owner_count,
        "application journal declaration projection owners");
    for (const auto& owner : path.owners())
      append_identity(output, owner);
  }
  append_identity(output, projection.evidence());
}

lease_bound_state_projection read_state_projection(reader& input)
{
  const auto expected =
      read_identity<lease_bound_state_projection_identity>(input);
  if (input.read_u16() != lease_bound_state_projection_schema_version)
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::unsupported_version,
        "application journal declaration state projection schema is unsupported");
  auto lease = read_identity<mutation_lease_instance_identity>(input);
  auto snapshot = read_identity<pkgplan::installed_state_snapshot_identity>(input);
  auto ownership = read_identity<pkgplan::ownership_inventory_identity>(input);
  const auto completeness = read_projection_completeness(input);
  const auto path_count = read_count(
      input, maximum_projection_path_count,
      "application journal declaration projection paths");
  std::vector<projected_path_owners> paths;
  paths.reserve(static_cast<std::size_t>(path_count));
  for (std::uint64_t index = 0; index < path_count; ++index) {
    auto path = pkgplan::package_path::parse(
        input.read_string(maximum_path_text_size));
    const auto owner_count = read_count(
        input, maximum_projection_owner_count,
        "application journal declaration projection owners");
    std::vector<pkgplan::installed_package_identity> owners;
    owners.reserve(static_cast<std::size_t>(owner_count));
    for (std::uint64_t owner = 0; owner < owner_count; ++owner)
      owners.push_back(read_identity<pkgplan::installed_package_identity>(input));
    paths.emplace_back(std::move(path), std::move(owners));
  }
  auto evidence = read_identity<state_projection_evidence_identity>(input);
  auto projection = lease_bound_state_projection::make(
      std::move(lease), std::move(snapshot), std::move(ownership),
      completeness, std::move(paths), std::move(evidence));
  if (projection.identity() != expected)
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::identity_mismatch,
        "application journal declaration projection identity does not match its body");
  return projection;
}

void append_header(writer& output, const application_journal_header& header)
{
  output.append_u16(header.schema_version());
  output.append_u8(encode_operation_kind(header.kind()));
  append_identity(output, header.request());
  append_identity(output, header.plan());
  output.append_u16(header.attempt().schema_version());
  output.append_bytes(
      header.attempt().nonce().bytes().data(),
      header.attempt().nonce().bytes().size());
  append_identity(output, header.target());
  append_identity(output, header.control());
  append_state_projection(output, header.admitted_state_projection());
  append_identity(output, header.lease());
  append_identity(output, header.backend());
}

application_journal_header read_header(reader& input)
{
  if (input.read_u16() != application_journal_schema_version)
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::unsupported_version,
        "application journal declaration header schema is unsupported");
  const auto kind = read_operation_kind(input);
  const auto request = read_identity<application_request_identity>(input);
  const auto plan = read_identity<pkgplan::operation_plan_identity>(input);
  if (input.read_u16() != application_attempt_schema_version)
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::unsupported_version,
        "application journal declaration attempt schema is unsupported");
  application_attempt_nonce::byte_array nonce_bytes{};
  for (auto& byte : nonce_bytes)
    byte = input.read_u8();
  const auto target = read_identity<application_target_context_identity>(input);
  const auto control =
      read_identity<application_execution_control_identity>(input);
  auto projection = read_state_projection(input);
  const auto lease = read_identity<mutation_lease_instance_identity>(input);
  const auto backend = read_identity<mutation_backend_identity>(input);
  const auto attempt = application_attempt::make(
      request, target, backend,
      application_attempt_nonce::from_bytes(nonce_bytes));
  return application_journal_header::make(
      kind, request, plan, attempt, target, control, std::move(projection),
      lease, backend);
}

application_journal_effect_kind read_effect_kind(reader& input)
{
  const auto value = input.read_u8();
  if (value < 1 || value > 15 || value == 6)
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::invalid_value,
        "application journal declaration encoding contains an invalid effect kind");
  return static_cast<application_journal_effect_kind>(value);
}

void append_effect(writer& output, const application_journal_effect& effect)
{
  output.append_u64(effect.ordinal());
  output.append_u8(static_cast<std::uint8_t>(effect.kind()));
  output.append_u8(effect.path().has_value() ? 1 : 0);
  if (effect.path())
    output.append_string(effect.path()->string());
}

application_journal_effect read_effect(reader& input)
{
  const auto ordinal = input.read_u64();
  const auto kind = read_effect_kind(input);
  std::optional<pkgplan::package_path> path;
  if (input.read_bool())
    path = pkgplan::package_path::parse(input.read_string(maximum_path_text_size));
  return application_journal_effect::make(ordinal, kind, std::move(path));
}

application_journal_state read_state(reader& input)
{
  switch (input.read_u8()) {
    case 1: return application_journal_state::preparing;
    case 2: return application_journal_state::prepared;
    case 3: return application_journal_state::mutating;
    case 4: return application_journal_state::effects_visible;
    case 5: return application_journal_state::result_observed;
    case 6: return application_journal_state::application_completed;
    case 7: return application_journal_state::external_resolution_pending;
    case 8: return application_journal_state::recovering;
    case 9: return application_journal_state::recovered;
    case 10: return application_journal_state::finalized;
    case 11: return application_journal_state::abandoned;
    case 12: return application_journal_state::indeterminate;
    case 13: return application_journal_state::recovery_pending;
  }
  throw application_journal_transport_codec_error(
      application_journal_transport_codec_error_code::invalid_value,
      "application journal transport encoding contains an invalid state");
}

application_journal_event_kind read_event_kind(reader& input)
{
  switch (input.read_u8()) {
    case 1: return application_journal_event_kind::intent;
    case 2: return application_journal_event_kind::completed;
    case 3: return application_journal_event_kind::failed;
    case 4: return application_journal_event_kind::indeterminate;
  }
  throw application_journal_transport_codec_error(
      application_journal_transport_codec_error_code::invalid_value,
      "application journal transport encoding contains an invalid event kind");
}

void append_event(writer& output, const application_journal_event& event)
{
  output.append_u64(event.sequence());
  output.append_u8(static_cast<std::uint8_t>(event.kind()));
  append_identity(output, event.effect());
  append_count(
      output, event.backend_evidence().size(), maximum_evidence_count,
      "application journal transport backend evidence");
  for (const auto& evidence : event.backend_evidence())
    append_identity(output, evidence);
}

application_journal_event read_event(reader& input)
{
  const auto sequence = input.read_u64();
  const auto kind = read_event_kind(input);
  auto effect = read_identity<application_journal_effect_identity>(input);
  const auto evidence_count = read_count(
      input, maximum_evidence_count,
      "application journal transport backend evidence");
  std::vector<application_backend_evidence_identity> evidence;
  evidence.reserve(static_cast<std::size_t>(evidence_count));
  for (std::uint64_t index = 0; index < evidence_count; ++index)
    evidence.push_back(read_identity<application_backend_evidence_identity>(input));
  return application_journal_event(
      sequence, kind, std::move(effect), std::move(evidence));
}


template<class Function>
auto translate_invalid(Function&& function)
{
  try {
    return function();
  } catch (const application_journal_transport_codec_error&) {
    throw;
  } catch (const std::invalid_argument& error) {
    throw application_journal_transport_codec_error(
        application_journal_transport_codec_error_code::invalid_value,
        std::string("application journal transport encoding is invalid: ") +
            error.what());
  }
}

} // namespace

application_journal_transport_codec_error::
application_journal_transport_codec_error(
    application_journal_transport_codec_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

application_journal_transport_codec_error::~application_journal_transport_codec_error() = default;

application_journal_transport_codec_error_code
application_journal_transport_codec_error::code() const noexcept
{
  return code_;
}

application_journal_transport_encoding
encode_application_journal_declaration(
    const application_journal_declaration& declaration)
{
  writer output;
  output.append_magic(declaration_magic);
  output.append_u16(application_journal_transport_encoding_version);
  output.append_u16(declaration.schema_version());
  append_identity(output, declaration.identity());
  append_header(output, declaration.header());
  append_count(
      output, declaration.effects().size(), maximum_effect_count,
      "application journal declaration effect graph");
  for (const auto& effect : declaration.effects())
    append_effect(output, effect);
  output.append_blob(declaration.replay_seed());
  return output.finish();
}

application_journal_declaration
decode_application_journal_declaration(const std::uint8_t* data,
                                       std::size_t size)
{
  return translate_invalid([&]() {
    reader input(data, size);
    input.expect_magic(declaration_magic);
    if (input.read_u16() != application_journal_transport_encoding_version)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::unsupported_version,
          "application journal declaration encoding version is unsupported");
    if (input.read_u16() != application_journal_declaration_schema_version)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::unsupported_version,
          "application journal declaration schema is unsupported");
    const auto expected =
        read_identity<application_journal_declaration_identity>(input);
    auto header = read_header(input);
    const auto effect_count = read_count(
        input, maximum_effect_count, "application journal declaration effect graph");
    std::vector<application_journal_effect> effects;
    effects.reserve(static_cast<std::size_t>(effect_count));
    for (std::uint64_t index = 0; index < effect_count; ++index)
      effects.push_back(read_effect(input));
    auto replay_seed = input.read_blob();
    input.require_end();
    auto declaration = application_journal_declaration::make(
        std::move(header), std::move(effects), std::move(replay_seed));
    if (declaration.identity() != expected)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::identity_mismatch,
          "application journal declaration identity does not match its content");
    return declaration;
  });
}

application_journal_declaration
decode_application_journal_declaration(
    const application_journal_transport_encoding& encoding)
{
  return decode_application_journal_declaration(encoding.data(), encoding.size());
}

application_journal_transport_encoding
encode_application_journal_step(const application_journal_step& step)
{
  writer output;
  output.append_magic(step_magic);
  output.append_u16(application_journal_transport_encoding_version);
  output.append_u16(step.schema_version());
  append_identity(output, step.identity());
  append_identity(output, step.declaration());
  output.append_u64(step.sequence());
  output.append_u8(step.predecessor().has_value() ? 1 : 0);
  if (step.predecessor())
    append_identity(output, *step.predecessor());
  output.append_u8(static_cast<std::uint8_t>(step.state()));
  output.append_u8(step.event().has_value() ? 1 : 0);
  if (step.event())
    append_event(output, *step.event());
  output.append_blob(step.replay_fact());
  output.append_u8(step.receipt().has_value() ? 1 : 0);
  if (step.receipt())
    append_identity(output, *step.receipt());
  output.append_u8(step.completed_evidence().has_value() ? 1 : 0);
  if (step.completed_evidence())
    append_identity(output, *step.completed_evidence());
  return output.finish();
}

application_journal_step
decode_application_journal_step(const std::uint8_t* data, std::size_t size)
{
  return translate_invalid([&]() {
    reader input(data, size);
    input.expect_magic(step_magic);
    if (input.read_u16() != application_journal_transport_encoding_version)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::unsupported_version,
          "application journal step encoding version is unsupported");
    if (input.read_u16() != application_journal_step_schema_version)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::unsupported_version,
          "application journal step schema is unsupported");
    const auto expected = read_identity<application_journal_step_identity>(input);
    auto declaration =
        read_identity<application_journal_declaration_identity>(input);
    const auto sequence = input.read_u64();
    std::optional<application_journal_step_identity> predecessor;
    if (input.read_bool())
      predecessor = read_identity<application_journal_step_identity>(input);
    const auto state = read_state(input);
    std::optional<application_journal_event> event;
    if (input.read_bool())
      event = read_event(input);
    auto replay_fact = input.read_blob();
    std::optional<application_receipt_identity> receipt;
    if (input.read_bool())
      receipt = read_identity<application_receipt_identity>(input);
    std::optional<completed_application_evidence_identity> completed_evidence;
    if (input.read_bool()) {
      completed_evidence =
          read_identity<completed_application_evidence_identity>(input);
    }
    input.require_end();
    auto step = application_journal_step::make(
        std::move(declaration), sequence, std::move(predecessor), state,
        std::move(event), std::move(replay_fact), std::move(receipt),
        std::move(completed_evidence));
    if (step.identity() != expected)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::identity_mismatch,
          "application journal step identity does not match its content");
    return step;
  });
}

application_journal_step
decode_application_journal_step(
    const application_journal_transport_encoding& encoding)
{
  return decode_application_journal_step(encoding.data(), encoding.size());
}

application_journal_transport_encoding
encode_application_journal_cursor(const application_journal_cursor& cursor)
{
  writer output;
  output.append_magic(cursor_magic);
  output.append_u16(application_journal_transport_encoding_version);
  output.append_u16(cursor.schema_version());
  append_identity(output, cursor.identity());
  append_identity(output, cursor.declaration());
  output.append_u64(cursor.step_count());
  output.append_u8(cursor.latest_step().has_value() ? 1 : 0);
  if (cursor.latest_step())
    append_identity(output, *cursor.latest_step());
  output.append_u8(static_cast<std::uint8_t>(cursor.state()));
  output.append_u8(cursor.receipt().has_value() ? 1 : 0);
  if (cursor.receipt())
    append_identity(output, *cursor.receipt());
  output.append_u8(cursor.completed_evidence().has_value() ? 1 : 0);
  if (cursor.completed_evidence())
    append_identity(output, *cursor.completed_evidence());
  return output.finish();
}

application_journal_cursor
decode_application_journal_cursor(const std::uint8_t* data, std::size_t size)
{
  return translate_invalid([&]() {
    reader input(data, size);
    input.expect_magic(cursor_magic);
    if (input.read_u16() != application_journal_transport_encoding_version)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::unsupported_version,
          "application journal cursor encoding version is unsupported");
    if (input.read_u16() != application_journal_cursor_schema_version)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::unsupported_version,
          "application journal cursor schema is unsupported");
    const auto expected = read_identity<application_journal_cursor_identity>(input);
    auto declaration =
        read_identity<application_journal_declaration_identity>(input);
    const auto step_count = input.read_u64();
    std::optional<application_journal_step_identity> latest_step;
    if (input.read_bool())
      latest_step = read_identity<application_journal_step_identity>(input);
    const auto state = read_state(input);
    std::optional<application_receipt_identity> receipt;
    if (input.read_bool())
      receipt = read_identity<application_receipt_identity>(input);
    std::optional<completed_application_evidence_identity> completed_evidence;
    if (input.read_bool()) {
      completed_evidence =
          read_identity<completed_application_evidence_identity>(input);
    }
    input.require_end();

    if ((step_count == 0) != !latest_step.has_value())
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::invalid_value,
          "application journal cursor head contradicts its step count");

    auto cursor = detail::application_journal_cursor_codec_access::restore(
        std::move(declaration), step_count, std::move(latest_step), state,
        std::move(receipt), std::move(completed_evidence));
    if (cursor.identity() != expected)
      throw application_journal_transport_codec_error(
          application_journal_transport_codec_error_code::identity_mismatch,
          "application journal cursor identity does not match its content");
    return cursor;
  });
}

application_journal_cursor
decode_application_journal_cursor(
    const application_journal_transport_encoding& encoding)
{
  return decode_application_journal_cursor(encoding.data(), encoding.size());
}

} // namespace pkgapply
