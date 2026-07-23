// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/canonical_record.h"

#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
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

std::string
hexadecimal(const std::array<std::uint8_t, 32>& digest)
{
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const std::uint8_t byte : digest)
    output << std::setw(2) << static_cast<unsigned int>(byte);
  return output.str();
}

} // namespace

int
main()
{
  pkgapply::detail::canonical_record record("pkgapply/test-vector/v1");
  record.append_u8(0x12);
  record.append_u16(0x3456);
  record.append_u32(0x789abcde);
  record.append_u64(0x0102030405060708ULL);
  record.append_bool(true);
  record.append_bool(false);
  record.append_bytes("abc");
  record.append_digest(pkgapply::application_attempt_identity::parse(
      "v1:sha256:000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"));

  require(hexadecimal(record.sha256()) ==
              "13279c274674e5b6f06fbc5978f8c1430dd17563ec6468551086c9cf170dd3e4",
          "canonical record vector changed");

  pkgapply::detail::canonical_record changed("pkgapply/test-vector/v1");
  changed.append_u8(0x13);
  require(record.sha256() != changed.sha256(),
          "canonical record must be field-sensitive");

  pkgapply::detail::canonical_record other_domain("pkgapply/other-vector/v1");
  other_domain.append_u8(0x12);
  require(record.sha256() != other_domain.sha256(),
          "canonical record must be domain-separated");

  return 0;
}
