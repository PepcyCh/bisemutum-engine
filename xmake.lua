add_rules("mode.debug", "mode.release")

set_languages("cxx20")

add_requires("spdlog")
add_requires("volk", {configs = {header_only = true}})
add_requires("vulkan-memory-allocator", "spirv-cross", "spirv-reflect", "glslang", "glfw", "glm")
if is_plat("windows") then
    add_requires("vcpkg::d3d12-memory-allocator", "directxshadercompiler")
end

if is_mode("debug") then
    add_defines("BI_DEBUG_MODE")
end

target("bismuth-core")
    set_kind("static")
    add_files("src/core/**.cpp")
    add_headerfiles("src/core/**.hpp")
    add_includedirs("src/")
    add_packages("spdlog", {public = true})

target("bismuth-graphics")
    set_kind("static")
    add_files("src/graphics/*.cpp")
    add_headerfiles("src/graphics/*.hpp")
    add_files("src/graphics/backend_vulkan/*.cpp")
    add_headerfiles("src/graphics/backend_vulkan/*.hpp")
    if is_plat("windows") then
        add_files("src/graphics/backend_d3d12/*.cpp")
        add_headerfiles("src/graphics/backend_d3d12/*.hpp")
    end
    add_includedirs("src/")
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

includes("test/xmake.lua")
