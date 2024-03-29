#pragma once

#include <bisemutum/graphics/displayer.hpp>

namespace bi {

struct EmptyEngineUI final {
    auto displayer() -> Dyn<gfx::IDisplayer>::Ref { return displayer_; }

    auto execute() -> void;

private:
    gfx::BlitDisplayer displayer_;
};

}
