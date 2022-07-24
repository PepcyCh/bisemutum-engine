add_rules("mode.debug", "mode.release")

set_languages("cxx20")

add_requires("spdlog")
add_requires("volk", "vulkan-memory-allocator", "spirv-reflect", "glfw", "glm")
if is_plat("windows") then
    add_requires("vcpkg::d3d12-memory-allocator")
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
    add_packages("volk", "vulkan-memory-allocator", "spirv-reflect", "glfw")
    if is_plat("windows") then
        add_defines("VK_USE_PLATFORM_WIN32_KHR")
        add_packages("vcpkg::d3d12-memory-allocator")
        add_syslinks("d3d12", "dxgi", "dxguid")
    end

target("bismuth")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("bismuth-graphics")
    add_packages("glfw")
