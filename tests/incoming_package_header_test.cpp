// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/incoming_package.h>

int main()
{
  return pkgapply::incoming_package_authority_schema_version == 1 ? 0 : 1;
}
