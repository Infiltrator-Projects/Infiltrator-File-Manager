// SPDX-License-Identifier: GPL-3.0-or-later
#include "mounted_places.hpp"

#include <cstddef>
#include <vector>

using infiltrator::files::MountedPlace;
using infiltrator::files::MountedPlacesMonitor;
using infiltrator::files::location_is_within_mounted_places;

int main()
{
    const std::vector<MountedPlace> places{
        {"Archive", "file:///srv/archive", "file:///srv/archive", false},
        {"Camera", "file:///media/user/CAMERA/DCIM", "file:///media/user/CAMERA", true},
    };

    if (!location_is_within_mounted_places("file:///media/user/CAMERA", places)) {
        return 1;
    }
    if (!location_is_within_mounted_places("file:///media/user/CAMERA/DCIM/100MEDIA", places)) {
        return 2;
    }
    if (location_is_within_mounted_places("file:///home/user", places)) {
        return 3;
    }
    if (location_is_within_mounted_places("", places)) {
        return 4;
    }

    const std::vector<MountedPlace> after_camera_disconnect{
        {"Archive", "file:///srv/archive", "file:///srv/archive", false},
    };
    const bool camera_was_mounted =
        location_is_within_mounted_places("file:///media/user/CAMERA/DCIM/100MEDIA", places);
    const bool camera_is_still_mounted =
        location_is_within_mounted_places("file:///media/user/CAMERA/DCIM/100MEDIA",
                                          after_camera_disconnect);
    if (!camera_was_mounted || camera_is_still_mounted) {
        return 5;
    }
    if (!location_is_within_mounted_places("file:///srv/archive/project",
                                           after_camera_disconnect)) {
        return 6;
    }

    // Repeated construction/destruction exercises the signal disconnect-before-unref lifetime path.
    for (std::size_t iteration = 0U; iteration < 32U; ++iteration) {
        MountedPlacesMonitor monitor([]() {});
        const std::vector<MountedPlace> snapshot = monitor.snapshot();

        for (std::size_t index = 0U; index < snapshot.size(); ++index) {
            const MountedPlace &place = snapshot[index];
            if (place.uri.empty() || place.root_uri.empty() || place.root_uri == "file:///") {
                return 7;
            }
            if (index > 0U) {
                const MountedPlace &previous = snapshot[index - 1U];
                if (previous.name > place.name ||
                    (previous.name == place.name && previous.uri > place.uri)) {
                    return 8;
                }
                if (previous.uri == place.uri) {
                    return 9;
                }
            }
        }
    }

    return 0;
}
