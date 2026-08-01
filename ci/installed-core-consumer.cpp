// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/libpkgapply.h>

int main()
{
  const auto validator = &pkgapply::validate_target_mutation_lease_scope;
  const auto receipt_encoder = &pkgapply::encode_application_receipt;
  (void)validator;
  (void)receipt_encoder;
  return pkgapply::version() == "2.3.0" && pkgapply::api_version == 2 ? 0 : 1;
}
