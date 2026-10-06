// SPDX-License-Identifier: GPL-3.0-or-later
#include "operation_engine.hpp"

#include <cstdint>
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

bool path_present(const std::filesystem::path &path, std::error_code &error)
{
    error.clear();
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
    if (error) {
        return false;
    }
    return status.type() != std::filesystem::file_type::not_found;
}

bool path_is_within(const std::filesystem::path &candidate,
                    const std::filesystem::path &ancestor,
                    std::error_code &error)
{
    error.clear();
    const std::filesystem::path canonical_candidate =
        std::filesystem::weakly_canonical(candidate, error);
    if (error) {
        return false;
    }
    const std::filesystem::path canonical_ancestor =
        std::filesystem::weakly_canonical(ancestor, error);
    if (error) {
        return false;
    }

    auto ancestor_it = canonical_ancestor.begin();
    auto candidate_it = canonical_candidate.begin();
    while (ancestor_it != canonical_ancestor.end()) {
        if (candidate_it == canonical_candidate.end() || *ancestor_it != *candidate_it) {
            return false;
        }
        ++ancestor_it;
        ++candidate_it;
    }
    return true;
}

bool equivalent_copy_shape(const std::filesystem::path &source,
                           const std::filesystem::path &destination,
                           std::error_code &error)
{
    error.clear();
    const std::filesystem::file_status source_status =
        std::filesystem::symlink_status(source, error);
    if (error) {
        return false;
    }
    const std::filesystem::file_status destination_status =
        std::filesystem::symlink_status(destination, error);
    if (error || source_status.type() != destination_status.type()) {
        return false;
    }

    if (std::filesystem::is_regular_file(source_status)) {
        const std::uintmax_t source_size = std::filesystem::file_size(source, error);
        if (error) {
            return false;
        }
        const std::uintmax_t destination_size = std::filesystem::file_size(destination, error);
        return !error && source_size == destination_size;
    }

    if (std::filesystem::is_symlink(source_status)) {
        const std::filesystem::path source_target = std::filesystem::read_symlink(source, error);
        if (error) {
            return false;
        }
        const std::filesystem::path destination_target =
            std::filesystem::read_symlink(destination, error);
        return !error && source_target == destination_target;
    }

    if (!std::filesystem::is_directory(source_status)) {
        return false;
    }

    std::uintmax_t source_entries = 0U;
    for (std::filesystem::recursive_directory_iterator iterator(source, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        ++source_entries;
        const std::filesystem::path relative = iterator->path().lexically_relative(source);
        if (relative.empty()) {
            return false;
        }

        const std::filesystem::path counterpart = destination / relative;
        const std::filesystem::file_status item_status = iterator->symlink_status(error);
        if (error) {
            return false;
        }
        const std::filesystem::file_status counterpart_status =
            std::filesystem::symlink_status(counterpart, error);
        if (error || item_status.type() != counterpart_status.type()) {
            return false;
        }
        if (std::filesystem::is_regular_file(item_status)) {
            const std::uintmax_t item_size = std::filesystem::file_size(iterator->path(), error);
            if (error) {
                return false;
            }
            const std::uintmax_t counterpart_size =
                std::filesystem::file_size(counterpart, error);
            if (error || item_size != counterpart_size) {
                return false;
            }
        } else if (std::filesystem::is_symlink(item_status)) {
            const std::filesystem::path item_target =
                std::filesystem::read_symlink(iterator->path(), error);
            if (error) {
                return false;
            }
            const std::filesystem::path counterpart_target =
                std::filesystem::read_symlink(counterpart, error);
            if (error || item_target != counterpart_target) {
                return false;
            }
        }
    }
    if (error) {
        return false;
    }

    std::uintmax_t destination_entries = 0U;
    for (std::filesystem::recursive_directory_iterator iterator(destination, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        ++destination_entries;
    }
    return !error && source_entries == destination_entries;
}

OperationResult preflight_source_and_parent(const std::filesystem::path &source,
                                            const std::filesystem::path &destination_parent,
                                            std::filesystem::path &destination)
{
    if (source.empty() || source.filename().empty()) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The selected item is not available.");
    }
    if (destination_parent.empty()) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The destination folder is not available.");
    }

    destination = destination_parent / source.filename();
    std::error_code error;
    const bool source_exists = path_present(source, error);
    if (error || !source_exists) {
        const OperationStatus status = error ? OperationEngine::status_for_error(error)
                                             : OperationStatus::InvalidRequest;
        return failure(status,
                       OperationPhase::Preflight,
                       destination,
                       status == OperationStatus::PermissionFailure
                           ? "You do not have permission to inspect the selected item."
                           : "The selected item no longer exists.");
    }

    const std::filesystem::file_status parent_status =
        std::filesystem::status(destination_parent, error);
    if (error) {
        const OperationStatus status = OperationEngine::status_for_error(error);
        return failure(status,
                       OperationPhase::Preflight,
                       destination,
                       status == OperationStatus::PermissionFailure
                           ? "You do not have permission to inspect the destination."
                           : "The destination folder is not available.");
    }
    if (!std::filesystem::is_directory(parent_status)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       destination,
                       "The destination is not a folder.");
    }

    const bool destination_exists = path_present(destination, error);
    if (error) {
        const OperationStatus status = OperationEngine::status_for_error(error);
        return failure(status,
                       OperationPhase::Preflight,
                       destination,
                       status == OperationStatus::PermissionFailure
                           ? "You do not have permission to inspect the destination."
                           : "The destination could not be checked.");
    }
    if (destination_exists) {
        return failure(OperationStatus::DestinationConflict,
                       OperationPhase::Preflight,
                       destination,
                       "An item named " + quoted_name(destination) + " already exists there.");
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Preflight,
                           destination,
                           {},
                           false};
}

