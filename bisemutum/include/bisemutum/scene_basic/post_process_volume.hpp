#pragma once

#include "../utils/srefl.hpp"

namespace bi {

struct PostProcessVolumeComponent final {
    static constexpr std::string_view component_type_name = "PostProcessVolumeComponent";

    bool global{true};
    float priority{0.0f};

    bool bloom{false};
    float bloom_threshold{1.5f};
    float bloom_threshold_softness{0.5f};
};
BI_SREFL(
    type(PostProcessVolumeComponent),
    field(global),
    field(priority, RangeF{}),
    field(bloom),
    field(bloom_threshold, RangeF{}),
    field(bloom_threshold_softness, RangeF{0.0f, 1.0f}),
)

}
