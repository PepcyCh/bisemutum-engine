#pragma once

#include <bisemutum/graphics/displayer.hpp>

#include "displayer.hpp"

namespace bi {

struct EditorEngineUI final {
    auto displayer() -> Dyn<gfx::IDisplayer>::Ref { return displayer_; }

    auto execute() -> void {}

private:
    EditorDisplayer displayer_;
};

}
