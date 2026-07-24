// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "checkpoint_test_fixture.h"

#include <libpkgapply/restart_checkpoint_codec.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
}

int main()
{
  const auto request = pkgapply::test::checkpoint_fixture::request();
  const auto journal =
      pkgapply::test::checkpoint_fixture::journal(request);
  const auto checkpoint =
      pkgapply::test::checkpoint_fixture::checkpoint(request, journal);
  const auto encoding =
      pkgapply::encode_application_restart_checkpoint(checkpoint);
  const auto decoded =
      pkgapply::decode_application_restart_checkpoint(encoding, journal, request);

  require(decoded.journal() == checkpoint.journal(),
          "checkpoint codec changed journal binding");
  require(decoded.admitted_observations().requested() ==
              checkpoint.admitted_observations().requested(),
          "checkpoint codec changed admitted paths");
  require(decoded.active_effects().size() == 1 &&
              decoded.active_effects().front().result().outcome() ==
                  pkgapply::backend_operation_outcome::completed,
          "checkpoint codec changed active effect truth");
  require(decoded.completed_evidence().has_value() &&
              decoded.completed_evidence()->identity() ==
                  checkpoint.completed_evidence()->identity(),
          "checkpoint codec changed completed evidence");
  require(pkgapply::encode_application_restart_checkpoint(decoded) == encoding,
          "checkpoint codec is not byte stable");

  bool rejected = false;
  try {
    auto truncated = encoding;
    truncated.pop_back();
    static_cast<void>(
        pkgapply::decode_application_restart_checkpoint(truncated, journal, request));
  }
  catch (const pkgapply::application_restart_checkpoint_codec_error& error) {
    rejected = error.code() ==
        pkgapply::application_restart_checkpoint_codec_error_code::truncated;
  }
  require(rejected, "checkpoint codec accepted truncated bytes");

  rejected = false;
  try {
    auto corrupted = encoding;
    corrupted.back() ^= 0x01U;
    static_cast<void>(
        pkgapply::decode_application_restart_checkpoint(
            corrupted, journal, request));
  }
  catch (const pkgapply::application_restart_checkpoint_codec_error& error) {
    rejected = error.code() ==
        pkgapply::application_restart_checkpoint_codec_error_code::identity_mismatch;
  }
  require(rejected, "checkpoint codec accepted same-length corruption");

  rejected = false;
  try {
    const auto foreign =
        pkgapply::test::checkpoint_fixture::request("other");
    static_cast<void>(
        pkgapply::decode_application_restart_checkpoint(encoding, journal, foreign));
  }
  catch (const pkgapply::application_restart_checkpoint_codec_error& error) {
    rejected = error.code() ==
        pkgapply::application_restart_checkpoint_codec_error_code::request_mismatch;
  }
  require(rejected, "checkpoint codec accepted another request plan");

  return 0;
}
