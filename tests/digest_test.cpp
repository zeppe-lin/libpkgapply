// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/digest.h>

#include <cstdlib>
#include <iostream>
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

template<class Function>
void
require_error(Function function, pkgapply::digest_error_code expected)
{
  try {
    function();
  } catch (const pkgapply::digest_error& error) {
    require(error.code() == expected, "unexpected digest error code");
    return;
  }
  require(false, "expected digest error");
}

} // namespace

int
main()
{
  constexpr std::string_view zero =
      "v1:sha256:0000000000000000000000000000000000000000000000000000000000000000";
  constexpr std::string_view one =
      "v1:sha256:0000000000000000000000000000000000000000000000000000000000000001";

  const auto attempt = pkgapply::application_attempt_identity::parse(zero);
  const auto same = pkgapply::application_attempt_identity::parse(zero);
  const auto later = pkgapply::application_attempt_identity::parse(one);
  const auto receipt = pkgapply::application_receipt_identity::parse(zero);

  require(attempt == same, "equal identity bytes must compare equal");
  require(attempt != later, "different identity bytes must differ");
  require(attempt < later, "identity order must use digest bytes");
  require(attempt.string() == zero, "identity representation must round trip");
  require(attempt.bytes().front() == 0 && attempt.bytes().back() == 0,
          "identity bytes were parsed incorrectly");
  require(receipt.string() == attempt.string(),
          "equal wire bytes should retain equal representation");
  require(pkgapply::application_attempt_identity::canonical_domain() !=
              pkgapply::application_receipt_identity::canonical_domain(),
          "identity domains must remain distinct");

  require_error(
      [] { static_cast<void>(pkgapply::application_attempt_identity::parse("sha256:00")); },
      pkgapply::digest_error_code::unsupported_version);
  require_error(
      [] {
        static_cast<void>(pkgapply::application_attempt_identity::parse(
            "v2:sha256:0000000000000000000000000000000000000000000000000000000000000000"));
      },
      pkgapply::digest_error_code::unsupported_version);
  require_error(
      [] {
        static_cast<void>(pkgapply::application_attempt_identity::parse(
            "v1:sha512:0000000000000000000000000000000000000000000000000000000000000000"));
      },
      pkgapply::digest_error_code::unsupported_algorithm);
  require_error(
      [] { static_cast<void>(pkgapply::application_attempt_identity::parse("v1:sha256:00")); },
      pkgapply::digest_error_code::invalid_length);
  require_error(
      [] {
        static_cast<void>(pkgapply::application_attempt_identity::parse(
            "v1:sha256:000000000000000000000000000000000000000000000000000000000000000G"));
      },
      pkgapply::digest_error_code::invalid_hexadecimal);
  require_error(
      [] {
        static_cast<void>(pkgapply::application_attempt_identity::parse(
            "v1:sha256:000000000000000000000000000000000000000000000000000000000000000A"));
      },
      pkgapply::digest_error_code::invalid_hexadecimal);

  return 0;
}
