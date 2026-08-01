// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/application_receipt_codec.h>

#include "checkpoint_test_fixture.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {

using namespace pkgapply;
namespace fixture = pkgapply::test::checkpoint_fixture;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

application_durability_profile all(application_durability_status status)
{
  using D = application_durability_domain;
  return application_durability_profile({
      {D::journal, status},
      {D::incoming_staging, status},
      {D::recovery_staging, status},
      {D::active_namespace, status},
      {D::rejected_object_store, status},
      {D::completed_evidence, status},
  });
}

application_durability_profile visibility_unconfirmed()
{
  using D = application_durability_domain;
  using S = application_durability_status;
  return application_durability_profile({
      {D::journal, S::confirmed},
      {D::incoming_staging, S::confirmed},
      {D::recovery_staging, S::confirmed},
      {D::active_namespace, S::unconfirmed},
      {D::rejected_object_store, S::confirmed},
      {D::completed_evidence, S::not_attempted},
  });
}

application_path_consequence failed_path(
    const installation_application_request& request,
    application_effect_status status)
{
  const auto& decision = request.plan().paths().front();
  const auto before = application_path_observation::absent(decision.path());
  auto after = status == application_effect_status::completed
      ? application_path_observation::present(fixture::directory(decision.path()))
      : application_path_observation::unknown(decision.path());
  return application_path_consequence(
      decision.path(), application_path_role::incoming_entry,
      decision.active(), decision.rejected(), decision.incoming_entry(),
      decision.ownership(), status, application_effect_status::not_attempted,
      before, std::move(after), std::nullopt,
      ownership_publication_status::ineligible);
}

void check_round_trip(
    const application_receipt& receipt,
    const installation_application_request& request)
{
  const auto encoded = encode_application_receipt(receipt);
  const auto decoded = decode_application_receipt(encoded, request);
  require(decoded.identity() == receipt.identity(),
          "application receipt identity changed during decode");
  require(decoded.request() == receipt.request(),
          "application receipt request changed during decode");
  require(decoded.outcome() == receipt.outcome(),
          "application receipt outcome changed during decode");
  require(decoded.recovery() == receipt.recovery(),
          "application receipt recovery changed during decode");
  require(decoded.paths().size() == receipt.paths().size(),
          "application receipt path count changed during decode");
  for (std::size_t index = 0; index < receipt.paths().size(); ++index) {
    require(decoded.paths()[index].path() == receipt.paths()[index].path() &&
                decoded.paths()[index].active_status() ==
                    receipt.paths()[index].active_status() &&
                decoded.paths()[index].rejected_status() ==
                    receipt.paths()[index].rejected_status() &&
                decoded.paths()[index].publication() ==
                    receipt.paths()[index].publication(),
            "application receipt path consequence changed during decode");
  }
  require(decoded.durability() == receipt.durability(),
          "application receipt durability changed during decode");
  require(decoded.journal() == receipt.journal(),
          "application receipt journal changed during decode");
  require(decoded.backend_evidence() == receipt.backend_evidence(),
          "application receipt backend evidence changed during decode");
  require(decoded.completed_evidence().has_value() ==
              receipt.completed_evidence().has_value(),
          "application receipt completed-evidence presence changed");
  if (receipt.completed_evidence())
    require(decoded.completed_evidence()->identity() ==
                receipt.completed_evidence()->identity(),
            "application receipt completed evidence changed during decode");
  require(encode_application_receipt(decoded) == encoded,
          "application receipt encoding is not canonical");
}

} // namespace

int main()
{
  const auto request = fixture::request();
  const auto journal = fixture::journal(request);
  const auto checkpoint = fixture::checkpoint(request, journal);
  const auto completed = application_receipt::completed(
      *checkpoint.completed_evidence(),
      application_recovery_state::recovery_assets_retained,
      {fixture::application_identity<application_backend_evidence_identity>(90)});
  check_round_trip(completed, request);

  const auto refused = application_receipt::failed(
      request,
      fixture::application_identity<application_attempt_identity>(91),
      fixture::application_identity<lease_bound_state_projection_identity>(92),
      application_attempt_outcome::precondition_refused,
      application_recovery_state::unchanged,
      all(application_durability_status::not_attempted), {}, std::nullopt,
      {fixture::application_identity<application_backend_evidence_identity>(93)});
  check_round_trip(refused, request);

  const auto failed = application_receipt::failed(
      request,
      fixture::application_identity<application_attempt_identity>(94),
      fixture::application_identity<lease_bound_state_projection_identity>(95),
      application_attempt_outcome::failed_before_target_mutation,
      application_recovery_state::unchanged,
      all(application_durability_status::not_attempted),
      {failed_path(request, application_effect_status::failed)},
      journal.header().identity(),
      {fixture::application_identity<application_backend_evidence_identity>(96)});
  check_round_trip(failed, request);

  const auto uncertain = application_receipt::failed(
      request,
      fixture::application_identity<application_attempt_identity>(97),
      fixture::application_identity<lease_bound_state_projection_identity>(98),
      application_attempt_outcome::effects_visible_durability_unconfirmed,
      application_recovery_state::recovery_assets_retained,
      visibility_unconfirmed(),
      {failed_path(request, application_effect_status::completed)},
      journal.header().identity(),
      {fixture::application_identity<application_backend_evidence_identity>(99)});
  check_round_trip(uncertain, request);

  auto corrupt = encode_application_receipt(completed);
  require(corrupt.size() > 60, "application receipt encoding is too short");
  corrupt[60] ^= 0x80U;
  bool rejected = false;
  try {
    static_cast<void>(decode_application_receipt(corrupt, request));
  }
  catch (const application_receipt_codec_error& error) {
    rejected = error.code() ==
        application_receipt_codec_error_code::checksum_mismatch;
  }
  require(rejected, "application receipt codec accepted corruption");

  auto truncated = encode_application_receipt(failed);
  truncated.pop_back();
  rejected = false;
  try {
    static_cast<void>(decode_application_receipt(truncated, request));
  }
  catch (const application_receipt_codec_error& error) {
    rejected = error.code() == application_receipt_codec_error_code::truncated;
  }
  require(rejected, "application receipt codec accepted truncation");

  const auto foreign = fixture::request("other");
  rejected = false;
  try {
    static_cast<void>(decode_application_receipt(
        encode_application_receipt(completed), foreign));
  }
  catch (const application_receipt_codec_error& error) {
    rejected = error.code() ==
        application_receipt_codec_error_code::request_mismatch ||
        error.code() ==
        application_receipt_codec_error_code::completed_evidence_invalid;
  }
  require(rejected, "application receipt codec accepted a foreign request");

  return 0;
}
