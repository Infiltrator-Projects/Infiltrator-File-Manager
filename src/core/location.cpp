// SPDX-License-Identifier: GPL-3.0-or-later
#include "location.hpp"

#include <string_view>

namespace infiltrator::files {

namespace {

[[nodiscard]] bool starts_with(const std::string &value, const std::string_view prefix) noexcept
{
    return value.size() >= prefix.size() &&
           std::string_view(value.data(), prefix.size()) == prefix;
}

} // namespace

bool Location::is_virtual() const noexcept
{
    return kind == LocationKind::Trash || kind == LocationKind::Search ||
           kind == LocationKind::History;
}

bool Location::operator==(const Location &other) const noexcept
{
    return uri == other.uri && kind == other.kind;
}

LocationKind classify_location_uri(const std::string &uri) noexcept
{
    if (starts_with(uri, "trash:")) {
        return LocationKind::Trash;
    }
    if (starts_with(uri, "search:")) {
        return LocationKind::Search;
    }
    if (starts_with(uri, "history:")) {
        return LocationKind::History;
    }
    if (starts_with(uri, "file:")) {
        return LocationKind::Directory;
    }
    if (uri.find("://") != std::string::npos) {
        return LocationKind::Remote;
    }
    return LocationKind::Other;
}

} // namespace infiltrator::files
