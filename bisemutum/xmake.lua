add_requires("fmt", "spdlog", "miniz", "glm", "entt", "magic_enum")
add_requires("glfw", "directxshadercompiler", "crypto-algorithms")
add_requires("nlohmann_json", "toml++", "assimp", "tinygltf", "mikktspace", "stb", "tinyexr")
add_requires("imgui v1.89.9-docking", {configs = {glfw = true}})

graphics_backend_vulkan = false
graphics_backend_d3d12 = false

if is_plat("windows") or is_plat("linux") then
    add_requires("vulkan-headers", "vulkan-memory-allocator")
    graphics_backend_vulkan = true
end
if is_plat("windows") then
    add_requires("d3d12-memory-allocator")
    graphics_backend_d3d12 = true
end

includes("thirdparty/xmake.lua")

target("bisemutum-lib")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/**.cpp", "src/**.c")
    add_headerfiles("include/**.hpp")
    add_headerfiles("src/**.hpp", "src/**.h", {install = false})

    add_deps("anyany", {public = true})
    add_deps("pep-cprep")
    add_deps("imgui-file-dialog", "imguizmo")

    add_packages("fmt", "spdlog", "miniz", "glm", "entt", "magic_enum", "imgui", {public = true})
    add_defines("MAGIC_ENUM_RANGE_MAX=8192")
    add_packages("glfw", "directxshadercompiler", "crypto-algorithms")
    add_packages("nlohmann_json", "toml++", "assimp", "tinygltf", "mikktspace", "stb", "tinyexr")

    if graphics_backend_vulkan then
        add_defines("BI_GRAPHICS_BACKEND_VULKAN")
        add_defines("VK_NO_PROTOTYPES")
        add_packages("vulkan-headers", "vulkan-memory-allocator")
    else
        remove_files("src/rhi/backend_vulkan/**.cpp")
        remove_headerfiles("src/rhi/backend_vulkan/**.hpp")
    end
    if graphics_backend_d3d12 then
        add_defines("BI_GRAPHICS_BACKEND_D3D12")
        add_packages("d3d12-memory-allocator")
        add_syslinks("d3d12", "dxgi", "dxguid")
    else
        remove_files("src/rhi/backend_d3d12/**.cpp")
        remove_headerfiles("src/rhi/backend_d3d12/**.hpp")
    end

    if is_plat("windows") then 
        add_defines("VK_USE_PLATFORM_WIN32_KHR", "NOMINMAX")
        add_deps("winpix")
    end

    after_build(function (target) 
        io.writefile(target:targetdir().."/bisemutum_path.txt", target:scriptdir())
    end)

target("bisemutum-engine")
    set_kind("binary")
    add_files("bin/main.cpp")
    add_deps("bisemutum-lib")