OperationResult copy_failure(const OperationStatus status,
                             const std::filesystem::path &destination,
                             const std::error_code &error,
                             const bool changed)
{
    std::string message;
    switch (status) {
    case OperationStatus::DestinationConflict:
        message = "An item named " + quoted_name(destination) + " already exists there.";
        break;
    case OperationStatus::PermissionFailure:
        message = "You do not have permission to copy " + quoted_name(destination) + " there.";
        break;
    case OperationStatus::ReadOnlyLocation:
        message = "The destination is read-only.";
        break;
    case OperationStatus::InvalidRequest:
        message = "This item cannot be copied to that destination.";
        break;
    default:
        message = error ? "The item could not be copied: " + error.message()
                        : "The item could not be copied.";
        break;
    }
    return failure(status, OperationPhase::Execute, destination, std::move(message), changed);
}

} // namespace

bool OperationEngine::validate_item_name(const std::string_view name, std::string *reason)
{
    auto reject = [reason](const char *message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };

    if (name.empty()) {
        return reject("Name cannot be empty.");
    }
    if (name == "." || name == "..") {
        return reject("That name is reserved.");
    }
    if (name.find('/') != std::string_view::npos) {
        return reject("Names cannot contain '/'.");
    }
    if (name.find('\0') != std::string_view::npos) {
        return reject("Names cannot contain a null character.");
    }

    if (reason != nullptr) {
        reason->clear();
    }
    return true;
}

bool OperationEngine::validate_directory_name(const std::string_view name, std::string *reason)
{
    return validate_item_name(name, reason);
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

    const bool destination_exists = path_present(plan.destination, error);
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

OperationResult OperationEngine::rename_item(const std::filesystem::path &source,
                                              const std::string_view new_name) const
{
    std::string validation_error;
    if (!validate_item_name(new_name, &validation_error)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       std::move(validation_error));
    }
    if (source.empty() || source.filename().empty()) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The selected item is not available.");
    }

    const std::filesystem::path destination =
        source.parent_path() / std::filesystem::path(std::string(new_name));
    if (destination == source) {
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               destination,
                               "The name is unchanged.",
                               false};
    }

    std::error_code error;
    const bool source_exists = path_present(source, error);
    if (error || !source_exists) {
        const OperationStatus status = error ? status_for_error(error)
                                             : OperationStatus::InvalidRequest;
        return failure(status,
                       OperationPhase::Preflight,
                       destination,
                       status == OperationStatus::PermissionFailure
                           ? "You do not have permission to inspect the selected item."
                           : "The selected item no longer exists.");
    }

    const bool destination_exists = path_present(destination, error);
    if (error) {
        const OperationStatus status = status_for_error(error);
        return failure(status,
                       OperationPhase::Preflight,
                       destination,
                       "The destination could not be checked.");
    }
    if (destination_exists) {
        return failure(OperationStatus::DestinationConflict,
                       OperationPhase::Preflight,
                       destination,
                       "An item named " + quoted_name(destination) + " already exists.");
    }

    std::filesystem::rename(source, destination, error);
    if (error) {
        const OperationStatus status = status_for_error(error);
        std::string message;
        switch (status) {
        case OperationStatus::PermissionFailure:
            message = "You do not have permission to rename " + quoted_name(source) + ".";
            break;
        case OperationStatus::ReadOnlyLocation:
            message = "This location is read-only.";
            break;
        default:
            message = "The item could not be renamed: " + error.message();
            break;
        }
        return failure(status, OperationPhase::Execute, destination, std::move(message));
    }

    const bool destination_now_exists = path_present(destination, error);
    if (error || !destination_now_exists) {
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       destination,
                       "The rename completed but the new item could not be verified.",
                       true);
    }
    const bool source_still_exists = path_present(source, error);
    if (error || source_still_exists) {
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       destination,
                       "The rename completed but the old path still appears to exist.",
                       true);
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Complete,
                           destination,
                           "Renamed to " + quoted_name(destination) + ".",
                           true};
}

