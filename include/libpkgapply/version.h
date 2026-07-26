// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string_view>

namespace pkgapply {

inline constexpr std::uint32_t api_version = 1;

[[nodiscard]] std::string_view version() noexcept;

} // namespace pkgapply
