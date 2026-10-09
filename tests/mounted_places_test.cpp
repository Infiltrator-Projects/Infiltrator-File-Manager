// SPDX-License-Identifier: GPL-3.0-or-later
#include "mounted_places.hpp"

#include <cstddef>
#include <vector>

using infiltrator::files::MountedPlace;
using infiltrator::files::MountedPlacesMonitor;
using infiltrator::files::most_specific_mounted_place_for_location;
using infiltrator::files::mounted_place_reappearance_is_proven;
using infiltrator::files::mounted_places_have_same_source;

int main()
{
    const std::vector<MountedPlace> places{
        {"Archive", "file:///srv/archive", "file:///srv/archive", "archive-uuid", false},
        {"Camera", "file:///media/user/CAMERA/DCIM", "file:///media/user/CAMERA", "camera-uuid", true},
    };

    const MountedPlace *camera = most_specific_mounted_place_for_location(
        "file:///media/user/CAMERA/DCIM/100MEDIA", places);
    if (camera == nullptr || camera->name != "Camera") {
        return 1;
    }
    if (most_specific_mounted_place_for_location("file:///home/user", places) != nullptr) {
        return 2;
    }
    if (most_specific_mounted_place_for_location("", places) != nullptr) {
        return 3;
    }

    const std::vector<MountedPlace> nested_places{
        {"Outer", "file:///media/user/DISK", "file:///media/user/DISK", "outer-uuid", true},
        {"Inner", "file:///media/user/DISK/NESTED", "file:///media/user/DISK/NESTED", "inner-uuid", true},
    };
    const MountedPlace *nested_source = most_specific_mounted_place_for_location(
        "file:///media/user/DISK/NESTED/project/file.txt", nested_places);
    if (nested_source == nullptr || nested_source->name != "Inner") {
        return 4;
    }

    const std::vector<MountedPlace> after_inner_unmount{
        {"Outer", "file:///media/user/DISK", "file:///media/user/DISK", "outer-uuid", true},
    };
    const MountedPlace *fallback_source = most_specific_mounted_place_for_location(
        "file:///media/user/DISK/NESTED/project/file.txt", after_inner_unmount);
    if (fallback_source == nullptr || fallback_source->name != "Outer" ||
        mounted_places_have_same_source(*nested_source, *fallback_source)) {
        return 5;
    }

    const MountedPlace inner_reappeared{
        "Inner", "file:///media/user/DISK/NESTED", "file:///media/user/DISK/NESTED", "inner-uuid", true};
    if (!mounted_place_reappearance_is_proven(*nested_source, inner_reappeared)) {
        return 6;
    }
    const MountedPlace replacement_at_same_root{
        "Replacement", "file:///media/user/DISK/NESTED", "file:///media/user/DISK/NESTED", "other-uuid", true};
    if (mounted_place_reappearance_is_proven(*nested_source, replacement_at_same_root)) {
        return 7;
    }

    const MountedPlace weak_identity_before{
        "Remote", "smb://server/share", "smb://server/share", "", false};
    const MountedPlace weak_identity_after{
        "Remote", "smb://server/share", "smb://server/share", "", false};
    if (!mounted_places_have_same_source(weak_identity_before, weak_identity_after)) {
        return 8;
    }
    if (mounted_place_reappearance_is_proven(weak_identity_before, weak_identity_after)) {
        return 9;
    }

    // Repeated construction/destruction exercises the signal disconnect-before-unref lifetime path.
    for (std::size_t iteration = 0U; iteration < 32U; ++iteration) {
        MountedPlacesMonitor monitor([]() {});
        const std::vector<MountedPlace> snapshot = monitor.snapshot();

        for (std::size_t index = 0U; index < snapshot.size(); ++index) {
            const MountedPlace &place = snapshot[index];
            if (place.uri.empty() || place.root_uri.empty() || place.root_uri == "file:///") {
                return 10;
            }
            if (index > 0U) {
                const MountedPlace &previous = snapshot[index - 1U];
                if (previous.name > place.name ||
                    (previous.name == place.name && previous.uri > place.uri)) {
                    return 11;
                }
                if (previous.uri == place.uri) {
                    return 12;
                }
            }
        }
    }

    return 0;
}
