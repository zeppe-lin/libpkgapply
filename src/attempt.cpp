// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/attempt.h>

#include "canonical_record.h"
#include "identity_factory.h"

#include <algorithm>
#include <utility>

namespace pkgapply {

application_attempt_nonce
application_attempt_nonce::from_bytes(byte_array bytes)
{
  return application_attempt_nonce(std::move(bytes));
}

application_attempt_nonce::application_attempt_nonce(byte_array bytes)
    : bytes_(std::move(bytes))
{
}

const application_attempt_nonce::byte_array&
application_attempt_nonce::bytes() const noexcept
{
  return bytes_;
}

bool
operator==(const application_attempt_nonce& lhs,
           const application_attempt_nonce& rhs) noexcept
{
  return lhs.bytes_ == rhs.bytes_;
}

bool
operator!=(const application_attempt_nonce& lhs,
           const application_attempt_nonce& rhs) noexcept
{
  return !(lhs == rhs);
}

bool
operator<(const application_attempt_nonce& lhs,
          const application_attempt_nonce& rhs) noexcept
{
  return lhs.bytes_ < rhs.bytes_;
}

application_attempt
application_attempt::make(
    application_request_identity request,
    application_target_context_identity target,
    mutation_backend_identity backend,
    application_attempt_nonce nonce)
{
  detail::canonical_record record(
      application_attempt_identity::canonical_domain());
  record.append_u16(application_attempt_schema_version);
  record.append_digest(request);
  record.append_digest(target);
  record.append_digest(backend);
  for (const auto byte : nonce.bytes())
    record.append_u8(byte);

  auto identity = detail::identity_factory::from_sha256<
      application_attempt_identity>(record.sha256());
  return application_attempt(
      std::move(identity), std::move(request), std::move(target),
      std::move(backend), std::move(nonce));
}

application_attempt::application_attempt(
    application_attempt_identity identity,
    application_request_identity request,
    application_target_context_identity target,
    mutation_backend_identity backend,
    application_attempt_nonce nonce)
    : identity_(std::move(identity)), request_(std::move(request)),
      target_(std::move(target)), backend_(std::move(backend)),
      nonce_(std::move(nonce))
{
}

std::uint16_t application_attempt::schema_version() const noexcept
{ return schema_version_; }
const application_attempt_identity& application_attempt::identity() const noexcept
{ return identity_; }
const application_request_identity& application_attempt::request() const noexcept
{ return request_; }
const application_target_context_identity& application_attempt::target() const noexcept
{ return target_; }
const mutation_backend_identity& application_attempt::backend() const noexcept
{ return backend_; }
const application_attempt_nonce& application_attempt::nonce() const noexcept
{ return nonce_; }

} // namespace pkgapply
