// SPDX-License-Identifier: GPL-3.0-or-later
#include "operation_engine.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

using infiltrator::files::OperationEngine;
using infiltrator::files::OperationStatus;

int main()
{
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() /
                          ("infiltrator-files-operation-test-" + std::to_string(::getpid()));
    std::error_code cleanup_error;
    fs::remove_all(root, cleanup_error);
    fs::create_directories(root);

    OperationEngine engine;

    const auto success = engine.create_directory(root, "created");
    if (!success.ok() || !success.changed || !fs::is_directory(root / "created")) {
        fs::remove_all(root, cleanup_error);
        return 1;
    }

    const auto conflict = engine.create_directory(root, "created");
    if (conflict.status != OperationStatus::DestinationConflict || conflict.changed) {
        fs::remove_all(root, cleanup_error);
        return 2;
    }

    std::string reason;
    if (OperationEngine::validate_directory_name("", &reason) || reason.empty() ||
        OperationEngine::validate_directory_name(".") ||
        OperationEngine::validate_directory_name("..") ||
        OperationEngine::validate_directory_name("a/b")) {
        fs::remove_all(root, cleanup_error);
        return 3;
    }

    if (OperationEngine::status_for_error(
            std::make_error_code(std::errc::permission_denied)) !=
        OperationStatus::PermissionFailure) {
        fs::remove_all(root, cleanup_error);
        return 4;
    }

    if (OperationEngine::status_for_error(
            std::make_error_code(std::errc::read_only_file_system)) !=
        OperationStatus::ReadOnlyLocation) {
        fs::remove_all(root, cleanup_error);
        return 5;
    }

    fs::remove_all(root, cleanup_error);
    return 0;
}
