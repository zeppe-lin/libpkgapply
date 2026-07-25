// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <libpkgapply-posix/payload_stage.h>
#include <libpkgapply/backend.h>
#include <libpkgapply/path_consequence.h>
#include <libpkgimage/package_image.h>

#include "active_workspace.h"

namespace pkgapply::posix::detail {

/*! \brief Attempt-bound POSIX mechanism for the managed active namespace. */
class application_active_namespace final {
public:
  [[nodiscard]] static application_active_namespace bind(
      int target_root_fd,
      application_attempt attempt,
      const pkgimage::package_image& incoming_image,
      const sealed_application_payloads* payloads,
      std::vector<application_path_observation> admitted);

  application_active_namespace(const application_active_namespace&) = delete;
  application_active_namespace& operator=(
      const application_active_namespace&) = delete;
  application_active_namespace(application_active_namespace&& other) noexcept;
  application_active_namespace& operator=(
      application_active_namespace&& other) noexcept;
  ~application_active_namespace();

  /*! \brief Publish one exact incoming active object. */
  [[nodiscard]] backend_operation_result publish_incoming(
      const backend_active_effect_request& request);

  /*! \brief Synchronize all active objects and parents changed so far. */
  [[nodiscard]] application_durability_fact synchronize();

  /* Internal descriptor retention used by mechanism helpers. */
  void retain_dirty_descriptor(int descriptor);

private:
  application_active_namespace(
      application_active_workspace workspace,
      const pkgimage::package_image* incoming_image,
      const sealed_application_payloads* payloads,
      std::vector<application_path_observation> admitted);

  [[nodiscard]] const application_path_observation* admitted(
      const pkgplan::package_path& path) const noexcept;
  application_active_workspace workspace_;
  const pkgimage::package_image* incoming_image_ = nullptr;
  const sealed_application_payloads* payloads_ = nullptr;
  std::vector<application_path_observation> admitted_;
  std::vector<int> dirty_descriptors_;
};

} // namespace pkgapply::posix::detail
