// SPDX-License-Identifier: GPL-3.0-or-later
#include "transfer_operation_engine.hpp"

#include <array>
#include <fstream>
#include <string>
#include <utility>

namespace infiltrator::files {

namespace {

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
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) {
        error.clear();
        return false;
    }
    return !error && status.type() != std::filesystem::file_type::not_found;
}

bool path_is_within(const std::filesystem::path &candidate,
                    const std::filesystem::path &ancestor,
                    std::error_code &error)
{
    error.clear();
    const auto canonical_candidate = std::filesystem::weakly_canonical(candidate, error);
    if (error) {
        return false;
    }
    const auto canonical_ancestor = std::filesystem::weakly_canonical(ancestor, error);
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

OperationResult preflight_parent(const std::filesystem::path &source,
                                 const std::filesystem::path &destination_parent)
{
    if (source.empty() || source.filename().empty() || destination_parent.empty()) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The source or destination is not available.");
    }

    std::error_code error;
    if (!path_present(source, error)) {
        return failure(error ? OperationEngine::status_for_error(error)
                             : OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The selected item no longer exists.");
    }

    const auto parent_status = std::filesystem::status(destination_parent, error);
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       {},
                       "The destination folder could not be inspected.");
    }
    if (!std::filesystem::is_directory(parent_status)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "The destination is not a folder.");
    }

    return OperationResult{OperationStatus::Success, OperationPhase::Preflight, {}, {}, false};
}

bool scan_totals(const std::filesystem::path &source,
                 std::uintmax_t &bytes,
                 std::uintmax_t &items,
                 std::error_code &error)
{
    bytes = 0;
    items = 0;
    error.clear();
    const auto status = std::filesystem::symlink_status(source, error);
    if (error) {
        return false;
    }

    ++items;
    if (std::filesystem::is_regular_file(status)) {
        bytes = std::filesystem::file_size(source, error);
        return !error;
    }
    if (std::filesystem::is_symlink(status)) {
        return true;
    }
    if (!std::filesystem::is_directory(status)) {
        error = std::make_error_code(std::errc::operation_not_supported);
        return false;
    }

    for (std::filesystem::recursive_directory_iterator iterator(source, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        ++items;
        const auto entry_status = iterator->symlink_status(error);
        if (error) {
            return false;
        }
        if (std::filesystem::is_regular_file(entry_status)) {
            bytes += iterator->file_size(error);
            if (error) {
                return false;
            }
        }
    }
    return !error;
}

OperationResult cancelled_result(const std::filesystem::path &destination,
                                 const bool changed = false)
{
    return failure(OperationStatus::Cancelled,
                   OperationPhase::Execute,
                   destination,
                   "Operation cancelled.",
                   changed);
}

bool copy_regular_file(const std::filesystem::path &source,
                       const std::filesystem::path &destination,
                       TransferControl *control,
                       std::error_code &error)
{
    error.clear();
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        error = std::make_error_code(std::errc::io_error);
        return false;
    }
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = std::make_error_code(std::errc::io_error);
        return false;
    }

    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        if (control != nullptr && control->cancelled()) {
            return false;
        }
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            output.write(buffer.data(), count);
            if (!output) {
                error = std::make_error_code(std::errc::io_error);
                return false;
            }
            if (control != nullptr) {
                control->add_bytes(static_cast<std::uintmax_t>(count));
            }
        }
    }
    if (!input.eof()) {
        error = std::make_error_code(std::errc::io_error);
        return false;
    }
    output.flush();
    if (!output) {
        error = std::make_error_code(std::errc::io_error);
        return false;
    }
    if (control != nullptr) {
        control->add_item();
    }
    return true;
}

