// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/admission.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace pkgapply {
namespace {

[[noreturn]] void
refuse(application_admission_error_code code,
       const char* message,
       std::vector<pkgplan::package_path> paths = {})
{
  throw application_admission_error(code, message, std::move(paths));
}

void
validate_backend(const application_target_context& target,
                 const application_backend& backend)
{
  if (backend.identity() != target.mutation_backend())
    refuse(application_admission_error_code::backend_identity_mismatch,
           "application backend does not match target context");
  if (backend.observation_identity() != target.observation_backend())
    refuse(application_admission_error_code::observation_backend_mismatch,
           "observation backend does not match target context");
  if (backend.capabilities() != target.capabilities())
    refuse(application_admission_error_code::capability_profile_mismatch,
           "backend capabilities do not match target context");
}

void
validate_state(const pkgplan::operation_preconditions& preconditions,
               const lease_bound_state_projection& state)
{
  if (state.completeness() != state_projection_completeness::complete)
    refuse(application_admission_error_code::incomplete_state_projection,
           "application state projection is incomplete");
  if (state.snapshot() != preconditions.installed_snapshot())
    refuse(application_admission_error_code::installed_snapshot_mismatch,
           "installed snapshot does not match accepted plan");
  if (state.ownership_inventory() != preconditions.ownership_inventory())
    refuse(application_admission_error_code::ownership_inventory_mismatch,
           "ownership inventory does not match accepted plan");

  const auto& expected = preconditions.paths();
  const auto& actual = state.paths();
  if (expected.size() != actual.size()) {
    std::vector<pkgplan::package_path> paths;
    paths.reserve(expected.size() + actual.size());
    for (const auto& item : expected)
      paths.push_back(item.path());
    for (const auto& item : actual)
      paths.push_back(item.path());
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    refuse(application_admission_error_code::state_path_universe_mismatch,
           "state projection does not cover the exact plan path universe",
           std::move(paths));
  }

  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (expected[index].path() != actual[index].path())
      refuse(application_admission_error_code::state_path_universe_mismatch,
             "state projection path differs from accepted plan",
             {expected[index].path(), actual[index].path()});
    if (expected[index].owners() != actual[index].owners())
      refuse(application_admission_error_code::state_path_owners_mismatch,
             "state projection owners differ from accepted plan",
             {expected[index].path()});
  }
}

template<class Request>
void
validate_common(const Request& request,
                pkgplan::operation_kind expected_kind,
                const lease_bound_state_projection& state,
                const target_mutation_lease& lease,
                const application_backend& backend)
{
  if (request.schema_version() != application_request_schema_version)
    refuse(application_admission_error_code::unsupported_request_schema,
           "unsupported application request schema");
  if (request.plan().schema_version() != 1)
    refuse(application_admission_error_code::unsupported_plan_schema,
           "unsupported package operation plan schema");
  if (request.plan().kind() != expected_kind)
    refuse(application_admission_error_code::operation_kind_mismatch,
           "application request contains another operation kind");
  if (request.plan().preconditions().target() != request.target().target())
    refuse(application_admission_error_code::target_context_mismatch,
           "application target no longer matches accepted plan");

  validate_target_mutation_lease(request.target(), state, lease);
  validate_backend(request.target(), backend);
  validate_state(request.plan().preconditions(), state);
}

