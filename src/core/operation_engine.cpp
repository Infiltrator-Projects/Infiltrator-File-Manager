// SPDX-License-Identifier: GPL-3.0-or-later
#include "operation_engine.hpp"

#include <string>
#include <utility>

namespace infiltrator::files {

namespace {

struct CreateDirectoryPlan {
    std::filesystem::path parent;
    std::filesystem::path destination;
};

OperationResult failure(const OperationStatus status,
                        const OperationPhase phase,
                        const std::filesystem::path &destination,
                        std::string message,
                        const bool changed = false)
{
    return OperationResult{status, phase, destination, std::move(message), changed};
}

std::string quoted_name(const std::filesystem::path &path)
{
    return "“" + path.filename().string() + "”";
}

} // namespace

bool OperationEngine::validate_directory_name(const std::string_view name, std::string *reason)
{
    auto reject = [reason](const char *message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };

    if (name.empty()) {
        return reject("Folder name cannot be empty.");
    }
    if (name == "." || name == "..") {
        return reject("That folder name is reserved.");
    }
    if (name.find('/') != std::string_view::npos) {
        return reject("Folder names cannot contain '/'.");
    }
    if (name.find('\0') != std::string_view::npos) {
        return reject("Folder names cannot contain a null character.");
    }

    if (reason != nullptr) {
        reason->clear();
    }
    return true;
}

OperationStatus OperationEngine::status_for_error(const std::error_code &error) noexcept
{
    if (!error) {
        return OperationStatus::Success;
    }
    if (error == std::errc::file_exists) {
        return OperationStatus::DestinationConflict;
    }
    if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted) {
        return OperationStatus::PermissionFailure;
    }
    if (error == std::errc::read_only_file_system) {
        return OperationStatus::ReadOnlyLocation;
    }
    if (error == std::errc::filename_too_long || error == std::errc::invalid_argument) {
        return OperationStatus::InvalidRequest;
    }
    return OperationStatus::ExecutionFailure;
}

OperationResult OperationEngine::create_directory(const std::filesystem::path &parent,
                                                   const std::string_view name) const
{
    std::string validation_error;
    if (!validate_directory_name(name, &validation_error)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       std::move(validation_error));
    }
    if (parent.empty()) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The destination folder is not available.");
    }

    const CreateDirectoryPlan plan{
        parent,
        parent / std::filesystem::path(std::string(name)),
    };

    std::error_code error;
    const std::filesystem::file_status parent_status = std::filesystem::status(plan.parent, error);
    if (error) {
        const OperationStatus status = status_for_error(error);
        return failure(status,
                       OperationPhase::Preflight,
                       plan.destination,
                       status == OperationStatus::PermissionFailure
                           ? "You do not have permission to create a folder here."
                           : "The destination folder is not available.");
    }
    if (!std::filesystem::is_directory(parent_status)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       plan.destination,
                       "The destination is not a folder.");
    }

    const bool destination_exists = std::filesystem::exists(plan.destination, error);
    if (error) {
        const OperationStatus status = status_for_error(error);
        return failure(status,
                       OperationPhase::Preflight,
                       plan.destination,
                       status == OperationStatus::PermissionFailure
                           ? "You do not have permission to inspect the destination."
                           : "The destination could not be checked.");
    }
    if (destination_exists) {
        return failure(OperationStatus::DestinationConflict,
                       OperationPhase::Preflight,
                       plan.destination,
                       "An item named " + quoted_name(plan.destination) + " already exists.");
    }

    const bool created = std::filesystem::create_directory(plan.destination, error);
    if (error || !created) {
        const OperationStatus status = error ? status_for_error(error)
                                             : OperationStatus::DestinationConflict;
        std::string message;
        switch (status) {
        case OperationStatus::DestinationConflict:
            message = "An item named " + quoted_name(plan.destination) + " already exists.";
            break;
        case OperationStatus::PermissionFailure:
            message = "You do not have permission to create " + quoted_name(plan.destination) + " here.";
            break;
        case OperationStatus::ReadOnlyLocation:
            message = "This location is read-only.";
            break;
        case OperationStatus::InvalidRequest:
            message = "The folder name is not valid for this location.";
            break;
        default:
            message = error ? "The folder could not be created: " + error.message()
                            : "The folder could not be created.";
            break;
        }
        return failure(status, OperationPhase::Execute, plan.destination, std::move(message));
    }

    error.clear();
    const std::filesystem::file_status result_status = std::filesystem::status(plan.destination, error);
    if (error || !std::filesystem::is_directory(result_status)) {
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       plan.destination,
                       "The folder was created but could not be verified.",
                       true);
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Complete,
                           plan.destination,
                           "Created " + quoted_name(plan.destination) + ".",
                           true};
}

} // namespace infiltrator::files
