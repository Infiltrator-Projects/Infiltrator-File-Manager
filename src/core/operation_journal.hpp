// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "operation_engine.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace infiltrator::files {

class OperationJournal final {
public:
    explicit OperationJournal(std::filesystem::path path = default_path());

    [[nodiscard]] std::string begin(std::string_view kind,
                                    std::string_view source,
                                    std::string_view destination) const;
    [[nodiscard]] bool finish(std::string_view operation_id,
                              std::string_view kind,
                              const OperationResult &result) const;

    [[nodiscard]] const std::filesystem::path &path() const noexcept { return path_; }
    [[nodiscard]] static std::filesystem::path default_path();

private:
    [[nodiscard]] bool append_line(const std::string &line) const;
    [[nodiscard]] static std::string escape(std::string_view value);

    std::filesystem::path path_;
};

} // namespace infiltrator::files
