// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file version.h
 *  \brief Public API and library release versions.
 */
#pragma once

#include <libpkgapply/export.h>

#include <cstdint>
#include <string_view>

namespace pkgapply {

/*! \brief Version of the public semantic application API. */
inline constexpr std::uint32_t api_version = 4;

/*! \brief Return the linked libpkgapply release version.
 *  \return Static semantic-version string with process lifetime.
 */
[[nodiscard]] PKGAPPLY_API std::string_view version() noexcept;

} // namespace pkgapply
