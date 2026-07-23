// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/digest.h>

int
main()
{
  return pkgapply::application_attempt_identity::canonical_domain().empty();
}
