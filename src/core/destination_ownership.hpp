// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif

namespace infiltrator::files::destination_ownership {

struct Identity {
    std::uintmax_t device{0};
    std::uintmax_t inode{0};
    bool valid{false};
};

struct OwnedOutput {
    std::filesystem::path path;
    Identity identity;
};

using OwnedOutputs = std::vector<OwnedOutput>;

inline Identity identity_for(const std::filesystem::path &path, std::error_code &error) noexcept
{
    struct stat status {};
    if (::lstat(path.c_str(), &status) != 0) {
        error = std::error_code(errno, std::generic_category());
        return {};
    }
    error.clear();
    return Identity{static_cast<std::uintmax_t>(status.st_dev),
                    static_cast<std::uintmax_t>(status.st_ino),
                    true};
}

inline Identity identity_for_fd(const int descriptor, std::error_code &error) noexcept
{
    struct stat status {};
    if (::fstat(descriptor, &status) != 0) {
        error = std::error_code(errno, std::generic_category());
        return {};
    }
    error.clear();
    return Identity{static_cast<std::uintmax_t>(status.st_dev),
                    static_cast<std::uintmax_t>(status.st_ino),
                    true};
}

inline bool same_object(const std::filesystem::path &path,
                        const Identity &expected,
                        std::error_code &error) noexcept
{
    const Identity current = identity_for(path, error);
    return !error && current.valid && expected.valid &&
           current.device == expected.device && current.inode == expected.inode;
}

inline bool record(const std::filesystem::path &path,
                   OwnedOutputs &outputs,
                   std::error_code &error) noexcept
{
    const Identity identity = identity_for(path, error);
    if (error) {
        return false;
    }
    outputs.push_back(OwnedOutput{path, identity});
    return true;
}

inline bool record_fd(const std::filesystem::path &path,
                      const int descriptor,
                      OwnedOutputs &outputs,
                      std::error_code &error) noexcept
{
    const Identity identity = identity_for_fd(descriptor, error);
    if (error) {
        return false;
    }
    outputs.push_back(OwnedOutput{path, identity});
    return true;
}

inline bool cleanup_owned(OwnedOutputs &outputs, std::error_code &first_error) noexcept
{
    bool complete = true;
    first_error.clear();
    for (auto iterator = outputs.rbegin(); iterator != outputs.rend(); ++iterator) {
        std::error_code inspect_error;
        if (!same_object(iterator->path, iterator->identity, inspect_error)) {
            if (inspect_error == std::errc::no_such_file_or_directory ||
                inspect_error == std::errc::not_a_directory) {
                continue;
            }
            if (inspect_error && !first_error) {
                first_error = inspect_error;
            }
            complete = false;
            continue;
        }

        std::error_code remove_error;
        const bool removed = std::filesystem::remove(iterator->path, remove_error);
        if (remove_error && !first_error) {
            first_error = remove_error;
        }
        if (!removed || remove_error) {
            complete = false;
        }
    }
    return complete;
}

inline int open_exclusive(const std::filesystem::path &path, std::error_code &error) noexcept
{
    const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        error = std::error_code(errno, std::generic_category());
        return -1;
    }
    error.clear();
    return descriptor;
}

inline bool rename_no_replace(const std::filesystem::path &source,
                              const std::filesystem::path &destination,
                              std::error_code &error) noexcept
{
#if defined(__linux__) && defined(SYS_renameat2)
    if (::syscall(SYS_renameat2,
                  AT_FDCWD,
                  source.c_str(),
                  AT_FDCWD,
                  destination.c_str(),
                  RENAME_NOREPLACE) == 0) {
        error.clear();
        return true;
    }
    error = std::error_code(errno, std::generic_category());
    return false;
#else
    (void)source;
    (void)destination;
    error = std::make_error_code(std::errc::operation_not_supported);
    return false;
#endif
}

} // namespace infiltrator::files::destination_ownership
