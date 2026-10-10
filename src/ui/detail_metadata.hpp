// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gio/gio.h>

#include <string>

namespace infiltrator::files {

[[nodiscard]] std::string detail_type_text(GFileInfo *info);
[[nodiscard]] std::string detail_size_text(GFileInfo *info);
[[nodiscard]] std::string detail_modified_text(GFileInfo *info);

// Raw metadata comparators for GtkColumnView sorting. They deliberately compare
// underlying values rather than the human-facing formatted strings.
[[nodiscard]] int detail_compare_name(GFileInfo *left, GFileInfo *right);
[[nodiscard]] int detail_compare_type(GFileInfo *left, GFileInfo *right);
[[nodiscard]] int detail_compare_size(GFileInfo *left, GFileInfo *right);
[[nodiscard]] int detail_compare_modified(GFileInfo *left, GFileInfo *right);

} // namespace infiltrator::files
