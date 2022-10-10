add_requires("spdlog")

target("bismuth-core")
    set_kind("static")
    add_files("src/**.cpp")
    add_headerfiles("include/bismuth.hpp", "include/core/**.hpp")
    add_headerfiles("src/core/**.hpp", {install = false})
    add_includedirs("include/", {public = true})
    add_packages("spdlog", {public = true})
