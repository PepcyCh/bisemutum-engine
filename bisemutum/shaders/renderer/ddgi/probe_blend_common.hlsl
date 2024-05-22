#pragma once

uint2 get_probe_border_coord(uint2 probe_coord, uint size) {
    if (probe_coord.x == 1) {
        if (probe_coord.y == 1) {
            return uint2(size + 1, size + 1);
        } else if (probe_coord.y == size) {
            return uint2(size + 1, 0);
        } else {
            return uint2(0, size + 1 - probe_coord.y);
        }
    } else if (probe_coord.x == size) {
        if (probe_coord.y == 1) {
            return uint2(0, size + 1);
        } else if (probe_coord.y == size) {
            return uint2(0, 0);
        } else {
            return uint2(size + 1, size + 1 - probe_coord.y);
        }
    } else if (probe_coord.y == 1) {
        return uint2(size + 1 - probe_coord.x, 0);
    } else if (probe_coord.y == size) {
        return uint2(size + 1 - probe_coord.x, size + 1);
    } else {
        return probe_coord;
    }
}
