// SPDX-License-Identifier: GPL-3.0-or-later
#include "recovery_operation_engine.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

using infiltrator::files::OperationStatus;
using infiltrator::files::RecoveryOperationEngine;

namespace {

bool write_text(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    return stream.good();
}

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

} // namespace

int main()
{
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() /
                          ("infiltrator-files-recovery-test-" + std::to_string(::getpid()));
    std::error_code cleanup_error;
    fs::remove_all(root, cleanup_error);
    fs::create_directories(root);

    RecoveryOperationEngine engine;

    const fs::path copy_source_dir = root / "copy-source";
    const fs::path copy_destination_dir = root / "copy-destination";
    fs::create_directories(copy_source_dir);
    fs::create_directories(copy_destination_dir);
    const fs::path copy_source = copy_source_dir / "same.txt";
    const fs::path copy_destination = copy_destination_dir / "same.txt";
    if (!write_text(copy_source, "new-copy\n") ||
        !write_text(copy_destination, "old-copy\n")) {
        fs::remove_all(root, cleanup_error);
        return 1;
    }

    const auto replaced_copy = engine.replace_copy(copy_source, copy_destination_dir);
    if (!replaced_copy.ok() || !replaced_copy.changed || !fs::exists(copy_source) ||
        read_text(copy_destination) != "new-copy\n") {
        fs::remove_all(root, cleanup_error);
        return 2;
    }

    for (const auto &entry : fs::directory_iterator(copy_destination_dir)) {
        if (entry.path().filename().string().starts_with(".infiltrator-replace-")) {
            fs::remove_all(root, cleanup_error);
            return 3;
        }
    }

    const fs::path move_source_dir = root / "move-source";
    const fs::path move_destination_dir = root / "move-destination";
    fs::create_directories(move_source_dir);
    fs::create_directories(move_destination_dir);
    const fs::path move_source = move_source_dir / "same.txt";
    const fs::path move_destination = move_destination_dir / "same.txt";
    if (!write_text(move_source, "new-move\n") ||
        !write_text(move_destination, "old-move\n")) {
        fs::remove_all(root, cleanup_error);
        return 4;
    }

    const auto replaced_move = engine.replace_move(move_source, move_destination_dir);
    if (!replaced_move.ok() || !replaced_move.changed || fs::exists(move_source) ||
        read_text(move_destination) != "new-move\n") {
        fs::remove_all(root, cleanup_error);
        return 5;
    }

    const fs::path delete_tree = root / "delete-me";
    fs::create_directories(delete_tree / "nested");
    if (!write_text(delete_tree / "nested" / "payload.txt", "delete\n")) {
        fs::remove_all(root, cleanup_error);
        return 6;
    }

    const auto deleted = engine.delete_item(delete_tree);
    if (!deleted.ok() || !deleted.changed || fs::exists(delete_tree)) {
        fs::remove_all(root, cleanup_error);
        return 7;
    }

    const auto missing_delete = engine.delete_item(delete_tree);
    if (missing_delete.status != OperationStatus::InvalidRequest || missing_delete.changed) {
        fs::remove_all(root, cleanup_error);
        return 8;
    }

    fs::remove_all(root, cleanup_error);
    return 0;
}
