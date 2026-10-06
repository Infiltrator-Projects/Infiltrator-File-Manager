// SPDX-License-Identifier: GPL-3.0-or-later
#include "location.hpp"

using infiltrator::files::Location;
using infiltrator::files::LocationKind;
using infiltrator::files::classify_location_uri;

int main()
{
    if (classify_location_uri("file:///home/user") != LocationKind::Directory) {
        return 1;
    }
    if (classify_location_uri("trash:///") != LocationKind::Trash) {
        return 2;
    }
    if (classify_location_uri("smb://server/share") != LocationKind::Remote) {
        return 3;
    }
    if (classify_location_uri("search:///recent") != LocationKind::Search) {
        return 4;
    }

    const Location first{"file:///tmp/a", "a", LocationKind::Directory};
    const Location same{"file:///tmp/a", "renamed presentation", LocationKind::Directory};
    const Location other{"file:///tmp/b", "b", LocationKind::Directory};
    if (!(first == same) || first == other || first.is_virtual()) {
        return 5;
    }

    const Location trash{"trash:///", "Trash", LocationKind::Trash};
    if (!trash.is_virtual()) {
        return 6;
    }
    return 0;
}
