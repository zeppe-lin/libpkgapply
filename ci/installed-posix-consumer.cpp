// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply-posix/libpkgapply-posix.h>
#include <libpkgapply/version.h>

#include <type_traits>

static_assert(std::is_base_of_v<
    pkgapply::application_backend,
    pkgapply::posix::application_posix_backend>);

int main()
{
  return pkgapply::version() == "0.1.0" ? 0 : 1;
}
