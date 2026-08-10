// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file libpkgapply.h
 *  \brief Complete public semantic application API.
 */
#pragma once

#include <libpkgapply/export.h>

#include <libpkgapply/digest.h>
#include <libpkgapply/admission.h>
#include <libpkgapply/apply.h>
#include <libpkgapply/application_receipt_codec.h>
#include <libpkgapply/attempt.h>
#include <libpkgapply/backend.h>
#include <libpkgapply/capture.h>
#include <libpkgapply/completed_evidence_codec.h>
#include <libpkgapply/execution_control.h>
#include <libpkgapply/incoming_package.h>
#include <libpkgapply/journal.h>
#include <libpkgapply/journal_codec.h>
#include <libpkgapply/mutation_lease.h>
#include <libpkgapply/object_fact.h>
#include <libpkgapply/path_consequence.h>
#include <libpkgapply/payload.h>
#include <libpkgapply/precondition.h>
#include <libpkgapply/request.h>
#include <libpkgapply/restart.h>
#include <libpkgapply/restart_checkpoint_codec.h>
#include <libpkgapply/result.h>
#include <libpkgapply/schedule.h>
#include <libpkgapply/state_projection.h>
#include <libpkgapply/target_context.h>
#include <libpkgapply/version.h>
