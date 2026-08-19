// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal_transport_codec.h>

#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool value, std::string_view message)
{
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template<class Identity>
Identity application_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const auto byte = static_cast<std::uint8_t>(value + index);
    text += hex[(byte >> 4) & 0x0fU];
    text += hex[byte & 0x0fU];
  }
  return Identity::parse(text);
}

template<class Identity>
Identity planning_identity(std::uint8_t value)
{
  std::array<std::uint8_t, 32> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return Identity::from_sha256(bytes);
}

pkgapply::application_journal_header make_header()
{
  pkgapply::application_attempt_nonce::byte_array nonce{};
  for (std::size_t index = 0; index < nonce.size(); ++index)
    nonce[index] = static_cast<std::uint8_t>(31 + index);
  const auto request = application_identity<pkgapply::application_request_identity>(1);
  const auto target = application_identity<pkgapply::application_target_context_identity>(2);
  const auto backend = application_identity<pkgapply::mutation_backend_identity>(3);
  const auto lease = application_identity<pkgapply::mutation_lease_instance_identity>(4);
  const auto attempt = pkgapply::application_attempt::make(
      request, target, backend,
      pkgapply::application_attempt_nonce::from_bytes(nonce));
  return pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install, request,
      planning_identity<pkgplan::operation_plan_identity>(5), attempt, target,
      application_identity<pkgapply::application_execution_control_identity>(6),
      pkgapply::lease_bound_state_projection::make(
          lease,
          planning_identity<pkgplan::installed_state_snapshot_identity>(7),
          planning_identity<pkgplan::ownership_inventory_identity>(8),
          pkgapply::state_projection_completeness::complete,
          {pkgapply::projected_path_owners(
              pkgplan::package_path::parse("usr/bin/tool"),
              {planning_identity<pkgplan::installed_package_identity>(10)})},
          application_identity<pkgapply::state_projection_evidence_identity>(9)),
      lease, backend);
}

std::vector<pkgapply::application_journal_effect> make_effects()
{
  return {
      pkgapply::application_journal_effect::make(
          0, pkgapply::application_journal_effect_kind::publish_active_object,
          pkgplan::package_path::parse("usr/bin/tool")),
      pkgapply::application_journal_effect::make(
          1, pkgapply::application_journal_effect_kind::observe_result,
          pkgplan::package_path::parse("usr/bin/tool")),
  };
}

std::string sha256_hex(const pkgapply::application_journal_transport_encoding& bytes)
{
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  require(EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size,
                     EVP_sha256(), nullptr) == 1,
          "cannot hash transport-codec test vector");
  require(size == 32, "unexpected SHA-256 output size");
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (unsigned int index = 0; index < size; ++index)
    output << std::setw(2) << static_cast<unsigned int>(digest[index]);
  return output.str();
}

template<class Function>
bool rejects_with(Function&& function,
                  pkgapply::application_journal_transport_codec_error_code code)
{
  try {
    function();
  } catch (const pkgapply::application_journal_transport_codec_error& error) {
    return error.code() == code;
  }
  return false;
}

} // namespace

