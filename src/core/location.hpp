// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace infiltrator::files {

enum class LocationKind {
    Directory,
    Trash,
    Remote,
    Search,
    History,
    Other
};

struct Location {
    std::string uri;
    std::string display_name;
    LocationKind kind{LocationKind::Other};

    [[nodiscard]] bool is_virtual() const noexcept;
    [[nodiscard]] bool operator==(const Location &other) const noexcept;
};

[[nodiscard]] LocationKind classify_location_uri(const std::string &uri) noexcept;

} // namespace infiltrator::files
