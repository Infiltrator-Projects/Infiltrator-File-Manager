// SPDX-License-Identifier: GPL-3.0-or-later
#include "detail_metadata.hpp"

#include <infiltratr/format.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>

#include <cstdint>
#include <cstring>
#include <langinfo.h>
#include <limits>
#include <string_view>
#include <vector>

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

std::string expand_locale_time_composites(std::string_view pattern,
                                          std::string_view ampm_pattern)
{
    std::string expanded(pattern);

    // nl_langinfo() formats may use the POSIX composite forms %T and %r.
    // Expand those before removing %S so the locale's actual field order and
    // AM/PM placement remain data rather than an application guess.
    for (unsigned pass = 0; pass < 3U; ++pass) {
        std::string next;
        bool changed = false;
        next.reserve(expanded.size() + 16U);

        for (std::size_t index = 0; index < expanded.size();) {
            if (expanded[index] != '%' || index + 1U >= expanded.size()) {
                next.push_back(expanded[index++]);
                continue;
            }
            if (expanded[index + 1U] == '%') {
                next.append("%%");
                index += 2U;
                continue;
            }

            std::size_t conversion = index + 1U;
            if (expanded[conversion] == 'E' || expanded[conversion] == 'O') {
                ++conversion;
            }
            if (conversion >= expanded.size()) {
                next.append(expanded.substr(index));
                break;
            }

            const char specifier = expanded[conversion];
            const bool has_modifier = conversion != index + 1U;
            if (!has_modifier && specifier == 'T') {
                next.append("%H:%M:%S");
                changed = true;
            } else if (!has_modifier && specifier == 'r' &&
                       !ampm_pattern.empty() && ampm_pattern != "%r") {
                next.append(ampm_pattern);
                changed = true;
            } else {
                next.append(expanded, index, conversion - index + 1U);
            }
            index = conversion + 1U;
        }

        expanded.swap(next);
        if (!changed) {
            break;
        }
    }
    return expanded;
}

struct TimeFormatToken {
    bool directive = false;
    char conversion = '\0';
    std::string text;
};

void append_literal(std::vector<TimeFormatToken> &tokens,
                    std::string_view literal)
{
    if (literal.empty()) {
        return;
    }
    if (!tokens.empty() && !tokens.back().directive) {
        tokens.back().text.append(literal);
        return;
    }
    tokens.push_back({false, '\0', std::string(literal)});
}

std::vector<TimeFormatToken> tokenize_time_format(std::string_view pattern)
{
    std::vector<TimeFormatToken> tokens;
    std::size_t index = 0U;

    while (index < pattern.size()) {
        if (pattern[index] != '%') {
            const std::size_t end = pattern.find('%', index);
            append_literal(tokens, pattern.substr(
                index, end == std::string_view::npos ? pattern.size() - index
                                                     : end - index));
            if (end == std::string_view::npos) {
                break;
            }
            index = end;
            continue;
        }

        if (index + 1U >= pattern.size()) {
            append_literal(tokens, pattern.substr(index));
            break;
        }
        if (pattern[index + 1U] == '%') {
            append_literal(tokens, "%%");
            index += 2U;
            continue;
        }

        std::size_t conversion = index + 1U;
        if (pattern[conversion] == 'E' || pattern[conversion] == 'O') {
            ++conversion;
        }
        if (conversion >= pattern.size()) {
            append_literal(tokens, pattern.substr(index));
            break;
        }

        tokens.push_back({true, pattern[conversion],
                          std::string(pattern.substr(
                              index, conversion - index + 1U))});
        index = conversion + 1U;
    }
    return tokens;
}

void trim_trailing_locale_separators(std::string &literal)
{
    const char *begin = literal.data();
    const char *end = begin + literal.size();

    while (end > begin) {
        const char *previous = g_utf8_find_prev_char(begin, end);
        if (previous == nullptr) {
            break;
        }
        const gunichar character = g_utf8_get_char(previous);
        if (g_unichar_isalnum(character)) {
            break;
        }
        end = previous;
    }
    literal.resize(static_cast<std::size_t>(end - begin));
}

bool begins_with_locale_space(std::string_view literal)
{
    return !literal.empty() &&
           g_unichar_isspace(g_utf8_get_char(literal.data()));
}

bool contains_locale_word(std::string_view literal)
{
    const char *cursor = literal.data();
    const char *end = cursor + literal.size();
    while (cursor < end) {
        const gunichar character = g_utf8_get_char(cursor);
        if (g_unichar_isalnum(character)) {
            return true;
        }
        cursor = g_utf8_next_char(cursor);
    }
    return false;
}

