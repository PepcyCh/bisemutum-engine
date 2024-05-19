#pragma once

#include "../utils/srefl.hpp"

namespace bi {

struct DdgiVolumeComponent final {
    static constexpr std::string_view component_type_name = "DdgiVolumeComponent";

    // For volume component
    static constexpr bool global{false};
    float priority{0.0f};
};
BI_SREFL(
    type(DdgiVolumeComponent),
    field(priority, RangeF{}),
)

}
