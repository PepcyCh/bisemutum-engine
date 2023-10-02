#pragma once

#include <bisemutum/prelude/poly.hpp>
#include <bisemutum/graphics/displayer.hpp>

namespace bi {

BI_TRAIT_BEGIN(IEngineUI, move)
    BI_TRAIT_METHOD(displayer, (&self) requires (self.displayer()) -> Dyn<gfx::IDisplayer>::Ref)
    BI_TRAIT_METHOD(execute, (&self) requires (self.execute()) -> void)
BI_TRAIT_END(IEngineUI)

auto create_empty_ui() -> Dyn<IEngineUI>::Box;

auto create_editor_ui() -> Dyn<IEngineUI>::Box;

}
