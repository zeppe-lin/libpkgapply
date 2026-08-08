// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/attempt.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
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

pkgapply::application_attempt_nonce
nonce(std::uint8_t value)
{
  pkgapply::application_attempt_nonce::byte_array bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<std::uint8_t>(value + index);
  return pkgapply::application_attempt_nonce::from_bytes(bytes);
}

} // namespace

int
main()
{
  const auto attempt = pkgapply::application_attempt::make(
      identity<pkgapply::application_request_identity>(1),
      identity<pkgapply::application_target_context_identity>(2),
      identity<pkgapply::mutation_backend_identity>(3),
      nonce(4));

  require(attempt.identity().string() == "v1:sha256:203366c29ebc454e4ab295fd790401270462929d008eafe1e17674512855d5cb",
          "application attempt identity vector changed");
  require(attempt.nonce() == nonce(4),
          "application attempt nonce was not retained");

  const auto changed_nonce = pkgapply::application_attempt::make(
      attempt.request(), attempt.target(), attempt.backend(), nonce(5));
  require(changed_nonce.identity() != attempt.identity(),
          "application attempt identity ignored backend nonce");

  const auto changed_backend = pkgapply::application_attempt::make(
      attempt.request(), attempt.target(),
      identity<pkgapply::mutation_backend_identity>(6), nonce(4));
  require(changed_backend.identity() != attempt.identity(),
          "application attempt identity ignored backend identity");

  return 0;
}
