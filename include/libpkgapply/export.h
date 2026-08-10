// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/** \file export.h
 *  \brief Public shared-library visibility annotation.
 */

#pragma once

#if defined(_WIN32)
#if defined(PKGAPPLY_BUILDING_LIBRARY)
#define PKGAPPLY_API __declspec(dllexport)
#else
#define PKGAPPLY_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define PKGAPPLY_API __attribute__((visibility("default")))
#else
#define PKGAPPLY_API
#endif
