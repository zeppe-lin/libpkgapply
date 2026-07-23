// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/path_consequence.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void
require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template<class Identity>
Identity
identity(std::uint8_t value)
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

pkgapply::completed_object_fact
directory(const pkgplan::package_path& path)
{
  return pkgapply::completed_object_fact(
      path,
      pkgapply::completed_object_kind::directory,
      pkgapply::qualified_fact<std::uint32_t>::known(0755),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::known(0),
      pkgapply::qualified_fact<std::uint64_t>::not_applicable(),
      pkgapply::qualified_fact<pkgapply::completed_object_timestamp>::unknown(),
      pkgapply::qualified_fact<
          pkgapply::completed_regular_content_identity>::not_applicable(),
      pkgapply::qualified_fact<std::string>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_device_number>::not_applicable(),
      pkgapply::qualified_fact<
          pkgapply::completed_hardlink_relation>::not_applicable(),
      pkgapply::object_fact_provenance::application_observation,
      pkgapply::object_fact_completeness::complete);
}

pkgplan::path_ownership_transition
ownership()
{
  return pkgplan::path_ownership_transition({}, {}, true);
}

} // namespace

int
main()
{
  const pkgplan::package_path path =
      pkgplan::package_path::parse("usr/share/tool");

  const pkgapply::application_path_consequence consequence(
      path,
      pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::none,
      pkgimage::entry_id{4},
      ownership(),
      pkgapply::application_effect_status::completed,
      pkgapply::application_effect_status::not_attempted,
      pkgapply::application_path_observation::absent(path),
      pkgapply::application_path_observation::present(directory(path)),
      std::nullopt,
      pkgapply::ownership_publication_status::eligible);

  require(consequence.incoming_entry() == pkgimage::entry_id{4},
          "incoming entry binding changed");

  bool rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::structural_parent,
        pkgplan::planned_active_outcome::retain_observed,
        pkgplan::planned_rejected_outcome::none,
        pkgimage::entry_id{4},
        ownership(),
        pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::present(directory(path)),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "non-incoming path accepted an incoming entry");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::incoming_entry,
        pkgplan::planned_active_outcome::activate_incoming,
        pkgplan::planned_rejected_outcome::stage_incoming,
        pkgimage::entry_id{4},
        ownership(),
        pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::completed,
        pkgapply::application_path_observation::absent(path),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "completed rejected consequence accepted no record");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::incoming_entry,
        pkgplan::planned_active_outcome::activate_incoming,
        pkgplan::planned_rejected_outcome::none,
        pkgimage::entry_id{4},
        ownership(),
        pkgapply::application_effect_status::conditional_retained,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::absent(path),
        pkgapply::application_path_observation::present(directory(path)),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "non-directory effect accepted conditional retention");

  rejected = false;
  try {
    static_cast<void>(pkgapply::application_path_consequence(
        path,
        pkgapply::application_path_role::incoming_entry,
        pkgplan::planned_active_outcome::activate_incoming,
        pkgplan::planned_rejected_outcome::none,
        pkgimage::entry_id{4},
        ownership(),
        pkgapply::application_effect_status::completed,
        pkgapply::application_effect_status::not_attempted,
        pkgapply::application_path_observation::absent(path),
        pkgapply::application_path_observation::unknown(path),
        std::nullopt,
        pkgapply::ownership_publication_status::eligible));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "publication accepted unknown resulting observation");

  const auto rejected_record =
      identity<pkgapply::rejected_object_record_identity>(80);
  const pkgapply::application_path_consequence staged(
      path,
      pkgapply::application_path_role::incoming_entry,
      pkgplan::planned_active_outcome::activate_incoming,
      pkgplan::planned_rejected_outcome::stage_incoming,
      pkgimage::entry_id{4},
      ownership(),
      pkgapply::application_effect_status::completed,
      pkgapply::application_effect_status::completed,
      pkgapply::application_path_observation::absent(path),
      pkgapply::application_path_observation::present(directory(path)),
      rejected_record,
      pkgapply::ownership_publication_status::eligible);
  require(staged.rejected_object() == rejected_record,
          "completed rejected record binding changed");

  return 0;
}
