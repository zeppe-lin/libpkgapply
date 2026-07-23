// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pkgapply {

/*! \brief Digest algorithm used by application identity schema version 1. */
enum class digest_algorithm : std::uint8_t {
  sha256 = 1,
};

/*! \brief Structured reason that an identity string was rejected. */
enum class digest_error_code : std::uint8_t {
  malformed_representation = 1,
  unsupported_version = 2,
  unsupported_algorithm = 3,
  invalid_length = 4,
  invalid_hexadecimal = 5,
};

/*! \brief Invalid algorithm-qualified application identity. */
class digest_error final : public std::invalid_argument {
public:
  digest_error(digest_error_code code, std::string message);

  [[nodiscard]] digest_error_code code() const noexcept;

private:
  digest_error_code code_;
};

namespace detail {

class identity_factory;


struct managed_target_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/managed-target-reference/v1";
  }
};

struct root_view_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/root-view-reference/v1";
  }
};

struct observation_backend_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/observation-backend-reference/v1";
  }
};

struct mutation_backend_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/mutation-backend-reference/v1";
  }
};

struct mutation_exclusion_domain_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/mutation-exclusion-domain-reference/v1";
  }
};

struct active_object_namespace_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/active-object-namespace-reference/v1";
  }
};

struct rejected_object_store_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/rejected-object-store-reference/v1";
  }
};

struct staging_namespace_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/staging-namespace-reference/v1";
  }
};

struct journal_namespace_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/journal-namespace-reference/v1";
  }
};

struct execution_capability_profile_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/execution-capability-profile-reference/v1";
  }
};

struct lifecycle_executor_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/lifecycle-executor-reference/v1";
  }
};

struct mutation_lease_instance_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/mutation-lease-instance-reference/v1";
  }
};

struct state_projection_evidence_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/state-projection-evidence-reference/v1";
  }
};

struct lease_bound_state_projection_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/lease-bound-state-projection/v1";
  }
};

struct application_attempt_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-attempt/v1";
  }
};

struct application_target_context_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-target-context/v1";
  }
};

struct application_execution_control_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-execution-control/v1";
  }
};

struct application_request_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-request/v1";
  }
};

struct application_journal_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-journal/v1";
  }
};

struct application_receipt_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-receipt/v1";
  }
};

struct completed_application_evidence_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/completed-application-evidence/v1";
  }
};

struct rejected_object_record_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/rejected-object-record/v1";
  }
};

struct application_backend_evidence_identity_domain final {
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-backend-evidence/v1";
  }
};

} // namespace detail

/*! \brief Strict versioned SHA-256 identity in one semantic domain. */
template<class Domain>
class typed_digest final {
public:
  using byte_array = std::array<std::uint8_t, 32>;

  /*! \brief Parse `v1:sha256:<64-lowercase-hex>` strictly. */
  [[nodiscard]] static typed_digest parse(std::string_view value);

  [[nodiscard]] static constexpr std::string_view
  canonical_domain() noexcept
  {
    return Domain::name();
  }

  [[nodiscard]] digest_algorithm algorithm() const noexcept
  {
    return digest_algorithm::sha256;
  }

  [[nodiscard]] const std::string& string() const noexcept
  {
    return value_;
  }

  [[nodiscard]] const byte_array& bytes() const noexcept
  {
    return bytes_;
  }

  friend bool operator==(const typed_digest& lhs,
                         const typed_digest& rhs) noexcept
  {
    return lhs.bytes_ == rhs.bytes_;
  }

  friend bool operator!=(const typed_digest& lhs,
                         const typed_digest& rhs) noexcept
  {
    return !(lhs == rhs);
  }

  friend bool operator<(const typed_digest& lhs,
                        const typed_digest& rhs) noexcept
  {
    return lhs.bytes_ < rhs.bytes_;
  }

private:
  typed_digest(std::string value, byte_array bytes)
      : value_(std::move(value)), bytes_(bytes)
  {
  }

  std::string value_;
  byte_array bytes_;

