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
    // Provider-reported filesystem/mount UUID. Empty means Files has no stable
    // evidence that a later mount at the same URI is the same underlying source.
    std::string mount_uuid;
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

// The returned pointer aliases `places` and remains valid only while that vector
// is unchanged. Nested mounts are resolved to the deepest root containing the location.
[[nodiscard]] const MountedPlace *most_specific_mounted_place_for_location(
    const std::string &location_uri,
    const std::vector<MountedPlace> &places);

// Continuous observation may fall back to a root URI when a provider exposes no
// UUID; reappearance after an unavailable interval deliberately requires a UUID.
[[nodiscard]] bool mounted_places_have_same_source(const MountedPlace &left,
                                                   const MountedPlace &right);
[[nodiscard]] bool mounted_place_reappearance_is_proven(const MountedPlace &previous,
                                                        const MountedPlace &current);

} // namespace infiltrator::files
