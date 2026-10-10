// SPDX-License-Identifier: GPL-3.0-or-later
#include "detail_metadata.hpp"

#include <infiltratr/format.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace infiltrator::files {

namespace {

const char *detail_name(GFileInfo *info)
{
    if (info == nullptr) {
        return "";
    }
    if (const char *display_name = g_file_info_get_display_name(info);
        display_name != nullptr && display_name[0] != '\0') {
        return display_name;
    }
    if (const char *name = g_file_info_get_name(info);
        name != nullptr && name[0] != '\0') {
        return name;
    }
    return "";
}

int compare_text(const char *left, const char *right)
{
    const int result = g_utf8_collate(left != nullptr ? left : "",
                                      right != nullptr ? right : "");
    return result < 0 ? -1 : (result > 0 ? 1 : 0);
}

int name_tiebreak(GFileInfo *left, GFileInfo *right)
{
    return compare_text(detail_name(left), detail_name(right));
}

bool detail_size_value(GFileInfo *info, guint64 *value)
{
    if (info == nullptr || value == nullptr ||
        g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
        return false;
    }
    const goffset size = g_file_info_get_size(info);
    if (size < 0) {
        return false;
    }
    *value = static_cast<guint64>(size);
    return true;
}

bool detail_modified_value(GFileInfo *info, guint64 *value)
{
    if (info == nullptr || value == nullptr ||
        !g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
        return false;
    }
    *value = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    return true;
}

int compare_optional_u64(const bool left_present,
                         const guint64 left,
                         const bool right_present,
                         const guint64 right)
{
    if (left_present != right_present) {
        return left_present ? 1 : -1;
    }
    if (!left_present) {
        return 0;
    }
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}

std::string locale_time_text(GDateTime *local)
{
    char *formatted = g_date_time_format(local, "%X");
    std::string result = formatted != nullptr && formatted[0] != '\0'
        ? formatted : "—";
    g_free(formatted);
    return result;
}

std::string policy_time_text(GDateTime *local, const guint64 seconds)
{
    InfiltratrTemporalPolicyV3 policy{};
    bool found = false;
    const InfiltratrIoResult loaded =
        infiltratr_temporal_posix_policy_load(&policy, &found);

    // "standard" deliberately belongs to the platform locale. An explicit
    // System Settings mode (including standard-12/standard-24) is rendered by
    // Common so Files cannot silently substitute its own 24-hour convention.
    if (loaded != INFILTRATR_IO_OK || !found ||
        std::strcmp(policy.clock_mode, "standard") == 0) {
        return locale_time_text(local);
    }

    constexpr guint64 kMicrosecondsPerSecond = 1000000U;
    if (seconds > static_cast<guint64>(std::numeric_limits<gint64>::max()) /
                      kMicrosecondsPerSecond) {
        return locale_time_text(local);
    }

    const gint64 offset_microseconds = g_date_time_get_utc_offset(local);
    const gint64 offset_seconds = offset_microseconds / G_TIME_SPAN_SECOND;
    if (offset_seconds < std::numeric_limits<std::int32_t>::min() ||
        offset_seconds > std::numeric_limits<std::int32_t>::max()) {
        return locale_time_text(local);
    }

    char buffer[128]{};
    std::size_t length = 0U;
    const bool formatted = infiltratr_temporal_format_clock_mode(
        policy.clock_mode,
        static_cast<gint64>(seconds * kMicrosecondsPerSecond),
        static_cast<std::int32_t>(offset_seconds),
        policy.show_seconds,
        false,
        policy.location_configured,
        policy.latitude,
        policy.longitude,
        buffer,
        sizeof(buffer),
        &length);
    if (!formatted || length == 0U || buffer[0] == '\0') {
        return locale_time_text(local);
    }
    return buffer;
}

} // namespace

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
    guint64 size = 0U;
    if (!detail_size_value(info, &size)) {
        return "—";
    }

    char buffer[64]{};
    const char *formatted = infiltratr_format_disk_capacity(
        static_cast<std::uint64_t>(size), buffer, sizeof(buffer));
    return formatted != nullptr && formatted[0] != '\0' ? formatted : "—";
}

std::string detail_modified_text(GFileInfo *info)
{
    guint64 seconds = 0U;
    if (!detail_modified_value(info, &seconds) ||
        seconds > static_cast<guint64>(std::numeric_limits<gint64>::max())) {
        return "—";
    }

    GDateTime *local = g_date_time_new_from_unix_local(static_cast<gint64>(seconds));
    if (local == nullptr) {
        return "—";
    }

    // Date order remains the operating-system locale's short date. The clock
    // component follows the shared temporal policy owned by System Settings.
    char *date = g_date_time_format(local, "%x");
    const std::string time = policy_time_text(local, seconds);
    std::string result = "—";
    if (date != nullptr && date[0] != '\0' && time != "—") {
        result = std::string(date) + " " + time;
    }
    g_free(date);
    g_date_time_unref(local);
    return result;
}

int detail_compare_name(GFileInfo *left, GFileInfo *right)
{
    return name_tiebreak(left, right);
}

int detail_compare_type(GFileInfo *left, GFileInfo *right)
{
    const std::string left_type = detail_type_text(left);
    const std::string right_type = detail_type_text(right);
    const int by_type = compare_text(left_type.c_str(), right_type.c_str());
    return by_type != 0 ? by_type : name_tiebreak(left, right);
}

int detail_compare_size(GFileInfo *left, GFileInfo *right)
{
    guint64 left_size = 0U;
    guint64 right_size = 0U;
    const bool left_present = detail_size_value(left, &left_size);
    const bool right_present = detail_size_value(right, &right_size);
    const int by_size = compare_optional_u64(left_present, left_size,
                                             right_present, right_size);
    return by_size != 0 ? by_size : name_tiebreak(left, right);
}

int detail_compare_modified(GFileInfo *left, GFileInfo *right)
{
    guint64 left_modified = 0U;
    guint64 right_modified = 0U;
    const bool left_present = detail_modified_value(left, &left_modified);
    const bool right_present = detail_modified_value(right, &right_modified);
    const int by_modified = compare_optional_u64(left_present, left_modified,
                                                 right_present, right_modified);
    return by_modified != 0 ? by_modified : name_tiebreak(left, right);
}

} // namespace infiltrator::files
