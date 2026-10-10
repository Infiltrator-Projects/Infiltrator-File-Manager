// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/ui/detail_metadata.hpp"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>

#include <cstring>
#include <string>

using infiltrator::files::detail_compare_modified;
using infiltrator::files::detail_compare_name;
using infiltrator::files::detail_compare_size;
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
    g_file_info_set_name(file, "small.txt");
    g_file_info_set_display_name(file, "small.txt");
    if (detail_type_text(file) != "File") {
        g_object_unref(file);
        return 2;
    }

    g_file_info_set_size(file, 1536);
    if (detail_size_text(file) != "1.5 KB") {
        g_object_unref(file);
        return 3;
    }

    // Put the shared temporal policy in an isolated test config before the
    // first timestamp formatting call. System Settings and Files consume this
    // same Common-owned policy document.
    GError *temp_error = nullptr;
    gchar *config_home = g_dir_make_tmp("infiltrator-files-temporal-XXXXXX", &temp_error);
    if (config_home == nullptr) {
        if (temp_error != nullptr) {
            g_error_free(temp_error);
        }
        g_object_unref(file);
        return 4;
    }
    g_setenv("XDG_CONFIG_HOME", config_home, TRUE);

    InfiltratrTemporalPolicyV3 policy{};
    if (!infiltratr_temporal_policy_v3_default(&policy)) {
        g_free(config_home);
        g_object_unref(file);
        return 5;
    }
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "standard-12");
    policy.show_seconds = false;
    if (infiltratr_temporal_posix_policy_save(&policy) != 0) {
        g_free(config_home);
        g_object_unref(file);
        return 6;
    }

    g_file_info_set_attribute_uint64(file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 46800U);
    const std::string modified = detail_modified_text(file);
    if (modified.empty() || modified == "—" ||
        (modified.find("AM") == std::string::npos &&
         modified.find("PM") == std::string::npos)) {
        g_free(config_home);
        g_object_unref(file);
        return 7;
    }

    GFileInfo *larger = g_file_info_new();
    g_file_info_set_file_type(larger, G_FILE_TYPE_REGULAR);
    g_file_info_set_name(larger, "large.txt");
    g_file_info_set_display_name(larger, "large.txt");
    g_file_info_set_size(larger, 4096);
    g_file_info_set_attribute_uint64(larger, G_FILE_ATTRIBUTE_TIME_MODIFIED, 86400U);

    if (detail_compare_size(file, larger) >= 0 ||
        detail_compare_size(larger, file) <= 0 ||
        detail_compare_modified(file, larger) >= 0 ||
        detail_compare_modified(larger, file) <= 0 ||
        detail_compare_name(larger, file) >= 0) {
        g_object_unref(larger);
        g_free(config_home);
        g_object_unref(file);
        return 8;
    }
    g_object_unref(larger);

    // A followed symlink may report the target's regular/directory type. The
    // explicit is-symlink attribute must win in the analytical Type column.
    g_file_info_set_is_symlink(file, TRUE);
    if (detail_type_text(file) != "Symbolic Link") {
        g_free(config_home);
        g_object_unref(file);
        return 9;
    }

    g_free(config_home);
    g_object_unref(file);
    return 0;
}
