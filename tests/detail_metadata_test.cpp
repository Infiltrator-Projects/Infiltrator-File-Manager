// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/ui/detail_metadata.hpp"

#include <string>

using infiltrator::files::detail_modified_text;
using infiltrator::files::detail_size_text;
using infiltrator::files::detail_type_text;

int main()
{
    GFileInfo *directory = g_file_info_new();
    g_file_info_set_file_type(directory, G_FILE_TYPE_DIRECTORY);
    const bool directory_ok = detail_type_text(directory) == "Folder" &&
                              detail_size_text(directory) == "—" &&
                              detail_modified_text(directory) == "—";
    g_object_unref(directory);
    if (!directory_ok) {
        return 1;
    }

    GFileInfo *file = g_file_info_new();
    g_file_info_set_file_type(file, G_FILE_TYPE_REGULAR);
    if (detail_type_text(file) != "File") {
        g_object_unref(file);
        return 2;
    }

    g_file_info_set_size(file, 1536);
    if (detail_size_text(file) != "1.5 KB") {
        g_object_unref(file);
        return 3;
    }

    g_file_info_set_attribute_uint64(file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0U);
    const std::string modified = detail_modified_text(file);
    if (modified.empty() || modified == "—") {
        g_object_unref(file);
        return 4;
    }

    // A followed symlink may report the target's regular/directory type. The
    // explicit is-symlink attribute must win in the analytical Type column.
    g_file_info_set_is_symlink(file, TRUE);
    if (detail_type_text(file) != "Symbolic Link") {
        g_object_unref(file);
        return 5;
    }

    g_object_unref(file);
    return 0;
}
