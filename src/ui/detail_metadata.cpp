// SPDX-License-Identifier: GPL-3.0-or-later
#include "detail_metadata.hpp"

#include <infiltratr/format.h>

#include <cstdint>
#include <limits>

namespace infiltrator::files {

std::string detail_type_text(GFileInfo *info)
{
    if (info == nullptr) {
        return {};
    }

    // GtkDirectoryList follows valid links when deriving standard::type. Preserve
    // the link identity explicitly when GIO reports standard::is-symlink.
    if (g_file_info_get_is_symlink(info)) {
        return "Symbolic Link";
    }

    switch (g_file_info_get_file_type(info)) {
    case G_FILE_TYPE_DIRECTORY:
        return "Folder";
    case G_FILE_TYPE_SYMBOLIC_LINK:
        return "Symbolic Link";
    case G_FILE_TYPE_SHORTCUT:
        return "Shortcut";
    case G_FILE_TYPE_MOUNTABLE:
        return "Mountable";
    case G_FILE_TYPE_SPECIAL:
        return "Special File";
    case G_FILE_TYPE_UNKNOWN:
        return "Unknown";
    case G_FILE_TYPE_REGULAR:
        break;
    }

    if (const char *content_type = g_file_info_get_content_type(info);
        content_type != nullptr && content_type[0] != '\0') {
        char *description = g_content_type_get_description(content_type);
        if (description != nullptr && description[0] != '\0') {
            std::string result(description);
            g_free(description);
            return result;
        }
        g_free(description);
    }
    return "File";
}

std::string detail_size_text(GFileInfo *info)
{
    if (info == nullptr || g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
        return "—";
    }

    const goffset size = g_file_info_get_size(info);
    if (size < 0) {
        return "—";
    }

    char buffer[64]{};
    const char *formatted = infiltratr_format_disk_capacity(
        static_cast<std::uint64_t>(size), buffer, sizeof(buffer));
    return formatted != nullptr && formatted[0] != '\0' ? formatted : "—";
}

std::string detail_modified_text(GFileInfo *info)
{
    if (info == nullptr ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
        return "—";
    }

    const guint64 seconds =
        g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    if (seconds > static_cast<guint64>(std::numeric_limits<gint64>::max())) {
        return "—";
    }

    GDateTime *local = g_date_time_new_from_unix_local(static_cast<gint64>(seconds));
    if (local == nullptr) {
        return "—";
    }

    // The standard clock/calendar presentation belongs to the platform locale.
    // Common 1.19.38 owns shared temporal policy but does not expose a general
    // calendar-date renderer, so Files does not invent a competing date policy.
    char *formatted = g_date_time_format(local, "%x %X");
    std::string result =
        formatted != nullptr && formatted[0] != '\0' ? formatted : "—";
    g_free(formatted);
    g_date_time_unref(local);
    return result;
}

} // namespace infiltrator::files
