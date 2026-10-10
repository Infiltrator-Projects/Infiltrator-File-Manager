// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gio/gio.h>

#include <string>

namespace infiltrator::files {

[[nodiscard]] std::string detail_type_text(GFileInfo *info);
[[nodiscard]] std::string detail_size_text(GFileInfo *info);
[[nodiscard]] std::string detail_modified_text(GFileInfo *info);

} // namespace infiltrator::files
