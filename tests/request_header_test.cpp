// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgapply/request.h>
int main() { return pkgapply::application_request_schema_version == 2 ? 0 : 1; }
