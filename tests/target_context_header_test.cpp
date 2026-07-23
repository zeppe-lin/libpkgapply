// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/target_context.h>

int
main()
{
  return pkgapply::application_target_context_schema_version == 1 ? 0 : 1;
}
