// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgapply/state_projection.h>
int main() { return pkgapply::lease_bound_state_projection_schema_version == 1 ? 0 : 1; }
