// SPDX-License-Identifier: GPL-3.0-or-later
#include "destination_ownership.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

namespace {

bool require(const bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

bool write_text(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream output(path);
    output << text;
    return output.good();
}

} // namespace

int main()
{
    namespace ownership = infiltrator::files::destination_ownership;

    const auto root = std::filesystem::temp_directory_path() /
                      ("infiltrator-destination-ownership-" +
                       std::to_string(static_cast<long long>(::getpid())));
    std::error_code error;
    std::filesystem::remove_all(root, error);
    error.clear();
    if (!require(std::filesystem::create_directories(root, error) && !error,
                 "could not create test root")) {
        return 1;
    }

    const auto source = root / "source";
    const auto destination = root / "destination";
    if (!require(write_text(source, "source") &&
                     write_text(destination, "competing writer"),
                 "could not create rename fixture")) {
        return 2;
    }

    const bool renamed = ownership::rename_no_replace(source, destination, error);
    if (!require(!renamed && error == std::errc::file_exists &&
                     read_text(source) == "source" &&
                     read_text(destination) == "competing writer",
                 "no-replace rename overwrote a competing destination")) {
        return 3;
    }

    error.clear();
    const int contested = ownership::open_exclusive(destination, error);
    if (!require(contested < 0 && error == std::errc::file_exists &&
                     read_text(destination) == "competing writer",
                 "exclusive file creation accepted a competing destination")) {
        if (contested >= 0) {
            (void)::close(contested);
        }
        return 4;
    }

    const auto competing_directory = root / "competing-directory";
    error.clear();
    if (!require(std::filesystem::create_directory(competing_directory, error) && !error,
                 "could not create competing directory fixture")) {
        return 5;
    }
    error.clear();
    if (!require(!ownership::create_directory_exclusive(competing_directory, error) &&
                     error == std::errc::file_exists,
                 "exclusive directory creation accepted a competing directory")) {
        return 6;
    }

    const auto owned_directory = root / "owned-directory";
    error.clear();
    if (!require(ownership::create_directory_exclusive(owned_directory, error),
                 "could not create owned directory")) {
        return 7;
    }
    ownership::OwnedOutputs directory_outputs;
    if (!require(ownership::record(owned_directory, directory_outputs, error),
                 "could not record owned directory")) {
        return 8;
    }

    const auto owned_file = owned_directory / "owned";
    const int descriptor = ownership::open_exclusive(owned_file, error);
    if (!require(descriptor >= 0, "could not create owned file")) {
        return 9;
    }
    if (!require(ownership::record_fd(owned_file, descriptor, directory_outputs, error) &&
                     ::write(descriptor, "owned", 5) == 5 &&
                     ::close(descriptor) == 0,
                 "could not record or write owned file")) {
        return 10;
    }

    const auto competing_file = owned_directory / "competing";
    if (!require(write_text(competing_file, "competing writer"),
                 "could not create nested competitor")) {
        return 11;
    }
    error.clear();
    if (!require(!ownership::cleanup_owned(directory_outputs, error) &&
                     !std::filesystem::exists(owned_file) &&
                     read_text(competing_file) == "competing writer" &&
                     std::filesystem::is_directory(owned_directory),
                 "cleanup removed or displaced non-owned directory content")) {
        return 12;
    }

    const auto raced_path = root / "raced";
    const auto displaced_owned = root / "displaced-owned";
    if (!require(write_text(raced_path, "owned"), "could not create race fixture")) {
        return 13;
    }
    ownership::OwnedOutputs raced_outputs;
    if (!require(ownership::record(raced_path, raced_outputs, error),
                 "could not record race fixture identity")) {
        return 14;
    }
    error.clear();
    std::filesystem::rename(raced_path, displaced_owned, error);
    if (!require(!error && write_text(raced_path, "competing writer"),
                 "could not install competing replacement")) {
        return 15;
    }
    error.clear();
    if (!require(!ownership::cleanup_owned(raced_outputs, error) &&
                     read_text(raced_path) == "competing writer" &&
                     read_text(displaced_owned) == "owned",
                 "cleanup deleted the object that replaced an owned pathname")) {
        return 16;
    }

    const auto clean_path = root / "clean-owned";
    if (!require(write_text(clean_path, "owned"), "could not create cleanup fixture")) {
        return 17;
    }
    ownership::OwnedOutputs clean_outputs;
    if (!require(ownership::record(clean_path, clean_outputs, error),
                 "could not record cleanup fixture")) {
        return 18;
    }
    error.clear();
    if (!require(ownership::cleanup_owned(clean_outputs, error) &&
                     !std::filesystem::exists(clean_path),
                 "cleanup did not remove an unchanged owned object")) {
        return 19;
    }

    const auto staged = root / "staged-previous-destination";
    if (!require(write_text(staged, "previous destination"),
                 "could not create rollback fixture")) {
        return 20;
    }
    error.clear();
    if (!require(!ownership::rename_no_replace(staged, destination, error) &&
                     error == std::errc::file_exists &&
                     read_text(staged) == "previous destination" &&
                     read_text(destination) == "competing writer",
                 "rollback displaced a competing destination")) {
        return 21;
    }

    const auto proof_root = root / "proof-root";
    const auto proof_child = proof_root / "child";
    const auto displaced_child = root / "displaced-proof-child";
    error.clear();
    if (!require(std::filesystem::create_directory(proof_root, error) &&
                     write_text(proof_child, "owned child"),
                 "could not create complete-proof fixture")) {
        return 22;
    }
    ownership::OwnedOutputs proof_outputs;
    if (!require(ownership::record(proof_root, proof_outputs, error) &&
                     ownership::record(proof_child, proof_outputs, error),
                 "could not record complete-proof fixture")) {
        return 23;
    }
    std::filesystem::rename(proof_child, displaced_child, error);
    if (!require(!error && write_text(proof_child, "competing child"),
                 "could not replace a copied child")) {
        return 24;
    }
    error.clear();
    if (!require(!ownership::verify_all(proof_outputs, error) &&
                     read_text(proof_child) == "competing child",
                 "complete ownership proof accepted a replaced child")) {
        return 25;
    }

    const auto backup = root / "owned-backup";
    const auto displaced_backup = root / "displaced-backup";
    const auto restore_destination = root / "restore-destination";
    if (!require(write_text(backup, "staged original"),
                 "could not create staged-backup fixture")) {
        return 26;
    }
    ownership::OwnedOutputs staged_outputs;
    if (!require(ownership::record(backup, staged_outputs, error),
                 "could not record staged-backup identity")) {
        return 27;
    }
    std::filesystem::rename(backup, displaced_backup, error);
    if (!require(!error && write_text(backup, "competing backup"),
                 "could not replace staged-backup pathname")) {
        return 28;
    }
    error.clear();
    if (!require(!ownership::restore_owned(staged_outputs.front(), restore_destination, error) &&
                     !std::filesystem::exists(restore_destination) &&
                     read_text(backup) == "competing backup" &&
                     read_text(displaced_backup) == "staged original",
                 "rollback restored or displaced a non-owned staged backup")) {
        return 29;
    }

    const auto cleanup_backup = root / "cleanup-backup";
    const auto displaced_cleanup_backup = root / "displaced-cleanup-backup";
    if (!require(write_text(cleanup_backup, "owned backup"),
                 "could not create backup-cleanup fixture")) {
        return 30;
    }
    ownership::OwnedOutputs cleanup_backup_outputs;
    if (!require(ownership::record(cleanup_backup, cleanup_backup_outputs, error),
                 "could not record backup-cleanup fixture")) {
        return 31;
    }
    std::filesystem::rename(cleanup_backup, displaced_cleanup_backup, error);
    if (!require(!error && write_text(cleanup_backup, "competing backup"),
                 "could not replace backup before cleanup")) {
        return 32;
    }
    error.clear();
    if (!require(!ownership::remove_owned_tree(cleanup_backup_outputs.front(), error) &&
                     read_text(cleanup_backup) == "competing backup" &&
                     read_text(displaced_cleanup_backup) == "owned backup",
                 "successful replacement cleanup deleted a competing backup")) {
        return 33;
    }

    const auto private_public = root / "published-tree";
    ownership::OwnedOutputs private_outputs;
    const auto private_root =
        ownership::create_private_directory(private_public, private_outputs, error);
    if (!require(!private_root.empty() && write_text(private_root / "child", "private"),
                 "could not create private publication fixture")) {
        return 34;
    }
    if (!require(write_text(private_public, "competing writer"),
                 "could not claim publication destination")) {
        return 35;
    }
    error.clear();
    if (!require(!ownership::publish_private_tree(
                     private_root, private_public, private_outputs, error) &&
                     error == std::errc::file_exists &&
                     read_text(private_public) == "competing writer" &&
                     read_text(private_root / "child") == "private",
                 "private tree publication displaced a competing writer")) {
        return 36;
    }

    const auto move_source = root / "move-source";
    const auto displaced_move_source = root / "displaced-move-source";
    const auto move_target = root / "move-target";
    if (!require(write_text(move_source, "original"), "could not create move fixture")) {
        return 37;
    }
    ownership::OwnedOutputs move_outputs;
    if (!require(ownership::record(move_source, move_outputs, error),
                 "could not record move source")) {
        return 38;
    }
    std::filesystem::rename(move_source, displaced_move_source, error);
    if (!require(!error && write_text(move_source, "replacement"),
                 "could not swap move source")) {
        return 39;
    }
    ownership::OwnedOutput moved;
    error.clear();
    if (!require(!ownership::move_owned_no_replace(
                     move_outputs.front(), move_target, moved, error) &&
                     !std::filesystem::exists(move_target) &&
                     read_text(move_source) == "replacement" &&
                     read_text(displaced_move_source) == "original",
                 "identity-safe move displaced a swapped source")) {
        return 40;
    }

    const auto remaining_quarantine = root / ".infiltrator-cleanup-partial";
    const auto restored_source = root / "restored-source";
    if (!require(std::filesystem::create_directory(remaining_quarantine, error) &&
                     write_text(remaining_quarantine / "remaining", "data"),
                 "could not create partial-removal fixture")) {
        return 41;
    }
    error.clear();
    if (!require(ownership::restore_quarantine(
                     remaining_quarantine, restored_source, error) &&
                     read_text(restored_source / "remaining") == "data" &&
                     !std::filesystem::exists(remaining_quarantine),
                 "remaining quarantined source data was not restored")) {
        return 42;
    }

    const auto retained_quarantine = root / ".infiltrator-cleanup-retained";
    if (!require(write_text(retained_quarantine, "remaining") &&
                     write_text(move_target, "new source occupant"),
                 "could not create unsafe-restoration fixture")) {
        return 43;
    }
    error.clear();
    if (!require(!ownership::restore_quarantine(
                     retained_quarantine, move_target, error) &&
                     error == std::errc::file_exists &&
                     read_text(retained_quarantine) == "remaining" &&
                     read_text(move_target) == "new source occupant",
                 "unsafe source restoration displaced another writer")) {
        return 44;
    }

    error.clear();
    std::filesystem::remove_all(root, error);
    return require(!error, "could not remove test root") ? 0 : 45;
}
