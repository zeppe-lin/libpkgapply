// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file digest.h
 *  \brief Strongly typed application identities and digest representation.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pkgapply {

/*! \brief Public identity-representation schema version. */
inline constexpr std::uint16_t digest_representation_version = 1;

/*! \brief Number of bytes in the supported SHA-256 digest. */
inline constexpr std::size_t sha256_digest_size = 32;

/*! \brief Digest algorithm carried by application identities. */
enum class digest_algorithm : std::uint8_t {
  sha256 = 1, /*!< SHA-256 with a 32-byte result. */
};

/*! \brief Stable reason that an identity representation was refused. */
enum class digest_error_code : std::uint8_t {
  malformed_representation = 1, /*!< The representation lacks required fields. */
  unsupported_version = 2, /*!< The representation version is unsupported. */
  unsupported_algorithm = 3, /*!< The algorithm tag is unsupported. */
  invalid_length = 4, /*!< The hexadecimal digest has the wrong length. */
  invalid_hexadecimal = 5, /*!< Digest text is not lowercase hexadecimal. */
};

/*! \brief Typed refusal raised while parsing an application identity. */
class digest_error final : public std::invalid_argument {
public:
  /*! \brief Construct a digest refusal.
   *  \param code Stable refusal category.
   *  \param message Human-readable diagnostic text.
   */
  digest_error(digest_error_code code, std::string message);

  /*! \brief Destroy the polymorphic refusal. */
  ~digest_error() override;

  /*! \brief Return the stable refusal category.
   *  \return Category supplied at construction.
   */
  [[nodiscard]] digest_error_code code() const noexcept;

private:
  digest_error_code code_;
};

namespace detail {

/*! \brief Internal authority allowed to construct identities from digest bytes. */
class identity_factory;

/*! \brief Domain tag for the identity of managed target selected by orchestration. */
struct managed_target_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/managed-target-reference/v1";
  }
};

/*! \brief Domain tag for the identity of root view through which a target is observed and mutated. */
struct root_view_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/root-view-reference/v1";
  }
};

/*! \brief Domain tag for the identity of observation backend implementation and schema. */
struct observation_backend_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/observation-backend-reference/v1";
  }
};

/*! \brief Domain tag for the identity of mutation backend implementation and schema. */
struct mutation_backend_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/mutation-backend-reference/v1";
  }
};

/*! \brief Domain tag for the identity of mutation-exclusion domain. */
struct mutation_exclusion_domain_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/mutation-exclusion-domain-reference/v1";
  }
};

/*! \brief Domain tag for the identity of active target object namespace. */
struct active_object_namespace_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/active-object-namespace-reference/v1";
  }
};

/*! \brief Domain tag for the identity of rejected-object evidence store. */
struct rejected_object_store_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/rejected-object-store-reference/v1";
  }
};

/*! \brief Domain tag for the identity of incoming payload staging namespace. */
struct staging_namespace_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/staging-namespace-reference/v1";
  }
};

/*! \brief Domain tag for the identity of application journal namespace. */
struct journal_namespace_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/journal-namespace-reference/v1";
  }
};

/*! \brief Domain tag for the identity of backend execution capability profile. */
struct execution_capability_profile_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/execution-capability-profile-reference/v1";
  }
};

/*! \brief Domain tag for the identity of lifecycle executor implementation and schema. */
struct lifecycle_executor_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/lifecycle-executor-reference/v1";
  }
};

/*! \brief Domain tag for the identity of one acquired mutation lease instance. */
struct mutation_lease_instance_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/mutation-lease-instance-reference/v1";
  }
};

/*! \brief Domain tag for the identity of evidence supporting one state projection. */
struct state_projection_evidence_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/state-projection-evidence-reference/v1";
  }
};

/*! \brief Domain tag for the identity of state projection bound to one mutation lease. */
struct lease_bound_state_projection_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/lease-bound-state-projection/v1";
  }
};

/*! \brief Domain tag for the identity of one physical application attempt. */
struct application_attempt_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-attempt/v1";
  }
};

/*! \brief Domain tag for the identity of one complete application target context. */
struct application_target_context_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-target-context/v1";
  }
};

/*! \brief Domain tag for the identity of one normalized execution-control policy. */
struct application_execution_control_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-execution-control/v1";
  }
};

/*! \brief Domain tag for the identity of one admitted incoming package authority. */
struct incoming_package_authority_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/incoming-package-authority/v1";
  }
};

/*! \brief Domain tag for the identity of one complete package application request. */
struct application_request_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-request/v1";
  }
};

/*! \brief Domain tag for the identity of one complete application journal. */
struct application_journal_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-journal/v1";
  }
};

/*! \brief Domain tag for the identity of one durable application journal effect. */
struct application_journal_effect_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-journal-effect/v1";
  }
};

/*! \brief Domain tag for the identity of one immutable journal record. */
struct application_journal_record_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-journal-record/v1";
  }
};

/*! \brief Domain tag for the identity of one completed application receipt. */
struct application_receipt_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-receipt/v1";
  }
};

