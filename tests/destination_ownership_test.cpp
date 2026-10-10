// SPDX-License-Identifier: GPL-3.0-or-later
#include "destination_ownership.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace {

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void write_text(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream output(path);
    output << text;
    assert(output.good());
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
    assert(std::filesystem::create_directories(root));

    const auto source = root / "source";
    const auto destination = root / "destination";
    write_text(source, "source");
    write_text(destination, "competing writer");

    assert(!ownership::rename_no_replace(source, destination, error));
    assert(error == std::errc::file_exists);
    assert(read_text(source) == "source");
    assert(read_text(destination) == "competing writer");

    error.clear();
    const int contested = ownership::open_exclusive(destination, error);
    assert(contested < 0);
    assert(error == std::errc::file_exists);
    assert(read_text(destination) == "competing writer");

    const auto owned_directory = root / "owned-directory";
    assert(std::filesystem::create_directory(owned_directory));
    ownership::OwnedOutputs outputs;
    assert(ownership::record(owned_directory, outputs, error));

    const auto owned_file = owned_directory / "owned";
    const int descriptor = ownership::open_exclusive(owned_file, error);
    assert(descriptor >= 0);
    assert(ownership::record_fd(owned_file, descriptor, outputs, error));
    assert(::write(descriptor, "owned", 5) == 5);
    assert(::close(descriptor) == 0);

    const auto competing_file = owned_directory / "competing";
    write_text(competing_file, "competing writer");
    assert(!ownership::cleanup_owned(outputs, error));
    assert(!error);
    assert(!std::filesystem::exists(owned_file));
    assert(read_text(competing_file) == "competing writer");
    assert(std::filesystem::is_directory(owned_directory));

    const auto staged = root / "staged-previous-destination";
    write_text(staged, "previous destination");
    assert(!ownership::rename_no_replace(staged, destination, error));
    assert(error == std::errc::file_exists);
    assert(read_text(staged) == "previous destination");
    assert(read_text(destination) == "competing writer");

    std::filesystem::remove_all(root, error);
    assert(!error);
    return 0;
}
