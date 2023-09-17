#include "ui.hpp"

#include "ui_empty.hpp"

namespace bi {

auto create_empty_ui() -> Dyn<IEngineUI>::Box {
    return EmptyEngineUI{};
}

}
