// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "operation_engine.hpp"
#include "transfer_operation_engine.hpp"

#include <filesystem>
#include <vector>

namespace infiltrator::files {

enum class BatchTransferKind {
    Copy,
    Move,
};

enum class BatchConflictPolicy {
    Fail,
    Skip,
    KeepBoth,
    Replace,
};

class BatchTransferEngine final {
public:
    [[nodiscard]] OperationResult execute(
        const std::vector<std::filesystem::path> &sources,
        const std::filesystem::path &destination_parent,
        BatchTransferKind kind,
        BatchConflictPolicy conflict_policy,
        TransferControl *control = nullptr) const;
};

} // namespace infiltrator::files
