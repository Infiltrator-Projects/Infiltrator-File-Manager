// SPDX-License-Identifier: GPL-3.0-or-later
#include "operation_journal.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

using infiltrator::files::OperationJournal;
using infiltrator::files::OperationPhase;
using infiltrator::files::OperationResult;
using infiltrator::files::OperationStatus;

int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() /
                          ("infiltrator-files-journal-test-" + std::to_string(::getpid()));
    const fs::path journal_path = root / "state" / "operations.log";
    std::error_code cleanup_error;
    fs::remove_all(root, cleanup_error);

    OperationJournal journal(journal_path);
    const std::string id = journal.begin("copy", "/tmp/source\tname", "/tmp/destination");
    if (id.empty() || !fs::exists(journal_path)) {
        fs::remove_all(root, cleanup_error);
        return 1;
    }

    const OperationResult result{
        OperationStatus::Success,
        OperationPhase::Complete,
        "/tmp/destination/source",
        "Copied successfully.",
        true,
    };
    if (!journal.finish(id, "copy", result)) {
        fs::remove_all(root, cleanup_error);
        return 2;
    }

    std::ifstream stream(journal_path, std::ios::binary);
    std::ostringstream content;
    content << stream.rdbuf();
    const std::string text = content.str();
    if (text.find("\t" + id + "\tSTART\tcopy\t/tmp/source\\tname\t/tmp/destination") == std::string::npos ||
        text.find("\t" + id + "\tEND\tcopy\tsuccess\tchanged\t/tmp/destination/source\tCopied successfully.") == std::string::npos) {
        fs::remove_all(root, cleanup_error);
        return 3;
    }

    const auto permissions = fs::status(journal_path).permissions();
    const auto group_or_other = fs::perms::group_read | fs::perms::group_write |
                                fs::perms::group_exec | fs::perms::others_read |
                                fs::perms::others_write | fs::perms::others_exec;
    if ((permissions & group_or_other) != fs::perms::none) {
        fs::remove_all(root, cleanup_error);
        return 4;
    }

    fs::remove_all(root, cleanup_error);
    return 0;
}
