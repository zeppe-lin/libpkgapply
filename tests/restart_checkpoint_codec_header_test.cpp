// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/restart_checkpoint_codec.h>

int main()
{
  static_assert(pkgapply::application_restart_checkpoint_encoding_version == 2);
  return 0;
}
