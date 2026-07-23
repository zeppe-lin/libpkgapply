// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgapply/attempt.h>
int main() { return pkgapply::application_attempt_schema_version == 1 ? 0 : 1; }
