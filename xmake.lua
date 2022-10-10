add_rules("mode.debug", "mode.release")

set_languages("cxx20")

option("build_example")
    set_description("If build example executables")
option_end()

includes("modules/xmake.lua")
