#include "ui.hpp"

#include "ui_empty.hpp"

namespace bi {

auto create_empty_ui() -> Dyn<IEngineUI>::Box {
    // Dyn<IEngineUI>::Box ui;
    // ui.emplace<EmptyEngineUI>();
    // return ui;
    return make_poly<IEngineUI, EmptyEngineUI>();
}

}