int main()
{
  const auto effects = make_effects();
  const pkgapply::application_journal_replay_encoding replay_seed{
      std::byte{0x00}, std::byte{0x7f}, std::byte{0x80}, std::byte{0xff}};
  const auto declaration = pkgapply::application_journal_declaration::make(
      make_header(), effects, replay_seed);

  const auto declaration_bytes =
      pkgapply::encode_application_journal_declaration(declaration);
  const auto decoded_declaration =
      pkgapply::decode_application_journal_declaration(declaration_bytes);
  require(decoded_declaration.identity() == declaration.identity() &&
              decoded_declaration.replay_seed() == replay_seed &&
              decoded_declaration.effects().size() == effects.size(),
          "declaration transport codec changed owner authority");

  auto cursor = pkgapply::application_journal_cursor::initial(declaration);
  const auto initial_cursor_bytes =
      pkgapply::encode_application_journal_cursor(cursor);
  const auto decoded_initial_cursor =
      pkgapply::decode_application_journal_cursor(initial_cursor_bytes);
  require(decoded_initial_cursor.identity() == cursor.identity() &&
              decoded_initial_cursor.step_count() == 0 &&
              !decoded_initial_cursor.latest_step() &&
              decoded_initial_cursor.state() ==
                  pkgapply::application_journal_state::preparing,
          "cursor transport codec changed the empty bounded head");
  const auto prepared = pkgapply::application_journal_step::make(
      declaration.identity(), 0, std::nullopt,
      pkgapply::application_journal_state::prepared);
  cursor = pkgapply::application_journal_cursor::advance(cursor, prepared);
  const auto intent = pkgapply::application_journal_step::make(
      declaration.identity(), 1, prepared.identity(),
      pkgapply::application_journal_state::mutating,
      pkgapply::application_journal_event(
          0, pkgapply::application_journal_event_kind::intent,
          effects[0].identity()));
  cursor = pkgapply::application_journal_cursor::advance(cursor, intent);
  const pkgapply::application_journal_replay_encoding replay_fact{
      std::byte{0xde}, std::byte{0xad}, std::byte{0x00}, std::byte{0xbe},
      std::byte{0xef}};
  const auto completed = pkgapply::application_journal_step::make(
      declaration.identity(), 2, intent.identity(),
      pkgapply::application_journal_state::effects_visible,
      pkgapply::application_journal_event(
          1, pkgapply::application_journal_event_kind::completed,
          effects[0].identity(),
          {application_identity<pkgapply::application_backend_evidence_identity>(20)}),
      replay_fact);
  cursor = pkgapply::application_journal_cursor::advance(cursor, completed);

  const auto step_bytes = pkgapply::encode_application_journal_step(completed);
  const auto decoded_step = pkgapply::decode_application_journal_step(step_bytes);
  require(decoded_step.identity() == completed.identity() &&
              decoded_step.declaration() == declaration.identity() &&
              decoded_step.sequence() == completed.sequence() &&
              decoded_step.predecessor() == completed.predecessor() &&
              decoded_step.replay_fact() == replay_fact &&
              decoded_step.event() &&
              decoded_step.event()->backend_evidence() ==
                  completed.event()->backend_evidence(),
          "step transport codec changed owner authority");

  const auto cursor_bytes = pkgapply::encode_application_journal_cursor(cursor);
  const auto decoded_cursor = pkgapply::decode_application_journal_cursor(cursor_bytes);
  require(decoded_cursor.identity() == cursor.identity() &&
              decoded_cursor.declaration() == cursor.declaration() &&
              decoded_cursor.step_count() == cursor.step_count() &&
              decoded_cursor.latest_step() == cursor.latest_step() &&
              decoded_cursor.state() == cursor.state(),
          "cursor transport codec changed bounded head authority");

  auto corrupted = step_bytes;
  require(corrupted.size() > 3, "step vector is unexpectedly small");
  corrupted[corrupted.size() - 3] ^= 0x01U;
  require(rejects_with(
              [&]() { (void)pkgapply::decode_application_journal_step(corrupted); },
              pkgapply::application_journal_transport_codec_error_code::identity_mismatch),
          "step transport codec accepted identity-changing bytes");

  auto trailing = cursor_bytes;
  trailing.push_back(0);
  require(rejects_with(
              [&]() { (void)pkgapply::decode_application_journal_cursor(trailing); },
              pkgapply::application_journal_transport_codec_error_code::trailing_data),
          "cursor transport codec accepted trailing bytes");

  auto wrong_magic = declaration_bytes;
  wrong_magic[0] ^= 0x01U;
  require(rejects_with(
              [&]() { (void)pkgapply::decode_application_journal_declaration(wrong_magic); },
              pkgapply::application_journal_transport_codec_error_code::invalid_magic),
          "declaration transport codec accepted foreign magic");

  require(rejects_with(
              [&]() {
                (void)pkgapply::decode_application_journal_step(
                    step_bytes.data(), step_bytes.size() - 1);
              },
              pkgapply::application_journal_transport_codec_error_code::truncated),
          "step transport codec accepted truncated bytes");

  auto unsupported_version = declaration_bytes;
  require(unsupported_version.size() > 9,
          "declaration vector is unexpectedly small");
  unsupported_version[9] = 2;
  require(rejects_with(
              [&]() {
                (void)pkgapply::decode_application_journal_declaration(
                    unsupported_version);
              },
              pkgapply::application_journal_transport_codec_error_code::
                  unsupported_version),
          "declaration transport codec accepted another wire generation");

  auto impossible_empty_cursor = initial_cursor_bytes;
  require(impossible_empty_cursor.size() > 3,
          "initial cursor vector is unexpectedly small");
  impossible_empty_cursor[impossible_empty_cursor.size() - 3] = 2;
  require(rejects_with(
              [&]() {
                (void)pkgapply::decode_application_journal_cursor(
                    impossible_empty_cursor);
              },
              pkgapply::application_journal_transport_codec_error_code::
                  invalid_value),
          "cursor transport codec accepted a non-preparing empty head");

  auto invalid_cursor_state = cursor_bytes;
  require(invalid_cursor_state.size() > 3,
          "cursor vector is unexpectedly small");
  invalid_cursor_state[invalid_cursor_state.size() - 3] = 0xffU;
  require(rejects_with(
              [&]() {
                (void)pkgapply::decode_application_journal_cursor(
                    invalid_cursor_state);
              },
              pkgapply::application_journal_transport_codec_error_code::
                  invalid_value),
          "cursor transport codec accepted an invalid lifecycle state");

  require(rejects_with(
              [&]() {
                (void)pkgapply::decode_application_journal_cursor(nullptr, 1);
              },
              pkgapply::application_journal_transport_codec_error_code::truncated),
          "cursor transport codec accepted missing byte storage");
  require(rejects_with(
              [&]() {
                (void)pkgapply::decode_application_journal_cursor(
                    nullptr,
                    pkgapply::maximum_application_journal_transport_encoding_size +
                        1U);
              },
              pkgapply::application_journal_transport_codec_error_code::
                  limit_exceeded),
          "cursor transport codec accepted an oversized byte extent");

  require(declaration_bytes.size() == 1234U &&
              sha256_hex(declaration_bytes) ==
                  "efa52893e06e0589e0f17f69417bfb93f5e67e1f6aff77ccfbb923cac62f0b3f",
          "declaration transport wire-format vector changed");
  require(step_bytes.size() == 465U &&
              sha256_hex(step_bytes) ==
                  "cf7a33a08b48fe3a1fbd87608a43b2d597168ff7fabf8998893c866063b22d8f",
          "step transport wire-format vector changed");
  require(cursor_bytes.size() == 270U &&
              sha256_hex(cursor_bytes) ==
                  "722a2c9ff8c2a57eec31fefdb8f737e7892a33b57fe851e2b977b425399abe03",
          "cursor transport wire-format vector changed");

  return 0;
}
