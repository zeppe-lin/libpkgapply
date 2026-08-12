// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/journal_codec.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <openssl/evp.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}


std::string sha256_hex(const pkgapply::application_journal_encoding& bytes)
{
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  if (EVP_Digest(
          bytes.data(), bytes.size(), digest.data(), &size, EVP_sha256(),
          nullptr) != 1 ||
      size != 32)
  {
    throw std::runtime_error("cannot hash journal codec test vector");
  }
  constexpr char hexadecimal[] = "0123456789abcdef";
  std::string result;
  result.reserve(size * 2);
  for (unsigned int index = 0; index < size; ++index) {
    result += hexadecimal[(digest[index] >> 4) & 0x0fU];
    result += hexadecimal[digest[index] & 0x0fU];
  }
  return result;
}

template<class Identity>
Identity application_identity(std::uint8_t value)
{
  std::string text = "v1:sha256:";
  constexpr char hexadecimal[] = "0123456789abcdef";
  for (std::size_t index = 0; index < 32; ++index) {
    const auto byte = static_cast<std::uint8_t>(value + index);
    text += hexadecimal[(byte >> 4) & 0x0fU];
    text += hexadecimal[byte & 0x0fU];
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

pkgapply::application_journal_header header(std::uint8_t seed = 1)
{
  pkgapply::application_attempt_nonce::byte_array nonce{};
  for (std::size_t index = 0; index < nonce.size(); ++index)
    nonce[index] = static_cast<std::uint8_t>(seed + 20 + index);
  const auto request =
      application_identity<pkgapply::application_request_identity>(seed);
  const auto target = application_identity<
      pkgapply::application_target_context_identity>(seed + 1);
  const auto backend =
      application_identity<pkgapply::mutation_backend_identity>(seed + 2);
  const auto attempt = pkgapply::application_attempt::make(
      request, target, backend,
      pkgapply::application_attempt_nonce::from_bytes(nonce));
  return pkgapply::application_journal_header::make(
      pkgplan::operation_kind::install,
      request,
      planning_identity<pkgplan::operation_plan_identity>(seed + 3),
      attempt,
      target,
      application_identity<
          pkgapply::application_execution_control_identity>(seed + 4),
      pkgapply::lease_bound_state_projection::make(
          application_identity<
              pkgapply::mutation_lease_instance_identity>(seed + 6),
          planning_identity<pkgplan::installed_state_snapshot_identity>(
              seed + 5),
          planning_identity<pkgplan::ownership_inventory_identity>(seed + 7),
          pkgapply::state_projection_completeness::complete,
          {pkgapply::projected_path_owners(
              pkgplan::package_path::parse("usr/bin/tool"),
              {planning_identity<pkgplan::installed_package_identity>(seed + 9),
               planning_identity<pkgplan::installed_package_identity>(
                   seed + 10)}),
           pkgapply::projected_path_owners(
               pkgplan::package_path::parse("etc/tool.conf"), {})},
          application_identity<pkgapply::state_projection_evidence_identity>(
              seed + 8)),
      application_identity<
          pkgapply::mutation_lease_instance_identity>(seed + 6),
      backend);
}

std::vector<pkgapply::application_journal_effect> effects()
{
  return {
      pkgapply::application_journal_effect::make(
          0,
          pkgapply::application_journal_effect_kind::publish_active_object,
          pkgplan::package_path::parse("usr/bin/tool")),
      pkgapply::application_journal_effect::make(
          1,
          pkgapply::application_journal_effect_kind::synchronize_journal),
  };
}

pkgapply::application_journal_record initial_record()
{
  return pkgapply::application_journal_record::make(
      header(), pkgapply::application_journal_state::preparing, effects(), {});
}

pkgapply::application_journal_record prepared_record()
{
  return pkgapply::application_journal_record::make(
      header(), pkgapply::application_journal_state::prepared, effects(), {});
}

pkgapply::application_journal_record intended_record()
{
  const auto graph = effects();
  return pkgapply::application_journal_record::make(
      header(), pkgapply::application_journal_state::mutating, graph,
      {{0, pkgapply::application_journal_event_kind::intent,
        graph[0].identity()}});
}

pkgapply::application_journal_record completed_record()
{
  const auto graph = effects();
  return pkgapply::application_journal_record::make(
      header(), pkgapply::application_journal_state::mutating, graph,
      {{0, pkgapply::application_journal_event_kind::intent,
        graph[0].identity()},
       {1, pkgapply::application_journal_event_kind::completed,
        graph[0].identity(),
        {application_identity<
            pkgapply::application_backend_evidence_identity>(70)}}});
}

pkgapply::application_journal_record terminal_record(
    pkgapply::application_journal_state state)
{
  const auto graph = effects();
  return pkgapply::application_journal_record::make(
      header(), state, graph,
      {{0, pkgapply::application_journal_event_kind::intent,
        graph[0].identity()},
       {1, pkgapply::application_journal_event_kind::completed,
        graph[0].identity()},
       {2, pkgapply::application_journal_event_kind::intent,
        graph[1].identity()},
       {3, pkgapply::application_journal_event_kind::completed,
        graph[1].identity()}},
      application_identity<pkgapply::application_receipt_identity>(80),
      application_identity<
          pkgapply::completed_application_evidence_identity>(81));
}

} // namespace

int main()
{
  const auto completed = completed_record();
  const auto encoding = pkgapply::encode_application_journal(completed);
  require(encoding.size() == 1604,
          "journal wire-format test vector size changed");
  require(sha256_hex(encoding) ==
              "6db0014279272e3776ee4aa5c20e5a7de6ee33b34ec2c8748d0aa5c688875032",
          "journal wire-format test vector changed");
  const auto decoded = pkgapply::decode_application_journal(encoding);
  require(decoded.identity() == completed.identity(),
          "journal codec changed the record identity");
  require(decoded.header().identity() == completed.header().identity(),
          "journal codec changed the header identity");
  require(
      decoded.header().admitted_state_projection().identity() ==
          completed.header().admitted_state_projection().identity() &&
      decoded.header().admitted_state_projection().lease() ==
          completed.header().admitted_state_projection().lease() &&
      decoded.header().admitted_state_projection().snapshot() ==
          completed.header().admitted_state_projection().snapshot() &&
      decoded.header().admitted_state_projection().ownership_inventory() ==
          completed.header().admitted_state_projection().ownership_inventory() &&
      decoded.header().admitted_state_projection().paths() ==
          completed.header().admitted_state_projection().paths() &&
      decoded.header().admitted_state_projection().evidence() ==
          completed.header().admitted_state_projection().evidence(),
      "journal codec lost the admitted state-projection body");
  require(decoded.events().size() == completed.events().size() &&
              decoded.events()[1].backend_evidence() ==
                  completed.events()[1].backend_evidence(),
          "journal codec lost event evidence");
  require(pkgapply::encode_application_journal(decoded) == encoding,
          "journal codec is not byte-stable");

  pkgapply::validate_application_journal_successor(
      initial_record(), prepared_record());
  pkgapply::validate_application_journal_successor(
      prepared_record(), intended_record());
  pkgapply::validate_application_journal_successor(
      intended_record(), completed_record());
  pkgapply::validate_application_journal_successor(completed, completed);
  const auto terminal = terminal_record(
      pkgapply::application_journal_state::application_completed);
  pkgapply::validate_application_journal_successor(terminal, terminal);

  bool rejected = false;
  try {
    pkgapply::validate_application_journal_successor(
        terminal, terminal_record(pkgapply::application_journal_state::finalized));
  } catch (const pkgapply::application_journal_transition_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_transition_error_code::
                   terminal_replaced;
  }
  require(rejected, "journal transition replaced a terminal snapshot");

  rejected = false;
  try {
    pkgapply::validate_application_journal_successor(
        prepared_record(), initial_record());
  } catch (const pkgapply::application_journal_transition_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_transition_error_code::
                   state_regressed;
  }
  require(rejected, "journal transition accepted state regression");

  rejected = false;
  try {
    pkgapply::validate_application_journal_successor(
        completed_record(), intended_record());
  } catch (const pkgapply::application_journal_transition_error& error) {
    rejected =
        error.code() == pkgapply::application_journal_transition_error_code::
                            event_history_rewritten;
  }
  require(rejected, "journal transition accepted truncated event history");

  rejected = false;
  try {
    pkgapply::validate_application_journal_successor(
        completed_record(),
        pkgapply::application_journal_record::make(
            header(9), pkgapply::application_journal_state::preparing,
            effects(), {}));
  } catch (const pkgapply::application_journal_transition_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_transition_error_code::
                   different_journal;
  }
  require(rejected, "journal transition accepted another durable attempt");

  auto bad_magic = encoding;
  bad_magic[0] ^= 0xffU;
  rejected = false;
  try {
    static_cast<void>(pkgapply::decode_application_journal(bad_magic));
  } catch (const pkgapply::application_journal_codec_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_codec_error_code::invalid_magic;
  }
  require(rejected, "journal codec accepted invalid magic");

  auto truncated = encoding;
  truncated.pop_back();
  rejected = false;
  try {
    static_cast<void>(pkgapply::decode_application_journal(truncated));
  } catch (const pkgapply::application_journal_codec_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_codec_error_code::truncated;
  }
  require(rejected, "journal codec accepted truncated input");

  auto trailing = encoding;
  trailing.push_back(0);
  rejected = false;
  try {
    static_cast<void>(pkgapply::decode_application_journal(trailing));
  } catch (const pkgapply::application_journal_codec_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_codec_error_code::trailing_data;
  }
  require(rejected, "journal codec accepted trailing input");

  auto mismatched_identity = encoding;
  constexpr std::size_t identity_text_offset = 8 + 2 + 8;
  constexpr std::size_t last_identity_character =
      identity_text_offset + sizeof("v1:sha256:") - 2 + 63;
  mismatched_identity[last_identity_character] =
      mismatched_identity[last_identity_character] == '0' ? '1' : '0';
  rejected = false;
  try {
    static_cast<void>(
        pkgapply::decode_application_journal(mismatched_identity));
  } catch (const pkgapply::application_journal_codec_error& error) {
    rejected = error.code() ==
               pkgapply::application_journal_codec_error_code::
                   identity_mismatch;
  }
  require(rejected, "journal codec accepted a mismatched record identity");

  return 0;
}