template<class Request>
void
validate_incoming_archive(const Request& request,
                          const pkgimage::package_archive& archive)
{
  const auto& plan = request.plan();
  const auto& incoming = plan.preconditions().incoming_archive();
  if (!incoming)
    refuse(
        application_admission_error_code::incoming_archive_precondition_missing,
        "incoming operation plan lacks archive preconditions");

  const pkgimage::package_image& image = archive.image();
  const pkgimage::archive_inspection_receipt& receipt =
      archive.inspection_receipt();
  const pkgimage::inspected_package_image& admitted = request.incoming().image();

  if (receipt.archive_digest() != admitted.receipt().archive_digest())
    refuse(application_admission_error_code::archive_digest_mismatch,
           "replay archive differs from request-bound build authority");
  if (image.identity() != admitted.image().identity())
    refuse(application_admission_error_code::package_image_mismatch,
           "replay image differs from request-bound build authority");
  if (receipt.identity() != admitted.receipt().identity())
    refuse(application_admission_error_code::inspection_receipt_mismatch,
           "replay inspection differs from request-bound build authority");

  if (receipt.archive_digest() != incoming->archive())
    refuse(application_admission_error_code::archive_digest_mismatch,
           "replay source archive digest differs from accepted plan");
  if (image.identity() != incoming->image() ||
      receipt.image_identity() != image.identity() ||
      receipt.entry_count() != image.size())
  {
    refuse(application_admission_error_code::package_image_mismatch,
           "replay source image differs from accepted plan");
  }
  if (receipt.identity() != incoming->inspection_receipt())
    refuse(application_admission_error_code::inspection_receipt_mismatch,
           "archive inspection receipt differs from accepted plan");

  for (const auto& decision : plan.paths()) {
    if (!decision.incoming_entry())
      continue;
    const pkgimage::package_entry* entry =
        image.entry(*decision.incoming_entry());
    if (entry == nullptr)
      refuse(application_admission_error_code::incoming_entry_missing,
             "plan cites an absent incoming image entry",
             {decision.path()});
    if (entry->path.string() != decision.path().string())
      refuse(application_admission_error_code::incoming_entry_path_mismatch,
             "plan incoming entry resolves to another logical path",
             {decision.path()});
  }
}

} // namespace

application_admission_error::application_admission_error(
    application_admission_error_code code,
    std::string message,
    std::vector<pkgplan::package_path> paths)
    : std::invalid_argument(std::move(message)),
      code_(code),
      paths_(std::move(paths))
{
  std::sort(paths_.begin(), paths_.end());
  paths_.erase(std::unique(paths_.begin(), paths_.end()), paths_.end());
}

application_admission_error::~application_admission_error() = default;

application_admission_error_code
application_admission_error::code() const noexcept
{
  return code_;
}

const std::vector<pkgplan::package_path>&
application_admission_error::paths() const noexcept
{
  return paths_;
}

void
validate_application_admission(
    const installation_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const pkgimage::package_archive& archive)
{
  validate_common(
      request, pkgplan::operation_kind::install, state, lease, backend);
  validate_incoming_archive(request, archive);
}

void
validate_application_admission(
    const upgrade_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const pkgimage::package_archive& archive)
{
  validate_common(
      request, pkgplan::operation_kind::upgrade, state, lease, backend);
  validate_incoming_archive(request, archive);
}

void
validate_application_admission(
    const removal_application_request& request,
    const lease_bound_state_projection& state,
    const target_mutation_lease& lease,
    const application_backend& backend)
{
  validate_common(
      request, pkgplan::operation_kind::remove, state, lease, backend);
  if (request.plan().preconditions().incoming_archive())
    refuse(
        application_admission_error_code::unexpected_incoming_archive_precondition,
        "removal plan contains incoming archive authority");
}

void
validate_backend_transaction(
    const application_target_context& target,
    const target_mutation_lease& lease,
    const application_backend& backend,
    const application_backend_transaction& transaction)
{
  if (transaction.backend() != backend.identity())
    refuse(application_admission_error_code::transaction_backend_mismatch,
           "backend transaction reports another mutation backend");
  if (transaction.observation_backend() != backend.observation_identity())
    refuse(
        application_admission_error_code::transaction_observation_backend_mismatch,
        "backend transaction reports another observation backend");
  if (transaction.capabilities() != backend.capabilities())
    refuse(application_admission_error_code::transaction_capability_mismatch,
           "backend transaction reports another capability profile");
  if (transaction.target() != target.identity())
    refuse(application_admission_error_code::transaction_target_mismatch,
           "backend transaction belongs to another target context");
  if (transaction.lease() != lease.identity())
    refuse(application_admission_error_code::transaction_lease_mismatch,
           "backend transaction belongs to another lease acquisition");
}

} // namespace pkgapply
