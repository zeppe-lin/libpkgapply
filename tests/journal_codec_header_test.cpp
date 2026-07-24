// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgapply/journal_codec.h>
int main()
{
  return pkgapply::application_journal_encoding_version == 1 ? 0 : 1;
}
