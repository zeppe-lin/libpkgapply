// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <libpkgapply/digest.h>

namespace pkgapply::detail {

class identity_factory final {
public:
  template<class Identity>
  [[nodiscard]] static Identity
  from_sha256(const std::array<std::uint8_t, 32>& bytes)
  {
    static constexpr char hexadecimal[] = "0123456789abcdef";
    std::string value = "v1:sha256:";
    value.reserve(value.size() + bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
      value.push_back(hexadecimal[(byte >> 4) & 0x0f]);
      value.push_back(hexadecimal[byte & 0x0f]);
    }
    return Identity(std::move(value), bytes);
  }
};

} // namespace pkgapply::detail