/*! \brief Domain tag for the identity of one complete successful application evidence set. */
struct completed_application_evidence_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/completed-application-evidence/v1";
  }
};

/*! \brief Domain tag for the identity of decoded bytes of one completed regular object. */
struct completed_regular_content_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/completed-regular-content/v1";
  }
};

/*! \brief Domain tag for the identity of one durably retained rejected object. */
struct rejected_object_record_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/rejected-object-record/v1";
  }
};

/*! \brief Domain tag for the identity of opaque evidence issued by an application backend. */
struct application_backend_evidence_identity_domain final {
  /*! \brief Return the canonical identity domain separator.
   *  \return Process-lifetime protocol string.
   */
  [[nodiscard]] static constexpr std::string_view name() noexcept
  {
    return "pkgapply/application-backend-evidence/v1";
  }
};

} // namespace detail

/*! \brief Strict versioned SHA-256 identity in one semantic domain.
 *
 *  \tparam Domain Empty domain tag exposing a canonical `name()` separator.
 *
 *  The template centralizes representation mechanics while the domain tag
 *  prevents identities from unrelated authorities from being interchanged or
 *  compared. Construction from digest bytes is reserved for identity-owning
 *  library code; ordinary consumers parse canonical text or receive values
 *  from owner APIs.
 */
template<class Domain>
class typed_digest final {
public:
  /*! \brief Fixed-size SHA-256 result accepted by the identity factory. */
  using byte_array = std::array<std::uint8_t, sha256_digest_size>;

  /*! \brief Parse the canonical algorithm-qualified representation.
   *  \param value Text in `v1:sha256:<64-lowercase-hex>` form.
   *  \return Parsed identity in this exact semantic domain.
   *  \throws digest_error When the representation is malformed or unsupported.
   */
  [[nodiscard]] static typed_digest parse(std::string_view value);

  /*! \brief Return the canonical semantic domain separator.
   *  \return Process-lifetime domain string supplied by `Domain`.
   */
  [[nodiscard]] static constexpr std::string_view
  canonical_domain() noexcept
  {
    return Domain::name();
  }

  /*! \brief Return the represented digest algorithm.
   *  \return `digest_algorithm::sha256` for every constructible identity.
   */
  [[nodiscard]] digest_algorithm algorithm() const noexcept
  {
    return digest_algorithm::sha256;
  }

  /*! \brief Return canonical algorithm-qualified text.
   *  \return Reference valid for the lifetime of this identity.
   */
  [[nodiscard]] const std::string& string() const noexcept
  {
    return value_;
  }

  /*! \brief Return the exact SHA-256 bytes.
   *  \return Reference valid for the lifetime of this identity.
   */
  [[nodiscard]] const byte_array& bytes() const noexcept
  {
    return bytes_;
  }

  /*! \brief Compare identities in the same semantic domain for equality.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when both SHA-256 results are equal.
   */
  friend bool operator==(const typed_digest& lhs,
                         const typed_digest& rhs) noexcept
  {
    return lhs.bytes_ == rhs.bytes_;
  }

  /*! \brief Compare identities in the same semantic domain for inequality.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when the SHA-256 results differ.
   */
  friend bool operator!=(const typed_digest& lhs,
                         const typed_digest& rhs) noexcept
  {
    return !(lhs == rhs);
  }

  /*! \brief Order identities in the same semantic domain canonically.
   *  \param lhs Left operand.
   *  \param rhs Right operand.
   *  \return `true` when `lhs` precedes `rhs` by digest bytes.
   */
  friend bool operator<(const typed_digest& lhs,
                        const typed_digest& rhs) noexcept
  {
    return lhs.bytes_ < rhs.bytes_;
  }

private:
  /*! \brief Construct canonical text and bytes issued by the identity factory. */
  typed_digest(std::string value, byte_array bytes)
      : value_(std::move(value)), bytes_(bytes)
  {
  }

  std::string value_;
  byte_array bytes_;

  friend class detail::identity_factory;
};

/*! \brief Identity of a managed target selected by orchestration. */
using managed_target_identity = typed_digest<detail::managed_target_identity_domain>;

/*! \brief Identity of a root view through which a target is observed and mutated. */
using root_view_identity = typed_digest<detail::root_view_identity_domain>;

/*! \brief Identity of an observation backend implementation and schema. */
using observation_backend_identity = typed_digest<detail::observation_backend_identity_domain>;

/*! \brief Identity of a mutation backend implementation and schema. */
using mutation_backend_identity = typed_digest<detail::mutation_backend_identity_domain>;

/*! \brief Identity of a mutation-exclusion domain. */
using mutation_exclusion_domain_identity = typed_digest<detail::mutation_exclusion_domain_identity_domain>;

/*! \brief Identity of an active target object namespace. */
using active_object_namespace_identity = typed_digest<detail::active_object_namespace_identity_domain>;

/*! \brief Identity of a rejected-object evidence store. */
using rejected_object_store_identity = typed_digest<detail::rejected_object_store_identity_domain>;