  friend class detail::identity_factory;
};


using managed_target_identity =
    typed_digest<detail::managed_target_identity_domain>;
using root_view_identity =
    typed_digest<detail::root_view_identity_domain>;
using observation_backend_identity =
    typed_digest<detail::observation_backend_identity_domain>;
using mutation_backend_identity =
    typed_digest<detail::mutation_backend_identity_domain>;
using mutation_exclusion_domain_identity =
    typed_digest<detail::mutation_exclusion_domain_identity_domain>;
using active_object_namespace_identity =
    typed_digest<detail::active_object_namespace_identity_domain>;
using rejected_object_store_identity =
    typed_digest<detail::rejected_object_store_identity_domain>;
using staging_namespace_identity =
    typed_digest<detail::staging_namespace_identity_domain>;
using journal_namespace_identity =
    typed_digest<detail::journal_namespace_identity_domain>;
using execution_capability_profile_identity =
    typed_digest<detail::execution_capability_profile_identity_domain>;
using lifecycle_executor_identity =
    typed_digest<detail::lifecycle_executor_identity_domain>;

using mutation_lease_instance_identity =
    typed_digest<detail::mutation_lease_instance_identity_domain>;
using state_projection_evidence_identity =
    typed_digest<detail::state_projection_evidence_identity_domain>;
using lease_bound_state_projection_identity =
    typed_digest<detail::lease_bound_state_projection_identity_domain>;

using application_attempt_identity =
    typed_digest<detail::application_attempt_identity_domain>;
using application_target_context_identity =
    typed_digest<detail::application_target_context_identity_domain>;
using application_execution_control_identity =
    typed_digest<detail::application_execution_control_identity_domain>;
using application_request_identity =
    typed_digest<detail::application_request_identity_domain>;
using application_journal_identity =
    typed_digest<detail::application_journal_identity_domain>;
using application_receipt_identity =
    typed_digest<detail::application_receipt_identity_domain>;
using completed_application_evidence_identity =
    typed_digest<detail::completed_application_evidence_identity_domain>;
using rejected_object_record_identity =
    typed_digest<detail::rejected_object_record_identity_domain>;
using application_backend_evidence_identity =
    typed_digest<detail::application_backend_evidence_identity_domain>;


extern template class typed_digest<detail::managed_target_identity_domain>;
extern template class typed_digest<detail::root_view_identity_domain>;
extern template class typed_digest<detail::observation_backend_identity_domain>;
extern template class typed_digest<detail::mutation_backend_identity_domain>;
extern template class typed_digest<
    detail::mutation_exclusion_domain_identity_domain>;
extern template class typed_digest<
    detail::active_object_namespace_identity_domain>;
extern template class typed_digest<detail::rejected_object_store_identity_domain>;
extern template class typed_digest<detail::staging_namespace_identity_domain>;
extern template class typed_digest<detail::journal_namespace_identity_domain>;
extern template class typed_digest<
    detail::execution_capability_profile_identity_domain>;
extern template class typed_digest<detail::lifecycle_executor_identity_domain>;

extern template class typed_digest<
    detail::mutation_lease_instance_identity_domain>;
extern template class typed_digest<
    detail::state_projection_evidence_identity_domain>;
extern template class typed_digest<
    detail::lease_bound_state_projection_identity_domain>;
extern template class typed_digest<detail::application_attempt_identity_domain>;
extern template class typed_digest<
    detail::application_target_context_identity_domain>;
extern template class typed_digest<
    detail::application_execution_control_identity_domain>;
extern template class typed_digest<detail::application_request_identity_domain>;
extern template class typed_digest<detail::application_journal_identity_domain>;
extern template class typed_digest<detail::application_receipt_identity_domain>;
extern template class typed_digest<
    detail::completed_application_evidence_identity_domain>;
extern template class typed_digest<detail::rejected_object_record_identity_domain>;
extern template class typed_digest<
    detail::application_backend_evidence_identity_domain>;

} // namespace pkgapply
