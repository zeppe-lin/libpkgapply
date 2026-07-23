// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pkgapply::detail {

[[nodiscard]] std::array<std::uint8_t, 32>
sha256(const std::byte* data, std::size_t size);

[[nodiscard]] std::array<std::uint8_t, 32>
sha256(const std::vector<std::byte>& data);

} // namespace pkgapply::detail
