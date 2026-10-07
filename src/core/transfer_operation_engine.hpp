// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "operation_engine.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>

namespace infiltrator::files {

enum class ConflictPolicy {
    Fail,
    Skip,
    KeepBoth,
};

struct TransferProgress {
    std::uintmax_t bytes_done{0};
    std::uintmax_t bytes_total{0};
    std::uintmax_t items_done{0};
    std::uintmax_t items_total{0};
};

class TransferControl final {
public:
    void request_cancel() noexcept { cancelled_.store(true, std::memory_order_relaxed); }
    [[nodiscard]] bool cancelled() const noexcept
    {
        return cancelled_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] TransferProgress progress() const noexcept;

    // Progress-reporting contract used by the transfer implementation.
    void set_total(std::uintmax_t bytes, std::uintmax_t items) noexcept;
    void add_bytes(std::uintmax_t bytes) noexcept;
    void add_item() noexcept;

private:
    std::atomic_bool cancelled_{false};
    mutable std::mutex progress_mutex_;
    TransferProgress progress_{};
};

class TransferOperationEngine final {
public:
    [[nodiscard]] OperationResult copy_item(const std::filesystem::path &source,
                                            const std::filesystem::path &destination_parent,
                                            ConflictPolicy policy,
                                            TransferControl *control = nullptr) const;
    [[nodiscard]] OperationResult move_item(const std::filesystem::path &source,
                                            const std::filesystem::path &destination_parent,
                                            ConflictPolicy policy,
                                            TransferControl *control = nullptr) const;

    [[nodiscard]] static std::filesystem::path keep_both_destination(
        const std::filesystem::path &source,
        const std::filesystem::path &destination_parent,
        std::error_code &error);
};

} // namespace infiltrator::files
