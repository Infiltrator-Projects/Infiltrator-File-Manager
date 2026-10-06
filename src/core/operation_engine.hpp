// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace infiltrator::files {

enum class OperationPhase {
    Preflight,
    Execute,
    Verify,
    Complete,
};

enum class OperationStatus {
    Success,
    InvalidRequest,
    DestinationConflict,
    PermissionFailure,
    ReadOnlyLocation,
    ExecutionFailure,
    VerificationFailure,
};

struct OperationResult {
    OperationStatus status{OperationStatus::ExecutionFailure};
    OperationPhase phase{OperationPhase::Preflight};
    std::filesystem::path destination;
    std::string message;
    bool changed{false};

    [[nodiscard]] bool ok() const noexcept { return status == OperationStatus::Success; }
};

class OperationEngine final {
public:
    [[nodiscard]] OperationResult create_directory(const std::filesystem::path &parent,
                                                   std::string_view name) const;

    [[nodiscard]] static bool validate_directory_name(std::string_view name,
                                                      std::string *reason = nullptr);
    [[nodiscard]] static OperationStatus status_for_error(const std::error_code &error) noexcept;
};

} // namespace infiltrator::files
