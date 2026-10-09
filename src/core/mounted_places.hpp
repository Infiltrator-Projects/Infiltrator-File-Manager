// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <gio/gio.h>

#include <functional>
#include <string>
#include <vector>

namespace infiltrator::files {

struct MountedPlace {
    std::string name;
    std::string uri;
    std::string root_uri;
    bool removable{false};
};

class MountedPlacesMonitor final {
public:
    using ChangedCallback = std::function<void()>;

    explicit MountedPlacesMonitor(ChangedCallback changed);
    ~MountedPlacesMonitor();

    MountedPlacesMonitor(const MountedPlacesMonitor &) = delete;
    MountedPlacesMonitor &operator=(const MountedPlacesMonitor &) = delete;

    [[nodiscard]] std::vector<MountedPlace> snapshot() const;

private:
    static void on_mount_changed(GVolumeMonitor *monitor, GMount *mount, gpointer user_data);
    void notify_changed() const;

    GVolumeMonitor *monitor_{nullptr};
    gulong mount_added_handler_{0};
    gulong mount_removed_handler_{0};
    gulong mount_changed_handler_{0};
    ChangedCallback changed_;
};

[[nodiscard]] bool location_is_within_mounted_places(
    const std::string &location_uri,
    const std::vector<MountedPlace> &places);

[[nodiscard]] bool mounted_location_disappeared(
    const std::string &location_uri,
    const std::vector<MountedPlace> &previous_places,
    const std::vector<MountedPlace> &current_places);

} // namespace infiltrator::files
