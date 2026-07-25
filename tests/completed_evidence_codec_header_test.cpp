// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/completed_evidence_codec.h>

int main()
{
  return pkgapply::completed_application_evidence_encoding_version == 1
      ? 0
      : 1;
}
