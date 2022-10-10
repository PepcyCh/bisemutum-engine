add_requires("volk", {configs = {header_only = true}})
add_requires("vulkan-memory-allocator", "spirv-cross", "spirv-reflect", "glslang", "glfw")
if is_plat("windows") then
    add_requires("vcpkg::d3d12-memory-allocator", "directxshadercompiler")
end

target("bismuth-graphics")
    set_kind("static")
    add_includedirs("include/", {public = true})

    add_files("src/graphics/*.cpp")
    add_headerfiles("include/graphics/*.hpp")
    add_headerfiles("src/graphics/*.hpp", {install = false})
    add_files("src/graphics/backend_vulkan/*.cpp")
    add_headerfiles("src/graphics/backend_vulkan/*.hpp", {install = false})
    if is_plat("windows") then
        add_files("src/graphics/backend_d3d12/*.cpp")
        add_headerfiles("src/graphics/backend_d3d12/*.hpp", {install = false})
    end
    set_configvar("MODULE_DIR", "$(curdir)/modules/graphics")
    add_configfiles("include/(graphics/mod.hpp.in)")
    add_includedirs("$(buildir)", {public = true})
    
    add_files("src/render_graph/*.cpp")
    add_headerfiles("include/render_graph/*.hpp")
    add_headerfiles("src/render_graph/*.hpp", {install = false})

    add_files("src/shader_manager/*.cpp")
    add_headerfiles("include/shader_manager/*.hpp")
    add_headerfiles("src/shader_manager/*.hpp", {install = false})

    add_deps("bismuth-core", {public = true})
    add_packages("volk", "vulkan-memory-allocator", "spirv-cross", "spirv-reflect", "glslang", "glfw")
    if is_plat("windows") then
        add_defines("VK_USE_PLATFORM_WIN32_KHR")
        add_packages("vcpkg::d3d12-memory-allocator", "directxshadercompiler")
        add_syslinks("d3d12", "dxgi", "dxguid")
        add_includedirs("thirdparty/WinPixEventRuntime/include")
        add_linkdirs("thirdparty/WinPixEventRuntime/bin/x64")
        add_links("WinPixEventRuntime")
        after_build(function (target)
            os.cp("thirdparty/WinPixEventRuntime/bin/x64/WinPixEventRuntime.dll", target:targetdir())
        end)
    end
target_end()

if has_config("build_example") then
    add_requires("stb", "glm")

    target("bismuth-graphics-example-triangle")
        set_kind("binary")
        add_files("examples/graphics/triangle.cpp")
        add_deps("bismuth-graphics")
        add_packages("glfw")

    target("bismuth-graphics-example-texture")
        set_kind("binary")
        add_files("examples/graphics/texture.cpp")
        add_deps("bismuth-graphics")
        add_packages("glfw", "stb")

    target("bismuth-graphics-example-compute")
        set_kind("binary")
        add_files("examples/graphics/compute.cpp")
        add_deps("bismuth-graphics")
        add_packages("glfw", "stb")

    target("bismuth-render_graph-example-texture")
        set_kind("binary")
        add_files("examples/render_graph/texture.cpp")
        add_deps("bismuth-graphics")
        add_packages("glfw", "stb")

    target("bismuth-render_graph-example-deferred")
        set_kind("binary")
        add_files("examples/render_graph/deferred.cpp")
        add_deps("bismuth-graphics")
        add_packages("glfw", "glm")
end
