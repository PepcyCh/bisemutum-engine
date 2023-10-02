#include "ui.hpp"

#include "ui_empty.hpp"
#include "../editor/ui.hpp"

namespace bi {

auto create_empty_ui() -> Dyn<IEngineUI>::Box {
    return make_poly<IEngineUI, EmptyEngineUI>();
}

auto create_editor_ui() -> Dyn<IEngineUI>::Box {
    return make_poly<IEngineUI, EditorEngineUI>();
}

}
