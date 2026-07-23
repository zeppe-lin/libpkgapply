// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <libpkgapply/digest.h>

namespace pkgapply::detail {

class canonical_record final {
public:
  explicit canonical_record(std::string_view domain);

  void append_u8(std::uint8_t value);
  void append_u16(std::uint16_t value);
  void append_u32(std::uint32_t value);
  void append_u64(std::uint64_t value);
  void append_bool(bool value);
  void append_bytes(std::string_view value);

  template<class Domain>
  void append_digest(const typed_digest<Domain>& value)
  {
    append_u8(static_cast<std::uint8_t>(value.algorithm()));
    for (const std::uint8_t byte : value.bytes())
      bytes_.push_back(static_cast<std::byte>(byte));
  }

  [[nodiscard]] std::array<std::uint8_t, 32> sha256() const;
  [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept;

private:
  std::vector<std::byte> bytes_;
};

} // namespace pkgapply::detail
