// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal_codec.h>

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

constexpr std::array<std::uint8_t, 8> journal_magic = {
  'Z', 'L', 'A', 'P', 'J', 'N', 'L', 0,
};
constexpr std::uint64_t maximum_effect_count = 1'000'000;
constexpr std::uint64_t maximum_event_count = 4'000'000;
constexpr std::uint64_t maximum_evidence_count = 1'000'000;
constexpr std::uint64_t maximum_projection_path_count = 1'000'000;
constexpr std::uint64_t maximum_projection_owner_count = 1'000'000;
constexpr std::uint64_t maximum_digest_text_size = 128;
constexpr std::uint64_t maximum_path_text_size =
    maximum_application_journal_encoding_size;

class writer final {
public:
  void append_u8(std::uint8_t value)
  {
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
    if (size > maximum_application_journal_encoding_size - bytes_.size())
      throw application_journal_codec_error(
          application_journal_codec_error_code::limit_exceeded,
          "application journal encoding exceeds the size limit");
    bytes_.insert(bytes_.end(), data, data + size);
  }

  void append_string(std::string_view value)
  {
    append_u64(static_cast<std::uint64_t>(value.size()));
    append_bytes(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  [[nodiscard]] application_journal_encoding finish()
  {
    if (bytes_.size() > maximum_application_journal_encoding_size)
      throw application_journal_codec_error(
          application_journal_codec_error_code::limit_exceeded,
          "application journal encoding exceeds the size limit");
    return std::move(bytes_);
  }

private:
  application_journal_encoding bytes_;
};

class reader final {
public:
  reader(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size)
  {
    if (size > maximum_application_journal_encoding_size)
      throw application_journal_codec_error(
          application_journal_codec_error_code::limit_exceeded,
          "application journal encoding exceeds the size limit");
    if (size != 0 && data == nullptr)
      throw application_journal_codec_error(
          application_journal_codec_error_code::truncated,
          "application journal encoding has no storage");
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
      throw application_journal_codec_error(
          application_journal_codec_error_code::invalid_value,
          "application journal encoding contains an invalid boolean");
    return value == 1;
  }

  [[nodiscard]] std::string read_string(std::uint64_t maximum_size)
  {
    const auto length = read_u64();
    if (length > maximum_size ||
        length > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
      throw application_journal_codec_error(
          application_journal_codec_error_code::limit_exceeded,
          "application journal string exceeds its size limit");
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

  void expect_magic()
  {
    require(journal_magic.size());
    if (!std::equal(
            journal_magic.begin(), journal_magic.end(), data_ + offset_))
    {
      throw application_journal_codec_error(
          application_journal_codec_error_code::invalid_magic,
          "application journal encoding has invalid magic");
    }
    offset_ += journal_magic.size();
  }

  void require_end() const
  {
    if (offset_ != size_)
      throw application_journal_codec_error(
          application_journal_codec_error_code::trailing_data,
          "application journal encoding contains trailing data");
  }

private:
  void require(std::size_t count) const
  {
    if (count > size_ - offset_)
      throw application_journal_codec_error(
          application_journal_codec_error_code::truncated,
          "application journal encoding is truncated");
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
Identity read_application_identity(reader& input)
{
  return Identity::parse(input.read_string(maximum_digest_text_size));
}

template<class Identity>
Identity read_planning_identity(reader& input)
{
  return Identity::parse(input.read_string(maximum_digest_text_size));
}

pkgplan::operation_kind read_operation_kind(reader& input)
{
  switch (input.read_u8()) {
    case 1:
      return pkgplan::operation_kind::install;
    case 2:
      return pkgplan::operation_kind::upgrade;
    case 3:
      return pkgplan::operation_kind::remove;
  }
  throw application_journal_codec_error(
      application_journal_codec_error_code::invalid_value,
      "application journal encoding contains an invalid operation kind");
}

std::uint8_t encode_operation_kind(pkgplan::operation_kind kind)
{
  switch (kind) {
    case pkgplan::operation_kind::install:
      return 1;
    case pkgplan::operation_kind::upgrade:
      return 2;
    case pkgplan::operation_kind::remove:
      return 3;
  }
  throw application_journal_codec_error(
      application_journal_codec_error_code::invalid_value,
      "application journal contains an invalid operation kind");
}

std::uint64_t read_count(
    reader& input, std::uint64_t maximum, std::string_view description);

std::uint8_t encode_projection_completeness(
    state_projection_completeness completeness)
{
  switch (completeness) {
    case state_projection_completeness::complete:
      return 1;
    case state_projection_completeness::incomplete:
      return 2;
  }
  throw application_journal_codec_error(
      application_journal_codec_error_code::invalid_value,
      "application journal contains invalid state-projection completeness");
}

state_projection_completeness read_projection_completeness(reader& input)
{
  switch (input.read_u8()) {
    case 1:
      return state_projection_completeness::complete;
    case 2:
      return state_projection_completeness::incomplete;
  }
  throw application_journal_codec_error(
      application_journal_codec_error_code::invalid_value,
      "application journal encoding contains invalid state-projection completeness");
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
  output.append_u64(static_cast<std::uint64_t>(projection.paths().size()));
  for (const auto& path : projection.paths()) {
    output.append_string(path.path().string());
    output.append_u64(static_cast<std::uint64_t>(path.owners().size()));
    for (const auto& owner : path.owners())
      append_identity(output, owner);
  }
  append_identity(output, projection.evidence());
}

lease_bound_state_projection read_state_projection(reader& input)
{
  const auto expected_identity = read_application_identity<
      lease_bound_state_projection_identity>(input);
  if (input.read_u16() != lease_bound_state_projection_schema_version)
    throw application_journal_codec_error(
        application_journal_codec_error_code::unsupported_version,
        "application journal state-projection schema is unsupported");

  auto lease = read_application_identity<mutation_lease_instance_identity>(input);
  auto snapshot = read_planning_identity<
      pkgplan::installed_state_snapshot_identity>(input);
  auto ownership = read_planning_identity<
      pkgplan::ownership_inventory_identity>(input);
  const auto completeness = read_projection_completeness(input);
  const auto path_count = read_count(
      input, maximum_projection_path_count,
      "application journal state-projection path universe");
  std::vector<projected_path_owners> paths;
  paths.reserve(static_cast<std::size_t>(path_count));
  for (std::uint64_t index = 0; index < path_count; ++index) {
    auto path = pkgplan::package_path::parse(
        input.read_string(maximum_path_text_size));
    const auto owner_count = read_count(
        input, maximum_projection_owner_count,
        "application journal state-projection owner universe");
    std::vector<pkgplan::installed_package_identity> owners;
    owners.reserve(static_cast<std::size_t>(owner_count));
    for (std::uint64_t owner = 0; owner < owner_count; ++owner)
      owners.push_back(read_planning_identity<
          pkgplan::installed_package_identity>(input));
    paths.emplace_back(std::move(path), std::move(owners));
  }
  auto evidence = read_application_identity<state_projection_evidence_identity>(input);
  auto projection = lease_bound_state_projection::make(
      std::move(lease), std::move(snapshot), std::move(ownership),
      completeness, std::move(paths), std::move(evidence));
  if (projection.identity() != expected_identity)
    throw application_journal_codec_error(
        application_journal_codec_error_code::identity_mismatch,
        "application journal state-projection body does not reproduce its identity");
  return projection;
}

application_journal_state read_state(reader& input)
{
  const auto value = input.read_u8();
  if (value < 1 || value > 13)
    throw application_journal_codec_error(
        application_journal_codec_error_code::invalid_value,
        "application journal encoding contains an invalid state");
  return static_cast<application_journal_state>(value);
}

application_journal_effect_kind read_effect_kind(reader& input)
{
  const auto value = input.read_u8();
  if (value < 1 || value > 15 || value == 6)
    throw application_journal_codec_error(
        application_journal_codec_error_code::invalid_value,
        "application journal encoding contains an invalid effect kind");
  return static_cast<application_journal_effect_kind>(value);
}

application_journal_event_kind read_event_kind(reader& input)
{
  const auto value = input.read_u8();
  if (value < 1 || value > 4)
    throw application_journal_codec_error(
        application_journal_codec_error_code::invalid_value,
        "application journal encoding contains an invalid event kind");
  return static_cast<application_journal_event_kind>(value);
}

std::uint64_t read_count(
    reader& input,
    std::uint64_t maximum,
    std::string_view description)
{
  const auto count = input.read_u64();
  if (count > maximum ||
      count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
  {
    throw application_journal_codec_error(
        application_journal_codec_error_code::limit_exceeded,
        std::string(description) + " exceeds its count limit");
  }
  return count;
}

bool same_event(
    const application_journal_event& lhs,
    const application_journal_event& rhs) noexcept
{
  return lhs.sequence() == rhs.sequence() && lhs.kind() == rhs.kind() &&
         lhs.effect() == rhs.effect() &&
         lhs.backend_evidence() == rhs.backend_evidence();
}

bool state_may_follow(
    application_journal_state previous,
    application_journal_state next) noexcept
{
  if (previous == next)
    return true;

  switch (previous) {
    case application_journal_state::preparing:
      return next == application_journal_state::prepared ||
             next == application_journal_state::abandoned;
    case application_journal_state::prepared:
      return next == application_journal_state::mutating;
    case application_journal_state::mutating:
      return next == application_journal_state::effects_visible ||
             next == application_journal_state::recovery_pending ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::effects_visible:
      return next == application_journal_state::result_observed ||
             next == application_journal_state::recovery_pending ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::result_observed:
      return next == application_journal_state::application_completed ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::recovery_pending ||
             next == application_journal_state::indeterminate;
    case application_journal_state::recovery_pending:
      return next == application_journal_state::recovering ||
             next == application_journal_state::recovered ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::indeterminate:
      return next == application_journal_state::recovering ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::abandoned;
    case application_journal_state::recovering:
      return next == application_journal_state::recovered ||
             next == application_journal_state::effects_visible ||
             next == application_journal_state::indeterminate ||
             next == application_journal_state::abandoned;
    case application_journal_state::application_completed:
    case application_journal_state::external_resolution_pending:
    case application_journal_state::recovered:
    case application_journal_state::finalized:
    case application_journal_state::abandoned:
      return false;
  }
  return false;
}

} // namespace

application_journal_codec_error::application_journal_codec_error(
    application_journal_codec_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

application_journal_codec_error::~application_journal_codec_error() = default;

application_journal_codec_error_code
application_journal_codec_error::code() const noexcept
{
  return code_;
}

application_journal_transition_error::application_journal_transition_error(
    application_journal_transition_error_code code,
    std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

application_journal_transition_error::~application_journal_transition_error() = default;

application_journal_transition_error_code
application_journal_transition_error::code() const noexcept
{
  return code_;
}

application_journal_encoding
encode_application_journal(const application_journal_record& record)
{
  writer output;
  output.append_bytes(journal_magic.data(), journal_magic.size());
  output.append_u16(application_journal_encoding_version);
  append_identity(output, record.identity());
  output.append_u16(record.schema_version());

  const auto& header = record.header();
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

  output.append_u8(static_cast<std::uint8_t>(record.state()));
  output.append_u64(static_cast<std::uint64_t>(record.effects().size()));
  for (const auto& effect : record.effects()) {
    output.append_u64(effect.ordinal());
    output.append_u8(static_cast<std::uint8_t>(effect.kind()));
    output.append_u8(effect.path().has_value() ? 1 : 0);
    if (effect.path())
      output.append_string(effect.path()->string());
  }

  output.append_u64(static_cast<std::uint64_t>(record.events().size()));
  for (const auto& event : record.events()) {
    output.append_u64(event.sequence());
    output.append_u8(static_cast<std::uint8_t>(event.kind()));
    append_identity(output, event.effect());
    output.append_u64(
        static_cast<std::uint64_t>(event.backend_evidence().size()));
    for (const auto& evidence : event.backend_evidence())
      append_identity(output, evidence);
  }

  output.append_u8(record.receipt().has_value() ? 1 : 0);
  if (record.receipt())
    append_identity(output, *record.receipt());
  output.append_u8(record.completed_evidence().has_value() ? 1 : 0);
  if (record.completed_evidence())
    append_identity(output, *record.completed_evidence());

  return output.finish();
}

application_journal_record
decode_application_journal(const std::uint8_t* data, std::size_t size)
{
  try {
    reader input(data, size);
    input.expect_magic();
    if (input.read_u16() != application_journal_encoding_version)
      throw application_journal_codec_error(
          application_journal_codec_error_code::unsupported_version,
          "application journal encoding version is unsupported");

    const auto expected_identity =
        read_application_identity<application_journal_record_identity>(input);
    if (input.read_u16() != application_journal_record_schema_version)
      throw application_journal_codec_error(
          application_journal_codec_error_code::unsupported_version,
          "application journal record schema is unsupported");
    if (input.read_u16() != application_journal_schema_version)
      throw application_journal_codec_error(
          application_journal_codec_error_code::unsupported_version,
          "application journal header schema is unsupported");

    const auto kind = read_operation_kind(input);
    const auto request =
        read_application_identity<application_request_identity>(input);
    const auto plan =
        read_planning_identity<pkgplan::operation_plan_identity>(input);
    if (input.read_u16() != application_attempt_schema_version)
      throw application_journal_codec_error(
          application_journal_codec_error_code::unsupported_version,
          "application attempt schema is unsupported");

    application_attempt_nonce::byte_array nonce_bytes{};
    for (auto& byte : nonce_bytes)
      byte = input.read_u8();

    const auto target =
        read_application_identity<application_target_context_identity>(input);
    const auto control = read_application_identity<
        application_execution_control_identity>(input);
    auto state_projection = read_state_projection(input);
    const auto lease = read_application_identity<
        mutation_lease_instance_identity>(input);
    const auto backend =
        read_application_identity<mutation_backend_identity>(input);

    const auto attempt = application_attempt::make(
        request, target, backend,
        application_attempt_nonce::from_bytes(nonce_bytes));
    auto header = application_journal_header::make(
        kind, request, plan, attempt, target, control, std::move(state_projection),
        lease, backend);

    const auto state = read_state(input);
    const auto effect_count = read_count(
        input, maximum_effect_count, "application journal effect graph");
    std::vector<application_journal_effect> effects;
    effects.reserve(static_cast<std::size_t>(effect_count));
    for (std::uint64_t index = 0; index < effect_count; ++index) {
      const auto ordinal = input.read_u64();
      const auto effect_kind = read_effect_kind(input);
      std::optional<pkgplan::package_path> path;
      if (input.read_bool())
        path = pkgplan::package_path::parse(
            input.read_string(maximum_path_text_size));
      effects.push_back(application_journal_effect::make(
          ordinal, effect_kind, std::move(path)));
    }

    const auto event_count = read_count(
        input, maximum_event_count, "application journal event history");
    std::vector<application_journal_event> events;
    events.reserve(static_cast<std::size_t>(event_count));
    for (std::uint64_t index = 0; index < event_count; ++index) {
      const auto sequence = input.read_u64();
      const auto event_kind = read_event_kind(input);
      auto effect = read_application_identity<
          application_journal_effect_identity>(input);
      const auto evidence_count = read_count(
          input, maximum_evidence_count,
          "application journal backend evidence");
      std::vector<application_backend_evidence_identity> evidence;
      evidence.reserve(static_cast<std::size_t>(evidence_count));
      for (std::uint64_t item = 0; item < evidence_count; ++item) {
        evidence.push_back(read_application_identity<
            application_backend_evidence_identity>(input));
      }
      events.emplace_back(
          sequence, event_kind, std::move(effect), std::move(evidence));
    }

    std::optional<application_receipt_identity> receipt;
    if (input.read_bool())
      receipt = read_application_identity<application_receipt_identity>(input);
    std::optional<completed_application_evidence_identity> completed_evidence;
    if (input.read_bool()) {
      completed_evidence = read_application_identity<
          completed_application_evidence_identity>(input);
    }
    input.require_end();

    auto record = application_journal_record::make(
        std::move(header), state, std::move(effects), std::move(events),
        std::move(receipt), std::move(completed_evidence));
    if (record.identity() != expected_identity)
      throw application_journal_codec_error(
          application_journal_codec_error_code::identity_mismatch,
          "application journal encoding identity does not match its content");
    return record;
  } catch (const application_journal_codec_error&) {
    throw;
  } catch (const std::invalid_argument& error) {
    throw application_journal_codec_error(
        application_journal_codec_error_code::invalid_value,
        std::string("application journal encoding is invalid: ") + error.what());
  }
}

application_journal_record
decode_application_journal(const application_journal_encoding& encoding)
{
  return decode_application_journal(encoding.data(), encoding.size());
}

void
validate_application_journal_successor(
    const application_journal_record& previous,
    const application_journal_record& next)
{
  if (previous.header().identity() != next.header().identity())
    throw application_journal_transition_error(
        application_journal_transition_error_code::different_journal,
        "journal replacement belongs to another durable attempt");

  if (previous.effects().size() != next.effects().size())
    throw application_journal_transition_error(
        application_journal_transition_error_code::effect_graph_changed,
        "journal replacement changed the frozen effect graph");
  for (std::size_t index = 0; index < previous.effects().size(); ++index) {
    if (previous.effects()[index].identity() != next.effects()[index].identity())
      throw application_journal_transition_error(
          application_journal_transition_error_code::effect_graph_changed,
          "journal replacement changed the frozen effect graph");
  }

  if (next.events().size() < previous.events().size())
    throw application_journal_transition_error(
        application_journal_transition_error_code::event_history_rewritten,
        "journal replacement truncated its event history");
  for (std::size_t index = 0; index < previous.events().size(); ++index) {
    if (!same_event(previous.events()[index], next.events()[index]))
      throw application_journal_transition_error(
          application_journal_transition_error_code::event_history_rewritten,
          "journal replacement rewrote its event history");
  }

  if (previous.receipt() && previous.receipt() != next.receipt())
    throw application_journal_transition_error(
        application_journal_transition_error_code::resolution_regressed,
        "journal replacement changed or removed its receipt identity");
  if (previous.completed_evidence() &&
      previous.completed_evidence() != next.completed_evidence())
  {
    throw application_journal_transition_error(
        application_journal_transition_error_code::resolution_regressed,
        "journal replacement changed or removed completed evidence");
  }

  if (previous.receipt() && previous.identity() != next.identity())
    throw application_journal_transition_error(
        application_journal_transition_error_code::terminal_replaced,
        "terminal journal snapshot cannot be replaced");

  if (!state_may_follow(previous.state(), next.state()))
    throw application_journal_transition_error(
        application_journal_transition_error_code::state_regressed,
        "journal replacement regressed or skipped its execution state");
}

} // namespace pkgapply
