#pragma once

#include <cstdint>

namespace bi {

// semantic version packed in a u32
struct Version final {
    uint32_t major : 10;
    uint32_t minor : 10;
    uint32_t patch : 12;

    Version(uint32_t major, uint32_t minor, uint32_t patch) : major(major), minor(minor), patch(patch) {}
    Version(uint32_t version) : major(version >> 22), minor((version >> 12) & 0x3ffu), patch(version & 0xfffu) {}
};
static_assert(sizeof(Version) == sizeof(uint32_t), "bad Version size");

}