OperationResult copy_exact(const std::filesystem::path &source,
                           const std::filesystem::path &destination,
                           TransferControl *control)
{
    std::error_code error;
    std::uintmax_t bytes = 0;
    std::uintmax_t items = 0;
    if (!scan_totals(source, bytes, items, error)) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       destination,
                       "The selected item could not be measured safely.");
    }
    if (control != nullptr) {
        control->set_total(bytes, items);
        if (control->cancelled()) {
            return cancelled_result(destination);
        }
    }

    const auto source_status = std::filesystem::symlink_status(source, error);
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       destination,
                       "The selected item could not be inspected.");
    }

    if (std::filesystem::is_symlink(source_status)) {
        const auto target = std::filesystem::read_symlink(source, error);
        if (!error) {
            std::filesystem::create_symlink(target, destination, error);
        }
        if (!error && control != nullptr) {
            control->add_item();
        }
    } else if (std::filesystem::is_regular_file(source_status)) {
        if (!copy_regular_file(source, destination, control, error)) {
            if (control != nullptr && control->cancelled()) {
                std::error_code cleanup_error;
                std::filesystem::remove(destination, cleanup_error);
                return cancelled_result(destination, static_cast<bool>(cleanup_error));
            }
        }
    } else if (std::filesystem::is_directory(source_status)) {
        std::filesystem::create_directory(destination, error);
        if (!error && control != nullptr) {
            control->add_item();
        }
        for (std::filesystem::recursive_directory_iterator iterator(source, error), end;
             !error && iterator != end;
             iterator.increment(error)) {
            if (control != nullptr && control->cancelled()) {
                std::error_code cleanup_error;
                std::filesystem::remove_all(destination, cleanup_error);
                return cancelled_result(destination, static_cast<bool>(cleanup_error));
            }

            const auto relative = iterator->path().lexically_relative(source);
            const auto target = destination / relative;
            const auto entry_status = iterator->symlink_status(error);
            if (error) {
                break;
            }
            if (std::filesystem::is_directory(entry_status)) {
                std::filesystem::create_directory(target, error);
                if (!error && control != nullptr) {
                    control->add_item();
                }
            } else if (std::filesystem::is_symlink(entry_status)) {
                const auto link_target = std::filesystem::read_symlink(iterator->path(), error);
                if (!error) {
                    std::filesystem::create_symlink(link_target, target, error);
                }
                if (!error && control != nullptr) {
                    control->add_item();
                }
            } else if (std::filesystem::is_regular_file(entry_status)) {
                if (!copy_regular_file(iterator->path(), target, control, error)) {
                    if (control != nullptr && control->cancelled()) {
                        std::error_code cleanup_error;
                        std::filesystem::remove_all(destination, cleanup_error);
                        return cancelled_result(destination, static_cast<bool>(cleanup_error));
                    }
                }
            } else {
                error = std::make_error_code(std::errc::operation_not_supported);
            }
        }
    } else {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       destination,
                       "This item type cannot be transferred yet.");
    }

    if (error) {
        const auto status = OperationEngine::status_for_error(error);
        std::error_code cleanup_error;
        std::filesystem::remove_all(destination, cleanup_error);
        return failure(status,
                       OperationPhase::Execute,
                       destination,
                       "The transfer failed: " + error.message(),
                       static_cast<bool>(cleanup_error));
    }

    if (control != nullptr && control->cancelled()) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(destination, cleanup_error);
        return cancelled_result(destination, static_cast<bool>(cleanup_error));
    }

    if (!path_present(destination, error) || error) {
        return failure(OperationStatus::VerificationFailure,
                       OperationPhase::Verify,
                       destination,
                       "The transfer completed but the destination could not be verified.",
                       true);
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Complete,
                           destination,
                           "Transferred " + quoted_name(destination) + ".",
                           true};
}

struct DestinationDecision {
    std::filesystem::path destination;
    bool skip{false};
    OperationResult failure_result{};
};

DestinationDecision choose_destination(const std::filesystem::path &source,
                                       const std::filesystem::path &destination_parent,
                                       const ConflictPolicy policy)
{
    DestinationDecision decision;
    decision.destination = destination_parent / source.filename();

    std::error_code error;
    const bool exists = path_present(decision.destination, error);
    if (error) {
        decision.failure_result = failure(OperationEngine::status_for_error(error),
                                          OperationPhase::Preflight,
                                          decision.destination,
                                          "The destination could not be checked.");
        return decision;
    }
    if (!exists) {
        return decision;
    }

    if (policy == ConflictPolicy::Skip) {
        decision.skip = true;
        return decision;
    }
    if (policy == ConflictPolicy::KeepBoth) {
        decision.destination = TransferOperationEngine::keep_both_destination(
            source, destination_parent, error);
        if (error || decision.destination.empty()) {
            decision.failure_result = failure(error ? OperationEngine::status_for_error(error)
                                                    : OperationStatus::ExecutionFailure,
                                              OperationPhase::Preflight,
                                              {},
                                              "A non-conflicting destination name could not be created.");
        }
        return decision;
    }

    decision.failure_result = failure(OperationStatus::DestinationConflict,
                                      OperationPhase::Preflight,
                                      decision.destination,
                                      "An item named " + quoted_name(decision.destination) +
                                          " already exists there.");
    return decision;
}

} // namespace

TransferProgress TransferControl::progress() const noexcept
{
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return progress_;
}

void TransferControl::set_total(const std::uintmax_t bytes, const std::uintmax_t items) noexcept
{
    std::lock_guard<std::mutex> lock(progress_mutex_);
    progress_.bytes_total = bytes;
    progress_.items_total = items;
}

void TransferControl::add_bytes(const std::uintmax_t bytes) noexcept
{
    std::lock_guard<std::mutex> lock(progress_mutex_);
    progress_.bytes_done += bytes;
}

void TransferControl::add_item() noexcept
{
    std::lock_guard<std::mutex> lock(progress_mutex_);
    ++progress_.items_done;
}

