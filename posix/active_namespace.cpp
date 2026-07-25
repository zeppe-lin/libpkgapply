// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "active_namespace.h"

#include <libpkgapply-posix/target_observer.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

namespace pkgapply::posix::detail {
namespace {

class unique_fd final {
public:
  explicit unique_fd(int value = -1) noexcept : value_(value) {}
  unique_fd(const unique_fd&) = delete;
  unique_fd& operator=(const unique_fd&) = delete;
  unique_fd(unique_fd&& other) noexcept : value_(other.release()) {}
  unique_fd& operator=(unique_fd&& other) noexcept
  {
    if (this != &other) {
      reset();
      value_ = other.release();
    }
    return *this;
  }
  ~unique_fd() { reset(); }
  [[nodiscard]] int get() const noexcept { return value_; }
  [[nodiscard]] int release() noexcept
  {
    const int value = value_;
    value_ = -1;
    return value;
  }
  void reset(int value = -1) noexcept
  {
    if (value_ >= 0)
      static_cast<void>(::close(value_));
    value_ = value;
  }
private:
  int value_;
};

struct evp_context_deleter final {
  void operator()(EVP_MD_CTX* context) const noexcept
  {
    EVP_MD_CTX_free(context);
  }
};

[[nodiscard]] bool
equal_digest_bytes(const std::array<std::uint8_t, 32>& lhs,
                   const pkgimage::digest_bytes& rhs) noexcept
{
  return lhs.size() == rhs.size() &&
      std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

[[nodiscard]] backend_operation_result
operation(backend_operation_outcome outcome)
{
  return backend_operation_result(outcome);
}

[[nodiscard]] int
duplicate_fd(int descriptor)
{
#ifdef F_DUPFD_CLOEXEC
  return ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
#else
  const int duplicate = ::dup(descriptor);
  if (duplicate >= 0) {
    const int flags = ::fcntl(duplicate, F_GETFD);
    if (flags < 0 || ::fcntl(duplicate, F_SETFD, flags | FD_CLOEXEC) != 0) {
      const int saved = errno;
      static_cast<void>(::close(duplicate));
      errno = saved;
      return -1;
    }
  }
  return duplicate;
#endif
}

[[nodiscard]] std::optional<struct stat>
stat_leaf(int parent, const std::string& name)
{
  struct stat status {};
  for (;;) {
    if (::fstatat(parent, name.c_str(), &status, AT_SYMLINK_NOFOLLOW) == 0)
      return status;
    if (errno == EINTR)
      continue;
    if (errno == ENOENT)
      return std::nullopt;
    throw std::runtime_error("cannot inspect active namespace leaf");
  }
}

void
remove_prepared(const active_path_workspace& workspace,
                pkgimage::entry_type type) noexcept
{
  const int flags = type == pkgimage::entry_type::directory
      ? AT_REMOVEDIR
      : 0;
  static_cast<void>(::unlinkat(
      workspace.parent_descriptor(), workspace.prepared_name().c_str(),
      flags));
}

[[nodiscard]] bool
same_inode_metadata(const pkgimage::package_entry& lhs,
                    const pkgimage::package_entry& rhs) noexcept
{
  return lhs.mode == rhs.mode && lhs.uid == rhs.uid && lhs.gid == rhs.gid &&
      lhs.mtime == rhs.mtime &&
      lhs.mtime_nanoseconds == rhs.mtime_nanoseconds;
}

void
validate_hard_links(const pkgimage::package_image& image)
{
  for (const auto& entry : image.entries()) {
    if (entry.type != pkgimage::entry_type::hardlink)
      continue;
    if (!entry.hardlink_target)
      throw std::invalid_argument("incoming hard link lacks its anchor");
    const auto* anchor = image.find(*entry.hardlink_target);
    if (anchor == nullptr || anchor->type != pkgimage::entry_type::regular)
      throw std::invalid_argument("incoming hard-link anchor is not regular");
    if (!same_inode_metadata(entry, *anchor)) {
      throw std::invalid_argument(
          "incoming hard-link metadata differs from its regular anchor");
    }
  }
}

void
validate_binding(const application_active_workspace& workspace,
                 const pkgimage::package_image& image,
                 const sealed_application_payloads* payloads)
{
  if (payloads == nullptr)
    return;
  if (payloads->attempt().identity() != workspace.attempt().identity() ||
      payloads->image() != image.identity())
  {
    throw std::invalid_argument(
        "active namespace payload authority binding mismatch");
  }
}

[[nodiscard]] bool
same_observation(const application_path_observation& lhs,
                 const application_path_observation& rhs) noexcept
{
  if (lhs.path() != rhs.path() || lhs.state() != rhs.state())
    return false;
  if (lhs.object().has_value() != rhs.object().has_value())
    return false;
  return !lhs.object() || *lhs.object() == *rhs.object();
}

[[nodiscard]] bool
same_object_without_hardlink(const completed_object_fact& lhs,
                             const completed_object_fact& rhs) noexcept
{
  return lhs.path() == rhs.path() && lhs.kind() == rhs.kind() &&
      lhs.mode() == rhs.mode() && lhs.uid() == rhs.uid() &&
      lhs.gid() == rhs.gid() && lhs.size() == rhs.size() &&
      lhs.mtime() == rhs.mtime() &&
      lhs.regular_content() == rhs.regular_content() &&
      lhs.symlink_target() == rhs.symlink_target() &&
      lhs.device() == rhs.device();
}

[[nodiscard]] bool
same_regular_inode(int member_parent,
                   const std::string& member_name,
                   int anchor_parent,
                   const std::string& anchor_name)
{
  const auto member = stat_leaf(member_parent, member_name);
  const auto anchor = stat_leaf(anchor_parent, anchor_name);
  return member && anchor && S_ISREG(member->st_mode) &&
      S_ISREG(anchor->st_mode) && member->st_dev == anchor->st_dev &&
      member->st_ino == anchor->st_ino;
}

[[nodiscard]] bool
still_admitted(const application_active_workspace& roots,
               const application_path_observation& admitted)
{
  application_target_observer observer =
      application_target_observer::from_directory_fd(
          roots.target_root_descriptor());

  const bool known_hardlink = admitted.object() &&
      admitted.object()->hardlink().state() == fact_state::known &&
      admitted.object()->hardlink().value();
  if (!known_hardlink) {
    backend_observation_batch observed = observer.observe(
        {admitted.path()}, {});
    const auto* current = observed.find(admitted.path());
    return current != nullptr && same_observation(admitted, *current);
  }

  backend_observation_batch observed = observer.observe(
      {admitted.path()}, {});
  const auto* current = observed.find(admitted.path());
  if (current == nullptr || current->state() != admitted.state() ||
      !current->object() ||
      !same_object_without_hardlink(*admitted.object(), *current->object()))
  {
    return false;
  }

  active_path_workspace member = roots.open(admitted.path());
  const auto& anchor_path =
      admitted.object()->hardlink().value()->anchor();
  active_path_workspace anchor = roots.open(anchor_path);
  const active_workspace_snapshot anchor_state = anchor.inspect();
  if (anchor_state.state() == active_workspace_state::contradictory)
    return false;

  const std::string& anchor_name = anchor_state.displaced_present()
      ? anchor.displaced_name()
      : anchor.leaf();
  return same_regular_inode(
      member.parent_descriptor(), member.leaf(),
      anchor.parent_descriptor(), anchor_name);
}


[[nodiscard]] const pkgimage::package_entry&
resolve_entry(const pkgimage::package_image& image,
              const backend_active_effect_request& request)
{
  if (request.outcome() != pkgplan::planned_active_outcome::activate_incoming ||
      !request.incoming_entry())
  {
    throw std::invalid_argument(
        "incoming publication requires an activate-incoming request");
  }
  const auto* entry = image.entry(*request.incoming_entry());
  if (entry == nullptr || entry->path.string() != request.path().string())
    throw std::invalid_argument("active request cites another image entry");
  return *entry;
}

[[nodiscard]] std::array<std::uint8_t, 32>
copy_regular_payload(int source,
                     int destination,
                     std::uint64_t expected_size)
{
  std::unique_ptr<EVP_MD_CTX, evp_context_deleter> context(EVP_MD_CTX_new());
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    throw std::runtime_error("cannot initialize active payload digest");

  std::array<std::byte, 64U * 1024U> buffer {};
  std::uint64_t copied = 0;
  for (;;) {
    ssize_t count;
    do {
      count = ::read(source, buffer.data(), buffer.size());
    } while (count < 0 && errno == EINTR);
    if (count < 0)
      throw std::runtime_error("cannot read sealed active payload");
    if (count == 0)
      break;
    const auto amount = static_cast<std::size_t>(count);
    std::size_t offset = 0;
    while (offset < amount) {
      ssize_t written;
      do {
        written = ::write(
            destination, buffer.data() + offset, amount - offset);
      } while (written < 0 && errno == EINTR);
      if (written <= 0)
        throw std::runtime_error("cannot write prepared active payload");
      offset += static_cast<std::size_t>(written);
    }
    if (EVP_DigestUpdate(context.get(), buffer.data(), amount) != 1)
      throw std::runtime_error("cannot update active payload digest");
    copied += static_cast<std::uint64_t>(amount);
  }
  if (copied != expected_size)
    throw std::runtime_error("sealed active payload size changed");

  std::array<std::uint8_t, 32> digest {};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1 ||
      size != digest.size())
  {
    throw std::runtime_error("cannot finalize active payload digest");
  }
  return digest;
}

[[nodiscard]] bool
apply_descriptor_metadata(int descriptor,
                          const pkgimage::package_entry& entry)
{
  if (::fchown(descriptor, static_cast<uid_t>(entry.uid),
               static_cast<gid_t>(entry.gid)) != 0)
    return false;
  if (::fchmod(descriptor, static_cast<mode_t>(entry.mode & 07777U)) != 0)
    return false;
  const struct timespec times[2] = {
      {0, UTIME_OMIT},
      {entry.mtime, static_cast<long>(entry.mtime_nanoseconds)},
  };
  return ::futimens(descriptor, times) == 0;
}

[[nodiscard]] bool
apply_path_metadata(int parent,
                    const std::string& name,
                    const pkgimage::package_entry& entry,
                    bool symbolic_link)
{
  if (::fchownat(parent, name.c_str(), static_cast<uid_t>(entry.uid),
                 static_cast<gid_t>(entry.gid), AT_SYMLINK_NOFOLLOW) != 0)
    return false;
  if (!symbolic_link &&
      ::fchmodat(parent, name.c_str(),
                 static_cast<mode_t>(entry.mode & 07777U), 0) != 0)
    return false;
  const struct timespec times[2] = {
      {0, UTIME_OMIT},
      {entry.mtime, static_cast<long>(entry.mtime_nanoseconds)},
  };
  return ::utimensat(
      parent, name.c_str(), times,
      symbolic_link ? AT_SYMLINK_NOFOLLOW : 0) == 0;
}

[[nodiscard]] unique_fd
prepare_regular(const active_path_workspace& workspace,
                const pkgimage::package_entry& entry,
                const sealed_application_payloads* payloads)
{
  if (payloads == nullptr)
    return unique_fd();
  staged_regular_payload payload = payloads->open(entry.id);
  unique_fd file(::openat(
      workspace.parent_descriptor(), workspace.prepared_name().c_str(),
      O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (file.get() < 0)
    return unique_fd();
  try {
    const auto digest = copy_regular_payload(
        payload.descriptor(), file.get(), payload.size());
    if (!entry.regular_content ||
        !equal_digest_bytes(digest, entry.regular_content->bytes()) ||
        payload.size() != entry.size ||
        !apply_descriptor_metadata(file.get(), entry))
    {
      remove_prepared(workspace, entry.type);
      return unique_fd();
    }
  } catch (...) {
    remove_prepared(workspace, entry.type);
    throw;
  }
  return file;
}

[[nodiscard]] unique_fd
prepare_directory(const active_path_workspace& workspace,
                  const pkgimage::package_entry& entry)
{
  if (::mkdirat(workspace.parent_descriptor(),
                workspace.prepared_name().c_str(), 0700) != 0)
    return unique_fd();
  unique_fd directory(::openat(
      workspace.parent_descriptor(), workspace.prepared_name().c_str(),
      O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.get() < 0 ||
      !apply_descriptor_metadata(directory.get(), entry))
  {
    remove_prepared(workspace, entry.type);
    return unique_fd();
  }
  return directory;
}

[[nodiscard]] bool
prepare_symlink(const active_path_workspace& workspace,
                const pkgimage::package_entry& entry)
{
  if (!entry.symlink_target ||
      ::symlinkat(entry.symlink_target->c_str(), workspace.parent_descriptor(),
                  workspace.prepared_name().c_str()) != 0)
    return false;
  if (!apply_path_metadata(workspace.parent_descriptor(),
                           workspace.prepared_name(), entry, true))
  {
    remove_prepared(workspace, entry.type);
    return false;
  }
  const auto status = stat_leaf(
      workspace.parent_descriptor(), workspace.prepared_name());
  if (!status || !S_ISLNK(status->st_mode) ||
      static_cast<std::uint32_t>(status->st_mode & 07777) !=
          (entry.mode & 07777U))
  {
    remove_prepared(workspace, entry.type);
    return false;
  }
  return true;
}

[[nodiscard]] bool
prepare_special(const active_path_workspace& workspace,
                const pkgimage::package_entry& entry)
{
  mode_t type = 0;
  dev_t device = 0;
  switch (entry.type) {
    case pkgimage::entry_type::fifo:
      type = S_IFIFO;
      break;
    case pkgimage::entry_type::character_device:
      if (!entry.device)
        return false;
      type = S_IFCHR;
      device = ::makedev(entry.device->major, entry.device->minor);
      break;
    case pkgimage::entry_type::block_device:
      if (!entry.device)
        return false;
      type = S_IFBLK;
      device = ::makedev(entry.device->major, entry.device->minor);
      break;
    default:
      return false;
  }
  if (::mknodat(workspace.parent_descriptor(),
                workspace.prepared_name().c_str(), type | 0600, device) != 0)
    return false;
  if (!apply_path_metadata(workspace.parent_descriptor(),
                           workspace.prepared_name(), entry, false))
  {
    remove_prepared(workspace, entry.type);
    return false;
  }
  return true;
}

[[nodiscard]] bool
directory_empty(int parent, const std::string& name)
{
  unique_fd directory(::openat(
      parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.get() < 0)
    return false;
  DIR* stream = ::fdopendir(directory.release());
  if (stream == nullptr)
    return false;
  bool empty = true;
  errno = 0;
  while (dirent* entry = ::readdir(stream)) {
    if (std::strcmp(entry->d_name, ".") != 0 &&
        std::strcmp(entry->d_name, "..") != 0)
    {
      empty = false;
      break;
    }
  }
  const int saved = errno;
  static_cast<void>(::closedir(stream));
  if (saved != 0)
    return false;
  return empty;
}

[[nodiscard]] bool
prepare_hardlink(const application_active_workspace& roots,
                 const active_path_workspace& workspace,
                 const pkgimage::package_entry& entry)
{
  if (!entry.hardlink_target)
    return false;
  active_path_workspace anchor = roots.open(
      pkgplan::package_path::parse(entry.hardlink_target->string()));
  if (::linkat(anchor.parent_descriptor(), anchor.leaf().c_str(),
               workspace.parent_descriptor(),
               workspace.prepared_name().c_str(), 0) != 0)
    return false;
  const auto anchor_status = stat_leaf(
      anchor.parent_descriptor(), anchor.leaf());
  const auto prepared_status = stat_leaf(
      workspace.parent_descriptor(), workspace.prepared_name());
  if (!anchor_status || !prepared_status ||
      !S_ISREG(anchor_status->st_mode) ||
      anchor_status->st_dev != prepared_status->st_dev ||
      anchor_status->st_ino != prepared_status->st_ino)
  {
    remove_prepared(workspace, entry.type);
    return false;
  }
  return true;
}

struct prepared_publication final {
  backend_operation_result result;
  int object_descriptor;
  int parent_descriptor;
};

[[nodiscard]] prepared_publication
publish_prepared(active_path_workspace& workspace,
                 const pkgimage::package_entry& entry,
                 unique_fd prepared_descriptor)
{
  const auto final_status = stat_leaf(
      workspace.parent_descriptor(), workspace.leaf());
  const bool final_directory =
      final_status && S_ISDIR(final_status->st_mode);

  if (final_directory && entry.type != pkgimage::entry_type::directory) {
    if (!directory_empty(workspace.parent_descriptor(), workspace.leaf())) {
      remove_prepared(workspace, entry.type);
      return {operation(backend_operation_outcome::failed), -1, -1};
    }
    if (::renameat(workspace.parent_descriptor(), workspace.leaf().c_str(),
                   workspace.parent_descriptor(),
                   workspace.displaced_name().c_str()) != 0)
    {
      remove_prepared(workspace, entry.type);
      return {operation(backend_operation_outcome::failed), -1, -1};
    }
    if (!directory_empty(
            workspace.parent_descriptor(), workspace.displaced_name()))
    {
      const bool restored = ::renameat(
          workspace.parent_descriptor(), workspace.displaced_name().c_str(),
          workspace.parent_descriptor(), workspace.leaf().c_str()) == 0;
      remove_prepared(workspace, entry.type);
      return {operation(restored ? backend_operation_outcome::failed
                                 : backend_operation_outcome::indeterminate),
              -1, -1};
    }
  } else if (entry.type == pkgimage::entry_type::directory && final_status &&
             !final_directory)
  {
    if (::renameat(workspace.parent_descriptor(), workspace.leaf().c_str(),
                   workspace.parent_descriptor(),
                   workspace.displaced_name().c_str()) != 0)
    {
      remove_prepared(workspace, entry.type);
      return {operation(backend_operation_outcome::failed), -1, -1};
    }
  }

  if (::renameat(workspace.parent_descriptor(),
                 workspace.prepared_name().c_str(),
                 workspace.parent_descriptor(), workspace.leaf().c_str()) != 0)
  {
    const auto displaced = stat_leaf(
        workspace.parent_descriptor(), workspace.displaced_name());
    if (displaced) {
      static_cast<void>(::renameat(
          workspace.parent_descriptor(), workspace.displaced_name().c_str(),
          workspace.parent_descriptor(), workspace.leaf().c_str()));
    }
    remove_prepared(workspace, entry.type);
    return {operation(backend_operation_outcome::indeterminate), -1, -1};
  }

  return {operation(backend_operation_outcome::completed),
          prepared_descriptor.release(),
          duplicate_fd(workspace.parent_descriptor())};
}

[[nodiscard]] backend_operation_result
update_existing_directory(application_active_namespace& target,
                          active_path_workspace& workspace,
                          const pkgimage::package_entry& entry)
{
  unique_fd directory(::openat(
      workspace.parent_descriptor(), workspace.leaf().c_str(),
      O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (directory.get() < 0)
    return operation(backend_operation_outcome::indeterminate);
  struct stat before {};
  if (::fstat(directory.get(), &before) != 0)
    return operation(backend_operation_outcome::indeterminate);

  const bool owner_matches =
      static_cast<std::uint64_t>(before.st_uid) == entry.uid &&
      static_cast<std::uint64_t>(before.st_gid) == entry.gid;
  const bool mode_matches =
      static_cast<std::uint32_t>(before.st_mode & 07777) ==
      (entry.mode & 07777U);
  const bool time_matches = before.st_mtim.tv_sec == entry.mtime &&
      static_cast<std::uint32_t>(before.st_mtim.tv_nsec) ==
      entry.mtime_nanoseconds;
  if (owner_matches && mode_matches && time_matches)
    return operation(backend_operation_outcome::completed);

  bool changed = false;
  if (!owner_matches) {
    if (::fchown(directory.get(), static_cast<uid_t>(entry.uid),
                 static_cast<gid_t>(entry.gid)) != 0)
      return operation(backend_operation_outcome::failed);
    changed = true;
  }
  if (!mode_matches) {
    if (::fchmod(
            directory.get(),
            static_cast<mode_t>(entry.mode & 07777U)) != 0)
      return operation(changed ? backend_operation_outcome::indeterminate
                               : backend_operation_outcome::failed);
    changed = true;
  }
  if (!time_matches) {
    const struct timespec times[2] = {
        {entry.mtime, static_cast<long>(entry.mtime_nanoseconds)},
        {entry.mtime, static_cast<long>(entry.mtime_nanoseconds)},
    };
    if (::futimens(directory.get(), times) != 0)
      return operation(changed ? backend_operation_outcome::indeterminate
                               : backend_operation_outcome::failed);
    changed = true;
  }
  if (changed)
    target.retain_dirty_descriptor(directory.release());
  return operation(backend_operation_outcome::completed);
}

} // namespace

application_active_namespace application_active_namespace::bind(
    int target_root_fd,
    application_attempt attempt,
    const pkgimage::package_image& incoming_image,
    const sealed_application_payloads* payloads,
    std::vector<application_path_observation> admitted)
{
  application_active_workspace workspace =
      application_active_workspace::from_directory_fd(
          target_root_fd, std::move(attempt));
  validate_binding(workspace, incoming_image, payloads);
  validate_hard_links(incoming_image);
  std::sort(
      admitted.begin(), admitted.end(),
      [](const auto& lhs, const auto& rhs) {
        return lhs.path() < rhs.path();
      });
  const auto duplicate = std::adjacent_find(
      admitted.begin(), admitted.end(),
      [](const auto& lhs, const auto& rhs) {
        return lhs.path() == rhs.path();
      });
  if (duplicate != admitted.end())
    throw std::invalid_argument("duplicate admitted active observation");
  return application_active_namespace(
      std::move(workspace), &incoming_image, payloads, std::move(admitted));
}

application_active_namespace::application_active_namespace(
    application_active_workspace workspace,
    const pkgimage::package_image* incoming_image,
    const sealed_application_payloads* payloads,
    std::vector<application_path_observation> admitted)
    : workspace_(std::move(workspace)), incoming_image_(incoming_image),
      payloads_(payloads), admitted_(std::move(admitted))
{
}

application_active_namespace::application_active_namespace(
    application_active_namespace&& other) noexcept
    : workspace_(std::move(other.workspace_)),
      incoming_image_(other.incoming_image_), payloads_(other.payloads_),
      admitted_(std::move(other.admitted_)),
      dirty_descriptors_(std::move(other.dirty_descriptors_))
{
  other.incoming_image_ = nullptr;
  other.payloads_ = nullptr;
  other.dirty_descriptors_.clear();
}

application_active_namespace& application_active_namespace::operator=(
    application_active_namespace&& other) noexcept
{
  if (this != &other) {
    for (int descriptor : dirty_descriptors_)
      static_cast<void>(::close(descriptor));
    workspace_ = std::move(other.workspace_);
    incoming_image_ = other.incoming_image_;
    payloads_ = other.payloads_;
    admitted_ = std::move(other.admitted_);
    dirty_descriptors_ = std::move(other.dirty_descriptors_);
    other.incoming_image_ = nullptr;
    other.payloads_ = nullptr;
    other.dirty_descriptors_.clear();
  }
  return *this;
}

application_active_namespace::~application_active_namespace()
{
  for (int descriptor : dirty_descriptors_)
    static_cast<void>(::close(descriptor));
}

const application_path_observation* application_active_namespace::admitted(
    const pkgplan::package_path& path) const noexcept
{
  const auto found = std::lower_bound(
      admitted_.begin(), admitted_.end(), path,
      [](const auto& observation, const auto& expected) {
        return observation.path() < expected;
      });
  return found != admitted_.end() && found->path() == path ? &*found : nullptr;
}

void application_active_namespace::retain_dirty_descriptor(int descriptor)
{
  if (descriptor < 0)
    throw std::invalid_argument("invalid active durability descriptor");
  dirty_descriptors_.push_back(descriptor);
}

backend_operation_result application_active_namespace::publish_incoming(
    const backend_active_effect_request& request)
{
  if (incoming_image_ == nullptr)
    throw std::logic_error("active namespace has no incoming image");
  const auto& entry = resolve_entry(*incoming_image_, request);
  const auto* before = admitted(request.path());
  if (before == nullptr)
    throw std::invalid_argument("active request lacks admitted observation");

  active_path_workspace workspace = workspace_.open(request.path());
  if (workspace.inspect().state() != active_workspace_state::clear)
    return operation(backend_operation_outcome::failed);
  if (!still_admitted(workspace_, *before))
    return operation(backend_operation_outcome::indeterminate);

  const auto current = stat_leaf(
      workspace.parent_descriptor(), workspace.leaf());
  if (entry.type == pkgimage::entry_type::directory && current &&
      S_ISDIR(current->st_mode))
  {
    return update_existing_directory(*this, workspace, entry);
  }

  unique_fd prepared;
  bool prepared_without_descriptor = false;
  switch (entry.type) {
    case pkgimage::entry_type::regular:
      prepared = prepare_regular(workspace, entry, payloads_);
      if (prepared.get() < 0)
        return operation(backend_operation_outcome::failed);
      break;
    case pkgimage::entry_type::directory:
      prepared = prepare_directory(workspace, entry);
      if (prepared.get() < 0)
        return operation(backend_operation_outcome::failed);
      break;
    case pkgimage::entry_type::symlink:
      prepared_without_descriptor = prepare_symlink(workspace, entry);
      break;
    case pkgimage::entry_type::hardlink:
      prepared_without_descriptor = prepare_hardlink(
          workspace_, workspace, entry);
      break;
    case pkgimage::entry_type::fifo:
    case pkgimage::entry_type::character_device:
    case pkgimage::entry_type::block_device:
      prepared_without_descriptor = prepare_special(workspace, entry);
      break;
  }
  if (prepared.get() < 0 && !prepared_without_descriptor)
    return operation(backend_operation_outcome::failed);
  prepared_publication published =
      publish_prepared(workspace, entry, std::move(prepared));
  if (published.object_descriptor >= 0)
    retain_dirty_descriptor(published.object_descriptor);
  if (published.parent_descriptor >= 0)
    retain_dirty_descriptor(published.parent_descriptor);
  return std::move(published.result);
}

application_durability_fact application_active_namespace::synchronize()
{
  for (int descriptor : dirty_descriptors_) {
    int result;
    do {
      result = ::fsync(descriptor);
    } while (result != 0 && errno == EINTR);
    if (result != 0) {
      return application_durability_fact(
          application_durability_domain::active_namespace,
          application_durability_status::unconfirmed);
    }
  }
  for (int descriptor : dirty_descriptors_)
    static_cast<void>(::close(descriptor));
  dirty_descriptors_.clear();
  return application_durability_fact(
      application_durability_domain::active_namespace,
      application_durability_status::confirmed);
}

} // namespace pkgapply::posix::detail
