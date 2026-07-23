// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgapply/digest.h>

#include "identity_factory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pkgapply {
namespace {

std::uint8_t
hexadecimal_value(char value)
{
  if (value >= '0' && value <= '9')
    return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f')
    return static_cast<std::uint8_t>(value - 'a' + 10);
  throw digest_error(digest_error_code::invalid_hexadecimal,
                     "application identity requires lowercase hexadecimal");
}

} // namespace

digest_error::digest_error(digest_error_code code, std::string message)
    : std::invalid_argument(std::move(message)), code_(code)
{
}

digest_error_code
digest_error::code() const noexcept
{
  return code_;
}

template<class Domain>
typed_digest<Domain>
typed_digest<Domain>::parse(std::string_view value)
{
  constexpr std::string_view prefix = "v1:sha256:";

  const std::size_t first = value.find(':');
  if (first == std::string_view::npos)
    throw digest_error(digest_error_code::malformed_representation,
                       "application identity has no version separator");

  if (value.substr(0, first) != "v1")
    throw digest_error(digest_error_code::unsupported_version,
                       "unsupported application identity version");

  const std::size_t second = value.find(':', first + 1);
  if (second == std::string_view::npos)
    throw digest_error(digest_error_code::malformed_representation,
                       "application identity has no algorithm separator");

  if (value.substr(first + 1, second - first - 1) != "sha256")
    throw digest_error(digest_error_code::unsupported_algorithm,
                       "unsupported application identity algorithm");

  if (value.size() != prefix.size() + 64)
    throw digest_error(digest_error_code::invalid_length,
                       "application SHA-256 identity has invalid length");

  typename typed_digest<Domain>::byte_array bytes{};
  const std::string_view hexadecimal = value.substr(prefix.size());
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const std::uint8_t high = hexadecimal_value(hexadecimal[index * 2]);
    const std::uint8_t low = hexadecimal_value(hexadecimal[index * 2 + 1]);
    bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
  }

  return detail::identity_factory::from_sha256<typed_digest<Domain>>(bytes);
}


template class typed_digest<detail::managed_target_identity_domain>;
template class typed_digest<detail::root_view_identity_domain>;
template class typed_digest<detail::observation_backend_identity_domain>;
template class typed_digest<detail::mutation_backend_identity_domain>;
template class typed_digest<
    detail::mutation_exclusion_domain_identity_domain>;
template class typed_digest<
    detail::active_object_namespace_identity_domain>;
template class typed_digest<detail::rejected_object_store_identity_domain>;
template class typed_digest<detail::staging_namespace_identity_domain>;
template class typed_digest<detail::journal_namespace_identity_domain>;
template class typed_digest<
    detail::execution_capability_profile_identity_domain>;
template class typed_digest<detail::lifecycle_executor_identity_domain>;

template class typed_digest<
    detail::mutation_lease_instance_identity_domain>;
template class typed_digest<
    detail::state_projection_evidence_identity_domain>;
template class typed_digest<
    detail::lease_bound_state_projection_identity_domain>;
template class typed_digest<detail::application_attempt_identity_domain>;
template class typed_digest<
    detail::application_target_context_identity_domain>;
template class typed_digest<
    detail::application_execution_control_identity_domain>;
template class typed_digest<detail::application_request_identity_domain>;
template class typed_digest<detail::application_journal_identity_domain>;
template class typed_digest<detail::application_receipt_identity_domain>;
template class typed_digest<
    detail::completed_application_evidence_identity_domain>;
template class typed_digest<detail::rejected_object_record_identity_domain>;
template class typed_digest<
    detail::application_backend_evidence_identity_domain>;

} // namespace pkgapply
