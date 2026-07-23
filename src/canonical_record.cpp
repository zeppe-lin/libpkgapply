// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "canonical_record.h"

#include "sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pkgapply::detail {
namespace {

constexpr std::array<std::uint8_t, 8> magic = {
  'Z', 'L', 'A', 'P', 'P', 'L', 'Y', 0,
};
constexpr std::uint16_t encoding_version = 1;

} // namespace

canonical_record::canonical_record(std::string_view domain)
{
  bytes_.reserve(magic.size() + 2 + 8 + domain.size());
  for (const std::uint8_t byte : magic)
    bytes_.push_back(static_cast<std::byte>(byte));
  append_u16(encoding_version);
  append_bytes(domain);
}

void
canonical_record::append_u8(std::uint8_t value)
{
  bytes_.push_back(static_cast<std::byte>(value));
}

void
canonical_record::append_u16(std::uint16_t value)
{
  append_u8(static_cast<std::uint8_t>((value >> 8) & 0xff));
  append_u8(static_cast<std::uint8_t>(value & 0xff));
}

void
canonical_record::append_u32(std::uint32_t value)
{
  append_u8(static_cast<std::uint8_t>((value >> 24) & 0xff));
  append_u8(static_cast<std::uint8_t>((value >> 16) & 0xff));
  append_u8(static_cast<std::uint8_t>((value >> 8) & 0xff));
  append_u8(static_cast<std::uint8_t>(value & 0xff));
}

void
canonical_record::append_u64(std::uint64_t value)
{
  for (int shift = 56; shift >= 0; shift -= 8)
    append_u8(static_cast<std::uint8_t>((value >> shift) & 0xff));
}

void
canonical_record::append_bool(bool value)
{
  append_u8(value ? 1 : 0);
}

void
canonical_record::append_bytes(std::string_view value)
{
  append_u64(static_cast<std::uint64_t>(value.size()));
  for (const char byte : value)
    bytes_.push_back(static_cast<std::byte>(static_cast<unsigned char>(byte)));
}

std::array<std::uint8_t, 32>
canonical_record::sha256() const
{
  return detail::sha256(bytes_);
}

const std::vector<std::byte>&
canonical_record::bytes() const noexcept
{
  return bytes_;
}

} // namespace pkgapply::detail
