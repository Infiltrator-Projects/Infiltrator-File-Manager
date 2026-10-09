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
        GFile *root = g_mount_get_root(mount);
        char *uri = root != nullptr ? g_file_get_uri(root) : nullptr;
        char *name = g_mount_get_name(mount);

        if (uri != nullptr && uri[0] != '\0' && std::string_view(uri) != "file:///") {
            bool removable = g_mount_can_eject(mount);
            if (GVolume *volume = g_mount_get_volume(mount); volume != nullptr) {
                if (GDrive *drive = g_volume_get_drive(volume); drive != nullptr) {
                    removable = removable || g_drive_is_removable(drive);
                    g_object_unref(drive);
                }
                g_object_unref(volume);
            }
            places.push_back(MountedPlace{name != nullptr ? name : uri, uri, removable});
        }

        g_free(name);
        g_free(uri);
        if (root != nullptr) {
            g_object_unref(root);
        }
    }
    g_list_free_full(mounts, g_object_unref);

    std::sort(places.begin(), places.end(), [](const MountedPlace &left, const MountedPlace &right) {
        if (left.name != right.name) {
            return left.name < right.name;
        }
        return left.uri < right.uri;
    });
    places.erase(std::unique(places.begin(), places.end(),
                             [](const MountedPlace &left, const MountedPlace &right) {
                                 return left.uri == right.uri;
                             }),
                 places.end());
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

} // namespace infiltrator::files