OperationResult OperationEngine::copy_item(const std::filesystem::path &source,
                                            const std::filesystem::path &destination_parent) const
{
    std::filesystem::path destination;
    OperationResult preflight = preflight_source_and_parent(source, destination_parent, destination);
    if (!preflight.ok()) {
        return preflight;
    }

    std::error_code error;
    const std::filesystem::file_status source_status =
        std::filesystem::symlink_status(source, error);
    if (error) {
        return failure(status_for_error(error),
                       OperationPhase::Preflight,
                       destination,
                       "The selected item could not be inspected.");
    }

    if (std::filesystem::is_directory(source_status) &&
        path_is_within(destination_parent, source, error)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       destination,
                       "A folder cannot be copied into itself or one of its descendants.");
    }
    if (error) {
        return failure(status_for_error(error),
                       OperationPhase::Preflight,
                       destination,
                       "The source and destination could not be resolved safely.");
    }

    if (std::filesystem::is_symlink(source_status)) {
        const std::filesystem::path target = std::filesystem::read_symlink(source, error);
        if (!error) {
            std::filesystem::create_symlink(target, destination, error);
        }
    } else if (std::filesystem::is_regular_file(source_status)) {
        (void)std::filesystem::copy_file(source, destination,
                                         std::filesystem::copy_options::none, error);
    } else if (std::filesystem::is_directory(source_status)) {
        std::filesystem::copy(source,
                              destination,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::copy_symlinks,
                              error);
    } else {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       destination,
                       "This item type cannot be copied yet.");
    }

    if (error) {
        const OperationStatus status = status_for_error(error);
        std::error_code cleanup_error;
        std::filesystem::remove_all(destination, cleanup_error);
        return copy_failure(status, destination, error, static_cast<bool>(cleanup_error));
    }

    if (!equivalent_copy_shape(source, destination, error)) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(destination, cleanup_error);
        if (cleanup_error) {
            return failure(OperationStatus::VerificationFailure,
                           OperationPhase::Verify,
                           destination,
                           "The copy could not be verified and its partial destination could not be removed.",
                           true);
        }
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       destination,
                       "The copy could not be verified, so the destination was removed.",
                       false);
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Complete,
                           destination,
                           "Copied " + quoted_name(source) + ".",
                           true};
}

OperationResult OperationEngine::move_item(const std::filesystem::path &source,
                                            const std::filesystem::path &destination_parent) const
{
    if (source.parent_path() == destination_parent) {
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               source,
                               "The item is already in that folder.",
                               false};
    }

    std::filesystem::path destination;
    OperationResult preflight = preflight_source_and_parent(source, destination_parent, destination);
    if (!preflight.ok()) {
        return preflight;
    }

    std::error_code error;
    const std::filesystem::file_status source_status =
        std::filesystem::symlink_status(source, error);
    if (error) {
        return failure(status_for_error(error),
                       OperationPhase::Preflight,
                       destination,
                       "The selected item could not be inspected.");
    }

    if (std::filesystem::is_directory(source_status) &&
        path_is_within(destination_parent, source, error)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       destination,
                       "A folder cannot be moved into itself or one of its descendants.");
    }
    if (error) {
        return failure(status_for_error(error),
                       OperationPhase::Preflight,
                       destination,
                       "The source and destination could not be resolved safely.");
    }

    std::filesystem::rename(source, destination, error);
    if (!error) {
        const bool destination_now_exists = path_present(destination, error);
        if (error || !destination_now_exists) {
            return failure(OperationStatus::VerificationFailure,
                           OperationPhase::Verify,
                           destination,
                           "The move completed but the destination could not be verified.",
                           true);
        }
        const bool source_still_exists = path_present(source, error);
        if (error || source_still_exists) {
            return failure(OperationStatus::VerificationFailure,
                           OperationPhase::Verify,
                           destination,
                           "The move completed but the original path still appears to exist.",
                           true);
        }
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               destination,
                               "Moved " + quoted_name(destination) + ".",
                               true};
    }

    if (error != std::errc::cross_device_link) {
        const OperationStatus status = status_for_error(error);
        const std::string message =
            status == OperationStatus::PermissionFailure
                ? "You do not have permission to move " + quoted_name(source) + "."
            : status == OperationStatus::ReadOnlyLocation
                ? "The source or destination is read-only."
                : "The item could not be moved: " + error.message();
        return failure(status, OperationPhase::Execute, destination, message);
    }

    OperationResult copied = copy_item(source, destination_parent);
    if (!copied.ok()) {
        if (!copied.message.empty()) {
            copied.message = "The cross-volume move could not copy the item first. " + copied.message;
        }
        return copied;
    }

    error.clear();
    std::filesystem::remove_all(source, error);
    if (error) {
        return failure(status_for_error(error),
                       OperationPhase::Execute,
                       destination,
                       "The item was copied to the destination, but the original could not be removed: " +
                           error.message(),
                       true);
    }

    const bool destination_now_exists = path_present(destination, error);
    if (error || !destination_now_exists) {
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       destination,
                       "The cross-volume move completed but the destination could not be verified.",
                       true);
    }
    const bool source_still_exists = path_present(source, error);
    if (error || source_still_exists) {
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       destination,
                       "The cross-volume move completed but the original path still appears to exist.",
                       true);
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Complete,
                           destination,
                           "Moved " + quoted_name(destination) + ".",
                           true};
}

} // namespace infiltrator::files
