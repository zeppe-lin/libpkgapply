// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/apply.h>

#include "application_engine.h"

#include <stdexcept>
#include <utility>

namespace pkgapply {
namespace {

application_receipt
admission_receipt(const detail::application_engine_admission& admission)
{
  const application_receipt* receipt = admission.refusal();
  if (receipt == nullptr)
    throw std::logic_error("refused application admission lacks a receipt");
  return *receipt;
}

application_receipt
preparation_receipt(const detail::application_engine_preparation& preparation)
{
  const application_receipt* receipt = preparation.failure();
  if (receipt == nullptr)
    throw std::logic_error("failed application preparation lacks a receipt");
  return *receipt;
}

application_receipt
publication_receipt(
    const detail::application_engine_rejected_publication& publication)
{
  const application_receipt* receipt = publication.failure();
  if (receipt == nullptr) {
    throw std::logic_error(
        "failed rejected-object publication lacks a receipt");
  }
  return *receipt;
}

template<class Request>
application_receipt
recover_interruption(detail::active_interrupted_application interrupted,
                     const Request& request,
                     const lease_bound_state_projection& state,
                     const target_mutation_lease& lease)
{
  return detail::recover_application_engine(
      std::move(interrupted), request, state, lease);
}

template<class Request>
application_receipt
finish_active(detail::application_engine_active_execution active,
              const Request& request,
              const lease_bound_state_projection& state,
              const target_mutation_lease& lease)
{
  detail::active_interrupted_application* interruption =
      active.interruption();
  if (interruption == nullptr)
    throw std::logic_error("incomplete active execution lacks interruption");
  return recover_interruption(
      std::move(*interruption), request, state, lease);
}

template<class Request>
application_receipt
finish_completion(detail::application_engine_completion completion,
                  const Request& request,
                  const lease_bound_state_projection& state,
                  const target_mutation_lease& lease)
{
  if (completion.has_receipt()) {
    application_receipt* receipt = completion.receipt();
    if (receipt == nullptr)
      throw std::logic_error("sealed application completion lacks a receipt");
    return std::move(*receipt);
  }

  detail::active_interrupted_application* interruption =
      completion.interruption();
  if (interruption == nullptr) {
    throw std::logic_error(
        "unsealed application completion lacks recovery authority");
  }
  return recover_interruption(
      std::move(*interruption), request, state, lease);
}

} // namespace

application_receipt
apply(const installation_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      application_journal_store& journal_store,
      const pkgimage::package_archive& archive)
{
  detail::application_engine_admission admission =
      detail::admit_application_engine(
          request, state, lease, backend, archive);
  if (!admission.is_admitted())
    return admission_receipt(admission);

  detail::admitted_application* admitted = admission.admitted();
  if (admitted == nullptr)
    throw std::logic_error("admitted installation lacks a transaction");

  detail::journaled_application journaled =
      detail::journal_application_engine(
          std::move(*admitted), request, state, lease, journal_store,
          archive.image());
  detail::application_engine_preparation preparation =
      detail::prepare_application_engine(
          std::move(journaled), request, state, lease, archive);
  if (!preparation.is_prepared())
    return preparation_receipt(preparation);

  detail::prepared_application* prepared = preparation.prepared();
  if (prepared == nullptr)
    throw std::logic_error("prepared installation lacks a transaction");

  detail::application_engine_rejected_publication publication =
      detail::publish_rejected_application_engine(
          std::move(*prepared), request, state, lease);
  if (!publication.is_published())
    return publication_receipt(publication);

  detail::rejected_published_application* published = publication.published();
  if (published == nullptr) {
    throw std::logic_error(
        "published installation lacks rejected-object authority");
  }

  detail::application_engine_active_execution active =
      detail::execute_active_application_engine(
          std::move(*published), request, state, lease);
  if (!active.is_complete())
    return finish_active(std::move(active), request, state, lease);

  detail::active_mutated_application* complete = active.complete();
  if (complete == nullptr)
    throw std::logic_error("completed installation lacks active state");

  return finish_completion(
      detail::complete_application_engine(
          std::move(*complete), request, state, lease, archive.image()),
      request, state, lease);
}

application_receipt
apply(const upgrade_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      application_journal_store& journal_store,
      const pkgimage::package_archive& archive)
{
  detail::application_engine_admission admission =
      detail::admit_application_engine(
          request, state, lease, backend, archive);
  if (!admission.is_admitted())
    return admission_receipt(admission);

  detail::admitted_application* admitted = admission.admitted();
  if (admitted == nullptr)
    throw std::logic_error("admitted upgrade lacks a transaction");

  detail::journaled_application journaled =
      detail::journal_application_engine(
          std::move(*admitted), request, state, lease, journal_store,
          archive.image());
  detail::application_engine_preparation preparation =
      detail::prepare_application_engine(
          std::move(journaled), request, state, lease, archive);
  if (!preparation.is_prepared())
    return preparation_receipt(preparation);

  detail::prepared_application* prepared = preparation.prepared();
  if (prepared == nullptr)
    throw std::logic_error("prepared upgrade lacks a transaction");

  detail::application_engine_rejected_publication publication =
      detail::publish_rejected_application_engine(
          std::move(*prepared), request, state, lease);
  if (!publication.is_published())
    return publication_receipt(publication);

  detail::rejected_published_application* published = publication.published();
  if (published == nullptr)
    throw std::logic_error("published upgrade lacks rejected-object authority");

  detail::application_engine_active_execution active =
      detail::execute_active_application_engine(
          std::move(*published), request, state, lease);
  if (!active.is_complete())
    return finish_active(std::move(active), request, state, lease);

  detail::active_mutated_application* complete = active.complete();
  if (complete == nullptr)
    throw std::logic_error("completed upgrade lacks active state");

  return finish_completion(
      detail::complete_application_engine(
          std::move(*complete), request, state, lease, archive.image()),
      request, state, lease);
}

application_receipt
apply(const removal_application_request& request,
      const lease_bound_state_projection& state,
      target_mutation_lease& lease,
      application_backend& backend,
      application_journal_store& journal_store)
{
  detail::application_engine_admission admission =
      detail::admit_application_engine(request, state, lease, backend);
  if (!admission.is_admitted())
    return admission_receipt(admission);

  detail::admitted_application* admitted = admission.admitted();
  if (admitted == nullptr)
    throw std::logic_error("admitted removal lacks a transaction");

  detail::journaled_application journaled =
      detail::journal_application_engine(
          std::move(*admitted), request, state, lease, journal_store);
  detail::application_engine_preparation preparation =
      detail::prepare_application_engine(
          std::move(journaled), request, state, lease);
  if (!preparation.is_prepared())
    return preparation_receipt(preparation);

  detail::prepared_application* prepared = preparation.prepared();
  if (prepared == nullptr)
    throw std::logic_error("prepared removal lacks a transaction");

  detail::application_engine_rejected_publication publication =
      detail::publish_rejected_application_engine(
          std::move(*prepared), request, state, lease);
  if (!publication.is_published())
    return publication_receipt(publication);

  detail::rejected_published_application* published = publication.published();
  if (published == nullptr)
    throw std::logic_error("published removal lacks rejected-object authority");

  detail::application_engine_active_execution active =
      detail::execute_active_application_engine(
          std::move(*published), request, state, lease);
  if (!active.is_complete())
    return finish_active(std::move(active), request, state, lease);

  detail::active_mutated_application* complete = active.complete();
  if (complete == nullptr)
    throw std::logic_error("completed removal lacks active state");

  return finish_completion(
      detail::complete_application_engine(
          std::move(*complete), request, state, lease),
      request, state, lease);
}

} // namespace pkgapply
