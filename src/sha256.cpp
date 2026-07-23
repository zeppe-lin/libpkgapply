// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>

namespace pkgapply::detail {
namespace {

struct evp_context_deleter final {
  void operator()(EVP_MD_CTX* context) const noexcept
  {
    EVP_MD_CTX_free(context);
  }
};

} // namespace

std::array<std::uint8_t, 32>
sha256(const std::byte* data, std::size_t size)
{
  std::unique_ptr<EVP_MD_CTX, evp_context_deleter> context(EVP_MD_CTX_new());
  if (!context)
    throw std::runtime_error("cannot allocate SHA-256 context");

  if (EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    throw std::runtime_error("cannot initialize SHA-256 context");

  if (size != 0 && EVP_DigestUpdate(context.get(), data, size) != 1)
    throw std::runtime_error("cannot update SHA-256 context");

  std::array<std::uint8_t, 32> digest{};
  unsigned int digest_size = 0;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1)
    throw std::runtime_error("cannot finalize SHA-256 context");

  if (digest_size != digest.size())
    throw std::runtime_error("unexpected SHA-256 digest size");

  return digest;
}

std::array<std::uint8_t, 32>
sha256(const std::vector<std::byte>& data)
{
  return sha256(data.data(), data.size());
}

} // namespace pkgapply::detail