std::filesystem::path TransferOperationEngine::keep_both_destination(
    const std::filesystem::path &source,
    const std::filesystem::path &destination_parent,
    std::error_code &error)
{
    error.clear();
    const std::string filename = source.filename().string();
    const auto source_status = std::filesystem::symlink_status(source, error);
    if (error) {
        return {};
    }

    std::string stem = filename;
    std::string extension;
    if (!std::filesystem::is_directory(source_status)) {
        extension = source.extension().string();
        stem = source.stem().string();
        if (stem.empty()) {
            stem = filename;
            extension.clear();
        }
    }

    for (std::uintmax_t index = 1; index < 1000000; ++index) {
        const std::string suffix = index == 1
                                       ? " (copy)"
                                       : " (copy " + std::to_string(index) + ")";
        const auto candidate = destination_parent / (stem + suffix + extension);
        if (!path_present(candidate, error)) {
            return error ? std::filesystem::path{} : candidate;
        }
        if (error) {
            return {};
        }
    }
    error = std::make_error_code(std::errc::filename_too_long);
    return {};
}

OperationResult TransferOperationEngine::copy_item(const std::filesystem::path &source,
                                                    const std::filesystem::path &destination_parent,
                                                    const ConflictPolicy policy,
                                                    TransferControl *control) const
{
    OperationResult preflight = preflight_parent(source, destination_parent);
    if (!preflight.ok()) {
        return preflight;
    }

    std::error_code error;
    const auto source_status = std::filesystem::symlink_status(source, error);
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       {},
                       "The selected item could not be inspected.");
    }
    if (std::filesystem::is_directory(source_status) &&
        path_is_within(destination_parent, source, error)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "A folder cannot be copied into itself or one of its descendants.");
    }
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       {},
                       "The source and destination could not be resolved safely.");
    }

    DestinationDecision decision = choose_destination(source, destination_parent, policy);
    if (!decision.failure_result.message.empty()) {
        return decision.failure_result;
    }
    if (decision.skip) {
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               decision.destination,
                               "Skipped " + quoted_name(source) + ".",
                               false};
    }

    OperationResult result = copy_exact(source, decision.destination, control);
    if (result.ok()) {
        result.message = "Copied " + quoted_name(source) +
                         (decision.destination.filename() == source.filename()
                              ? "."
                              : " as " + quoted_name(decision.destination) + ".");
    }
    return result;
}

OperationResult TransferOperationEngine::move_item(const std::filesystem::path &source,
                                                    const std::filesystem::path &destination_parent,
                                                    const ConflictPolicy policy,
                                                    TransferControl *control) const
{
    OperationResult preflight = preflight_parent(source, destination_parent);
    if (!preflight.ok()) {
        return preflight;
    }
    if (source.parent_path() == destination_parent) {
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               source,
                               "The item is already in that folder.",
                               false};
    }

    std::error_code error;
    const auto source_status = std::filesystem::symlink_status(source, error);
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       {},
                       "The selected item could not be inspected.");
    }
    if (std::filesystem::is_directory(source_status) &&
        path_is_within(destination_parent, source, error)) {
        return failure(OperationStatus::InvalidRequest,
                       OperationPhase::Preflight,
                       {},
                       "A folder cannot be moved into itself or one of its descendants.");
    }
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Preflight,
                       {},
                       "The source and destination could not be resolved safely.");
    }

    DestinationDecision decision = choose_destination(source, destination_parent, policy);
    if (!decision.failure_result.message.empty()) {
        return decision.failure_result;
    }
    if (decision.skip) {
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               decision.destination,
                               "Skipped " + quoted_name(source) + ".",
                               false};
    }

    std::uintmax_t bytes = 0;
    std::uintmax_t items = 0;
    if (control != nullptr && scan_totals(source, bytes, items, error)) {
        control->set_total(bytes, items);
        if (control->cancelled()) {
            return cancelled_result(decision.destination);
        }
    }
    error.clear();

    std::filesystem::rename(source, decision.destination, error);
    if (!error) {
        if (control != nullptr) {
            control->add_bytes(bytes);
            for (std::uintmax_t index = 0; index < items; ++index) {
                control->add_item();
            }
        }
        return OperationResult{OperationStatus::Success,
                               OperationPhase::Complete,
                               decision.destination,
                               "Moved " + quoted_name(decision.destination) + ".",
                               true};
    }

    if (error != std::errc::cross_device_link) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Execute,
                       decision.destination,
                       "The item could not be moved: " + error.message());
    }

    OperationResult copied = copy_exact(source, decision.destination, control);
    if (!copied.ok()) {
        return copied;
    }
    if (control != nullptr && control->cancelled()) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(decision.destination, cleanup_error);
        return cancelled_result(decision.destination, static_cast<bool>(cleanup_error));
    }

    std::filesystem::remove_all(source, error);
    if (error) {
        return failure(OperationEngine::status_for_error(error),
                       OperationPhase::Execute,
                       decision.destination,
                       "The item was copied, but the original could not be removed: " + error.message(),
                       true);
    }

    return OperationResult{OperationStatus::Success,
                           OperationPhase::Complete,
                           decision.destination,
                           "Moved " + quoted_name(decision.destination) + ".",
                           true};
}

} // namespace infiltrator::files