/*! \brief Identity of an incoming payload staging namespace. */
using staging_namespace_identity = typed_digest<detail::staging_namespace_identity_domain>;

/*! \brief Identity of an application journal namespace. */
using journal_namespace_identity = typed_digest<detail::journal_namespace_identity_domain>;

/*! \brief Identity of a backend execution capability profile. */
using execution_capability_profile_identity = typed_digest<detail::execution_capability_profile_identity_domain>;

/*! \brief Identity of a lifecycle executor implementation and schema. */
using lifecycle_executor_identity = typed_digest<detail::lifecycle_executor_identity_domain>;

/*! \brief Identity of an one acquired mutation lease instance. */
using mutation_lease_instance_identity = typed_digest<detail::mutation_lease_instance_identity_domain>;

/*! \brief Identity of an evidence supporting one state projection. */
using state_projection_evidence_identity = typed_digest<detail::state_projection_evidence_identity_domain>;

/*! \brief Identity of a state projection bound to one mutation lease. */
using lease_bound_state_projection_identity = typed_digest<detail::lease_bound_state_projection_identity_domain>;

/*! \brief Identity of an one physical application attempt. */
using application_attempt_identity = typed_digest<detail::application_attempt_identity_domain>;

/*! \brief Identity of an one complete application target context. */
using application_target_context_identity = typed_digest<detail::application_target_context_identity_domain>;

/*! \brief Identity of an one normalized execution-control policy. */
using application_execution_control_identity = typed_digest<detail::application_execution_control_identity_domain>;

/*! \brief Identity of an one admitted incoming package authority. */
using incoming_package_authority_identity = typed_digest<detail::incoming_package_authority_identity_domain>;

/*! \brief Identity of an one complete package application request. */
using application_request_identity = typed_digest<detail::application_request_identity_domain>;

/*! \brief Identity of an one complete application journal. */
using application_journal_identity = typed_digest<detail::application_journal_identity_domain>;

/*! \brief Identity of an one durable application journal effect. */
using application_journal_effect_identity = typed_digest<detail::application_journal_effect_identity_domain>;

/*! \brief Identity of an one immutable journal record. */
using application_journal_record_identity = typed_digest<detail::application_journal_record_identity_domain>;

/*! \brief Identity of an one completed application receipt. */
using application_receipt_identity = typed_digest<detail::application_receipt_identity_domain>;

/*! \brief Identity of an one complete successful application evidence set. */
using completed_application_evidence_identity = typed_digest<detail::completed_application_evidence_identity_domain>;

/*! \brief Identity of a decoded bytes of one completed regular object. */
using completed_regular_content_identity = typed_digest<detail::completed_regular_content_identity_domain>;

/*! \brief Identity of an one durably retained rejected object. */
using rejected_object_record_identity = typed_digest<detail::rejected_object_record_identity_domain>;

/*! \brief Identity of an opaque evidence issued by an application backend. */
using application_backend_evidence_identity = typed_digest<detail::application_backend_evidence_identity_domain>;

/*! \cond */
extern template class typed_digest<detail::managed_target_identity_domain>;
extern template class typed_digest<detail::root_view_identity_domain>;
extern template class typed_digest<detail::observation_backend_identity_domain>;
extern template class typed_digest<detail::mutation_backend_identity_domain>;
extern template class typed_digest<detail::mutation_exclusion_domain_identity_domain>;
extern template class typed_digest<detail::active_object_namespace_identity_domain>;
extern template class typed_digest<detail::rejected_object_store_identity_domain>;
extern template class typed_digest<detail::staging_namespace_identity_domain>;
extern template class typed_digest<detail::journal_namespace_identity_domain>;
extern template class typed_digest<detail::execution_capability_profile_identity_domain>;
extern template class typed_digest<detail::lifecycle_executor_identity_domain>;
extern template class typed_digest<detail::mutation_lease_instance_identity_domain>;
extern template class typed_digest<detail::state_projection_evidence_identity_domain>;
extern template class typed_digest<detail::lease_bound_state_projection_identity_domain>;
extern template class typed_digest<detail::application_attempt_identity_domain>;
extern template class typed_digest<detail::application_target_context_identity_domain>;
extern template class typed_digest<detail::application_execution_control_identity_domain>;
extern template class typed_digest<detail::incoming_package_authority_identity_domain>;
extern template class typed_digest<detail::application_request_identity_domain>;
extern template class typed_digest<detail::application_journal_identity_domain>;
extern template class typed_digest<detail::application_journal_effect_identity_domain>;
extern template class typed_digest<detail::application_journal_record_identity_domain>;
extern template class typed_digest<detail::application_receipt_identity_domain>;
extern template class typed_digest<detail::completed_application_evidence_identity_domain>;
extern template class typed_digest<detail::completed_regular_content_identity_domain>;
extern template class typed_digest<detail::rejected_object_record_identity_domain>;
extern template class typed_digest<detail::application_backend_evidence_identity_domain>;
/*! \endcond */

} // namespace pkgapply