std::string trailing_locale_space(std::string_view literal)
{
    const char *begin = literal.data();
    const char *end = begin + literal.size();
    const char *cursor = end;

    while (cursor > begin) {
        const char *previous = g_utf8_find_prev_char(begin, cursor);
        if (previous == nullptr ||
            !g_unichar_isspace(g_utf8_get_char(previous))) {
            break;
        }
        cursor = previous;
    }
    return std::string(cursor, static_cast<std::size_t>(end - cursor));
}

std::string locale_time_text(GDateTime *local, const bool show_seconds)
{
    if (local == nullptr) {
        return "—";
    }

    if (show_seconds) {
        char *formatted = g_date_time_format(local, "%X");
        std::string result = formatted != nullptr && formatted[0] != '\0'
            ? formatted : "—";
        g_free(formatted);
        return result;
    }

    // Standard time belongs to the operating-system locale. Use the locale's
    // own POSIX time pattern and remove its seconds field instead of replacing
    // the layout with an application-owned 12/24-hour template.
    const char *time_format = nl_langinfo(T_FMT);
    const char *ampm_format = nl_langinfo(T_FMT_AMPM);
    const std::string minute_format = detail_locale_time_format_without_seconds(
        time_format != nullptr ? std::string_view(time_format) : std::string_view(),
        ampm_format != nullptr ? std::string_view(ampm_format) : std::string_view());
    if (minute_format.empty()) {
        return "—";
    }

    char *formatted = g_date_time_format(local, minute_format.c_str());
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

    // "standard" deliberately belongs to the platform locale, but the
    // Common-owned show-seconds setting still applies to that presentation.
    // Explicit modes (including standard-12/standard-24) are rendered by
    // Common so Files cannot silently substitute its own clock convention.
    if (loaded != INFILTRATR_IO_OK || !found) {
        return locale_time_text(local, true);
    }
    if (std::strcmp(policy.clock_mode, "standard") == 0) {
        return locale_time_text(local, policy.show_seconds);
    }

    constexpr guint64 kMicrosecondsPerSecond = 1000000U;
    if (seconds > static_cast<guint64>(std::numeric_limits<gint64>::max()) /
                      kMicrosecondsPerSecond) {
        return locale_time_text(local, policy.show_seconds);
    }

    const gint64 offset_microseconds = g_date_time_get_utc_offset(local);
    const gint64 offset_seconds = offset_microseconds / G_TIME_SPAN_SECOND;
    if (offset_seconds < std::numeric_limits<std::int32_t>::min() ||
        offset_seconds > std::numeric_limits<std::int32_t>::max()) {
        return locale_time_text(local, policy.show_seconds);
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
        return locale_time_text(local, policy.show_seconds);
    }
    return buffer;
}

} // namespace

std::string detail_locale_time_format_without_seconds(
    std::string_view time_format,
    std::string_view ampm_format)
{
    if (time_format.empty()) {
        return {};
    }

    const std::string expanded =
        expand_locale_time_composites(time_format, ampm_format);
    std::vector<TimeFormatToken> tokens = tokenize_time_format(expanded);

    for (std::size_t index = 0U; index < tokens.size(); ++index) {
        if (!tokens[index].directive || tokens[index].conversion != 'S') {
            continue;
        }

        // A punctuation/space separator immediately before seconds belongs to
        // that field. A localized minute unit (for example 分) is alphanumeric
        // and therefore remains intact.
        if (index > 0U && !tokens[index - 1U].directive) {
            trim_trailing_locale_separators(tokens[index - 1U].text);
        }

        // Locale patterns may attach a localized unit directly to seconds,
        // e.g. %H時%M分%S秒. Remove that attached unit too, while preserving
        // whitespace that separates a following directive such as %p. A word
        // after whitespace (e.g. a whole-time suffix) is not treated as an
        // attached seconds unit.
        if (index + 1U < tokens.size() && !tokens[index + 1U].directive &&
            !begins_with_locale_space(tokens[index + 1U].text) &&
            contains_locale_word(tokens[index + 1U].text)) {
            if (index + 2U < tokens.size()) {
                tokens[index + 1U].text =
                    trailing_locale_space(tokens[index + 1U].text);
            } else {
                tokens[index + 1U].text.clear();
            }
        }
        tokens[index].text.clear();
    }

    std::string result;
    for (const TimeFormatToken &token : tokens) {
        result.append(token.text);
    }
    return result;
}

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
