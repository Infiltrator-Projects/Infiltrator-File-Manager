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

    error.clear();
    std::filesystem::remove_all(root, error);
    return require(!error, "could not remove test root") ? 0 : 22;
}
