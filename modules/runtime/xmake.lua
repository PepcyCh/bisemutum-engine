add_requires("entt", "nlohmann_json")

target("bismuth-runtime")
    set_kind("static")
    add_includedirs("include/", {public = true})

    add_files("src/*.cpp")
    add_headerfiles("include/runtime/*.hpp")
    add_headerfiles("src/*.hpp", {install = false})

    add_deps("bismuth-core", {public = true})
    add_packages("entt", "nlohmann_json", {public = true})
