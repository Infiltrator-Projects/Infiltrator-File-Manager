// SPDX-License-Identifier: GPL-3.0-or-later
#include "transfer_operation_engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

using infiltrator::files::ConflictPolicy;
using infiltrator::files::OperationStatus;
using infiltrator::files::TransferControl;
using infiltrator::files::TransferOperationEngine;

namespace {

bool write_text(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    return stream.good();
}

} // namespace

int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
                          ("infiltrator-files-transfer-test-" + std::to_string(::getpid()));
    std::error_code cleanup_error;
    fs::remove_all(root, cleanup_error);
    fs::create_directories(root / "destination");

    TransferOperationEngine engine;
    const fs::path source = root / "report.txt";
    if (!write_text(source, "transfer payload\n")) {
        return 1;
    }

    TransferControl first_control;
    const auto first = engine.copy_item(source,
                                        root / "destination",
                                        ConflictPolicy::Fail,
                                        &first_control);
    if (!first.ok() || !first.changed || !fs::exists(root / "destination" / "report.txt")) {
        fs::remove_all(root, cleanup_error);
        return 2;
    }
    const auto first_progress = first_control.progress();
    if (first_progress.bytes_total == 0U ||
        first_progress.bytes_done != first_progress.bytes_total ||
        first_progress.items_done != first_progress.items_total) {
        fs::remove_all(root, cleanup_error);
        return 3;
    }

    const auto conflict = engine.copy_item(source,
                                           root / "destination",
                                           ConflictPolicy::Fail);
    if (conflict.status != OperationStatus::DestinationConflict || conflict.changed) {
        fs::remove_all(root, cleanup_error);
        return 4;
    }

    const auto skipped = engine.copy_item(source,
                                          root / "destination",
                                          ConflictPolicy::Skip);
    if (!skipped.ok() || skipped.changed) {
        fs::remove_all(root, cleanup_error);
        return 5;
    }

    const auto kept = engine.copy_item(source,
                                       root / "destination",
                                       ConflictPolicy::KeepBoth);
    if (!kept.ok() || !kept.changed ||
        kept.destination.filename() != "report (copy).txt" ||
        !fs::exists(kept.destination)) {
        fs::remove_all(root, cleanup_error);
        return 6;
    }

    const auto kept_again = engine.copy_item(source,
                                             root / "destination",
                                             ConflictPolicy::KeepBoth);
    if (!kept_again.ok() ||
        kept_again.destination.filename() != "report (copy 2).txt" ||
        !fs::exists(kept_again.destination)) {
        fs::remove_all(root, cleanup_error);
        return 7;
    }

    TransferControl cancelled_control;
    cancelled_control.request_cancel();
    fs::create_directories(root / "cancel-destination");
    const auto cancelled = engine.copy_item(source,
                                            root / "cancel-destination",
                                            ConflictPolicy::Fail,
                                            &cancelled_control);
    if (cancelled.status != OperationStatus::Cancelled || cancelled.changed ||
        fs::exists(root / "cancel-destination" / "report.txt")) {
        fs::remove_all(root, cleanup_error);
        return 8;
    }

    const fs::path tree = root / "tree";
    fs::create_directories(tree / "nested");
    if (!write_text(tree / "nested" / "data.bin", "tree payload\n")) {
        fs::remove_all(root, cleanup_error);
        return 9;
    }
    fs::create_directories(root / "tree-destination");
    TransferControl tree_control;
    const auto tree_copy = engine.copy_item(tree,
                                            root / "tree-destination",
                                            ConflictPolicy::Fail,
                                            &tree_control);
    if (!tree_copy.ok() ||
        !fs::is_regular_file(root / "tree-destination" / "tree" / "nested" / "data.bin")) {
        fs::remove_all(root, cleanup_error);
        return 10;
    }
    const auto tree_progress = tree_control.progress();
    if (tree_progress.items_total < 3U || tree_progress.items_done != tree_progress.items_total) {
        fs::remove_all(root, cleanup_error);
        return 11;
    }

    const fs::path movable = root / "movable.txt";
    if (!write_text(movable, "move payload\n")) {
        fs::remove_all(root, cleanup_error);
        return 12;
    }
    fs::create_directories(root / "move-destination");
    if (!write_text(root / "move-destination" / "movable.txt", "occupied\n")) {
        fs::remove_all(root, cleanup_error);
        return 13;
    }
    const auto moved = engine.move_item(movable,
                                        root / "move-destination",
                                        ConflictPolicy::KeepBoth);
    if (!moved.ok() || fs::exists(movable) ||
        moved.destination.filename() != "movable (copy).txt" ||
        !fs::exists(moved.destination)) {
        fs::remove_all(root, cleanup_error);
        return 14;
    }

    fs::remove_all(root, cleanup_error);
    return 0;
}
