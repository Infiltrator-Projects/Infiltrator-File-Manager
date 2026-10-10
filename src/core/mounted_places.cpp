// SPDX-License-Identifier: GPL-3.0-or-later
#include "mounted_places.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace infiltrator::files {

MountedPlacesMonitor::MountedPlacesMonitor(ChangedCallback changed)
    : monitor_(g_volume_monitor_get()), changed_(std::move(changed))
{
    if (monitor_ == nullptr) {
        return;
    }

    mount_added_handler_ = g_signal_connect(monitor_, "mount-added",
                                            G_CALLBACK(on_mount_changed), this);
    mount_removed_handler_ = g_signal_connect(monitor_, "mount-removed",
                                              G_CALLBACK(on_mount_changed), this);
    mount_changed_handler_ = g_signal_connect(monitor_, "mount-changed",
                                              G_CALLBACK(on_mount_changed), this);
}

MountedPlacesMonitor::~MountedPlacesMonitor()
{
    if (monitor_ == nullptr) {
        return;
    }

    // Disconnect before releasing the monitor: callbacks capture this object's lifetime.
    if (mount_added_handler_ != 0U) {
        g_signal_handler_disconnect(monitor_, mount_added_handler_);
    }
    if (mount_removed_handler_ != 0U) {
        g_signal_handler_disconnect(monitor_, mount_removed_handler_);
    }
    if (mount_changed_handler_ != 0U) {
        g_signal_handler_disconnect(monitor_, mount_changed_handler_);
    }
    g_object_unref(monitor_);
}

std::vector<MountedPlace> MountedPlacesMonitor::snapshot() const
{
    std::vector<MountedPlace> places;
    if (monitor_ == nullptr) {
        return places;
    }

    GList *mounts = g_volume_monitor_get_mounts(monitor_);
    for (GList *node = mounts; node != nullptr; node = node->next) {
        auto *mount = G_MOUNT(node->data);
        if (g_mount_is_shadowed(mount)) {
            continue;
        }

        GFile *root = g_mount_get_root(mount);
        GFile *default_location = g_mount_get_default_location(mount);
        GFile *target = default_location != nullptr ? default_location : root;
        char *uri = target != nullptr ? g_file_get_uri(target) : nullptr;
        char *root_uri = root != nullptr ? g_file_get_uri(root) : nullptr;
        char *mount_uuid = g_mount_get_uuid(mount);
        char *name = g_mount_get_name(mount);

        const bool meaningful_target = uri != nullptr && uri[0] != '\0';
        const bool is_filesystem_root = root_uri != nullptr && std::string_view(root_uri) == "file:///";
        if (meaningful_target && !is_filesystem_root) {
            bool removable = g_mount_can_eject(mount);
            if (GVolume *volume = g_mount_get_volume(mount); volume != nullptr) {
                if (GDrive *drive = g_volume_get_drive(volume); drive != nullptr) {
                    removable = removable || g_drive_is_removable(drive);
                    g_object_unref(drive);
                }
                g_object_unref(volume);
            }
            places.push_back(MountedPlace{
                name != nullptr ? name : uri,
                uri,
                root_uri != nullptr ? root_uri : uri,
                mount_uuid != nullptr ? mount_uuid : "",
                removable});
        }

        g_free(name);
        g_free(mount_uuid);
        g_free(root_uri);
        g_free(uri);
        if (default_location != nullptr) {
            g_object_unref(default_location);
        }
        if (root != nullptr) {
            g_object_unref(root);
        }
    }
    g_list_free_full(mounts, g_object_unref);

    return normalize_mounted_places(std::move(places));
}

std::vector<MountedPlace> normalize_mounted_places(std::vector<MountedPlace> places)
{
    // Keep supplying roots independent of their navigation targets: two mounts
    // can expose the same default target without being the same source.
    std::sort(places.begin(), places.end(), [](const MountedPlace &left, const MountedPlace &right) {
        if (left.uri != right.uri) {
            return left.uri < right.uri;
        }
        if (left.root_uri != right.root_uri) {
            return left.root_uri < right.root_uri;
        }
        if (left.mount_uuid.empty() != right.mount_uuid.empty()) {
            return !left.mount_uuid.empty();
        }
        if (left.mount_uuid != right.mount_uuid) {
            return left.mount_uuid < right.mount_uuid;
        }
        if (left.name != right.name) {
            return left.name < right.name;
        }
        return left.removable < right.removable;
    });
    places.erase(std::unique(places.begin(), places.end(),
                             [](const MountedPlace &left, const MountedPlace &right) {
                                 return mounted_places_have_same_target(left, right);
                             }),
                 places.end());
    std::sort(places.begin(), places.end(), [](const MountedPlace &left, const MountedPlace &right) {
        if (left.name != right.name) {
            return left.name < right.name;
        }
        if (left.uri != right.uri) {
            return left.uri < right.uri;
        }
        if (left.root_uri != right.root_uri) {
            return left.root_uri < right.root_uri;
        }
        return left.mount_uuid < right.mount_uuid;
    });
    return places;
}

void MountedPlacesMonitor::on_mount_changed(GVolumeMonitor *monitor, GMount *mount, gpointer user_data)
{
    (void)monitor;
    (void)mount;
    static_cast<MountedPlacesMonitor *>(user_data)->notify_changed();
}

void MountedPlacesMonitor::notify_changed() const
{
    if (changed_) {
        changed_();
    }
}

const MountedPlace *most_specific_mounted_place_for_location(
    const std::string &location_uri,
    const std::vector<MountedPlace> &places)
{
    if (location_uri.empty()) {
        return nullptr;
    }

    GFile *location = g_file_new_for_uri(location_uri.c_str());
    if (location == nullptr) {
        return nullptr;
    }

    const MountedPlace *best = nullptr;
    GFile *best_root = nullptr;
    for (const MountedPlace &place : places) {
        if (place.root_uri.empty()) {
            continue;
        }

        GFile *root = g_file_new_for_uri(place.root_uri.c_str());
        if (root == nullptr) {
            continue;
        }

        const bool contains_location =
            g_file_equal(location, root) || g_file_has_prefix(location, root);
        bool more_specific = best == nullptr;
        if (contains_location && best_root != nullptr && !g_file_equal(root, best_root)) {
            more_specific = g_file_has_prefix(root, best_root);
        }

        if (contains_location && more_specific) {
            if (best_root != nullptr) {
                g_object_unref(best_root);
            }
            best = &place;
            best_root = root;
        } else {
            g_object_unref(root);
        }
    }

    if (best_root != nullptr) {
        g_object_unref(best_root);
    }
    g_object_unref(location);
    return best;
}

bool mounted_places_have_same_source(const MountedPlace &left, const MountedPlace &right)
{
    if (left.root_uri.empty() || left.root_uri != right.root_uri) {
        return false;
    }
    if (!left.mount_uuid.empty() || !right.mount_uuid.empty()) {
        return !left.mount_uuid.empty() && left.mount_uuid == right.mount_uuid;
    }
    return true;
}

bool mounted_place_reappearance_is_proven(const MountedPlace &previous,
                                          const MountedPlace &current)
{
    // A path can be reused by a different device/share after an unavailable interval.
    // Automatic restoration therefore requires both the same mount root and provider UUID.
    return !previous.mount_uuid.empty() && previous.root_uri == current.root_uri &&
           previous.mount_uuid == current.mount_uuid;
}

bool mounted_places_have_same_target(const MountedPlace &left, const MountedPlace &right)
{
    return left.uri == right.uri && mounted_places_have_same_source(left, right);
}

} // namespace infiltrator::files
