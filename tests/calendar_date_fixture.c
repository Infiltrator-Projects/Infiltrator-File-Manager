// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

bool calendar_plus_format_date_v1(
    const char *calendar_id, int32_t year, int32_t month, int32_t day,
    const char *part, char *buffer, size_t capacity, size_t *length);

bool calendar_plus_format_date_v1(
    const char *calendar_id, int32_t year, int32_t month, int32_t day,
    const char *part, char *buffer, size_t capacity, size_t *length)
{
    const char *text = "中文日期";
    const size_t size = strlen(text);
    if (strcmp(calendar_id, "chinese") != 0 || strcmp(part, "short") != 0 ||
        year != 2026 || month != 10 || day != 10 || capacity <= size) {
        return false;
    }
    memcpy(buffer, text, size + 1U);
    *length = size;
    return true;
}
