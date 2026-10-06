// SPDX-License-Identifier: GPL-3.0-or-later
#include "operation_engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

using infiltrator::files::OperationEngine;
using infiltrator::files::OperationStatus;

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
                          ("infiltrator-files-operation-test-" + std::to_string(::getpid()));
    std::error_code cleanup_error;
    fs::remove_all(root, cleanup_error);
    fs::create_directories(root);

    OperationEngine engine;

    const auto create_success = engine.create_directory(root, "created");
    if (!create_success.ok() || !create_success.changed || !fs::is_directory(root / "created")) {
        fs::remove_all(root, cleanup_error);
        return 1;
    }

    const auto create_conflict = engine.create_directory(root, "created");
    if (create_conflict.status != OperationStatus::DestinationConflict || create_conflict.changed) {
        fs::remove_all(root, cleanup_error);
        return 2;
    }

    std::string reason;
    if (OperationEngine::validate_item_name("", &reason) || reason.empty() ||
        OperationEngine::validate_item_name(".") ||
        OperationEngine::validate_item_name("..") ||
        OperationEngine::validate_item_name("a/b")) {
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

    const fs::path original = root / "original.txt";
    if (!write_text(original, "rename-copy-move\n")) {
        fs::remove_all(root, cleanup_error);
        return 6;
    }

    const auto rename_success = engine.rename_item(original, "renamed.txt");
    const fs::path renamed = root / "renamed.txt";
    if (!rename_success.ok() || !rename_success.changed || fs::exists(original) ||
        !fs::is_regular_file(renamed)) {
        fs::remove_all(root, cleanup_error);
        return 7;
    }

    if (!write_text(root / "occupied.txt", "occupied\n")) {
        fs::remove_all(root, cleanup_error);
        return 8;
    }
    const auto rename_conflict = engine.rename_item(renamed, "occupied.txt");
    if (rename_conflict.status != OperationStatus::DestinationConflict ||
        rename_conflict.changed || !fs::exists(renamed)) {
        fs::remove_all(root, cleanup_error);
        return 9;
    }

    const fs::path copy_destination = root / "copy-destination";
    fs::create_directories(copy_destination);
    const auto copy_success = engine.copy_item(renamed, copy_destination);
    const fs::path copied_file = copy_destination / renamed.filename();
    if (!copy_success.ok() || !copy_success.changed || !fs::exists(renamed) ||
        !fs::is_regular_file(copied_file) || fs::file_size(copied_file) != fs::file_size(renamed)) {
        fs::remove_all(root, cleanup_error);
        return 10;
    }

    const auto copy_conflict = engine.copy_item(renamed, copy_destination);
    if (copy_conflict.status != OperationStatus::DestinationConflict || copy_conflict.changed) {
        fs::remove_all(root, cleanup_error);
        return 11;
    }

    const fs::path source_tree = root / "source-tree";
    fs::create_directories(source_tree / "nested");
    if (!write_text(source_tree / "nested" / "data.bin", "tree payload\n")) {
        fs::remove_all(root, cleanup_error);
        return 12;
    }
    const fs::path tree_destination = root / "tree-destination";
    fs::create_directories(tree_destination);
    const auto tree_copy = engine.copy_item(source_tree, tree_destination);
    if (!tree_copy.ok() ||
        !fs::is_regular_file(tree_destination / "source-tree" / "nested" / "data.bin")) {
        fs::remove_all(root, cleanup_error);
        return 13;
    }

    const auto recursive_copy_rejected = engine.copy_item(source_tree, source_tree / "nested");
    if (recursive_copy_rejected.status != OperationStatus::InvalidRequest ||
        recursive_copy_rejected.changed) {
        fs::remove_all(root, cleanup_error);
        return 14;
    }

    const fs::path move_destination = root / "move-destination";
    fs::create_directories(move_destination);
    const auto move_success = engine.move_item(copied_file, move_destination);
    const fs::path moved_file = move_destination / copied_file.filename();
    if (!move_success.ok() || !move_success.changed || fs::exists(copied_file) ||
        !fs::is_regular_file(moved_file)) {
        fs::remove_all(root, cleanup_error);
        return 15;
    }

    const fs::path move_tree_destination = root / "move-tree-destination";
    fs::create_directories(move_tree_destination);
    const auto move_tree = engine.move_item(source_tree, move_tree_destination);
    if (!move_tree.ok() || !move_tree.changed || fs::exists(source_tree) ||
        !fs::is_regular_file(move_tree_destination / "source-tree" / "nested" / "data.bin")) {
        fs::remove_all(root, cleanup_error);
        return 16;
    }

    fs::remove_all(root, cleanup_error);
    return 0;
}
