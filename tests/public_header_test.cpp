// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/libpkgapply.h>

int
main()
{
  return pkgapply::version() == "1.0.0" && pkgapply::api_version == 1 ? 0 : 1;
}
