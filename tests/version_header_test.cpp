// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/version.h>

int
main()
{
  return pkgapply::version().empty() ? 1 : 0;
}
