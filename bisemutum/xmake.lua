add_requires("fmt", "spdlog", "glm", "entt", "magic_enum")
add_requires("glfw", "directxshadercompiler", "crypto-algorithms")
add_requires("vulkan-headers", "vulkan-memory-allocator")
add_requires("nlohmann_json", "toml++", "assimp", "mikktspace", "stb")
if is_plat("windows") then
    add_requires("d3d12-memory-allocator")
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

    add_packages("fmt", "spdlog", "glm", "entt", "magic_enum", {public = true})
    add_packages("glfw", "directxshadercompiler", "crypto-algorithms")
    add_packages("vulkan-headers", "vulkan-memory-allocator")
    add_defines("VK_NO_PROTOTYPES")
    add_packages("nlohmann_json", "toml++", "assimp", "mikktspace", "stb")
    if is_plat("windows") then
        add_defines("VK_USE_PLATFORM_WIN32_KHR", "NOMINMAX")
        add_packages("d3d12-memory-allocator")
        add_syslinks("d3d12", "dxgi", "dxguid")
        add_deps("winpix")
    else
        remove_files("src/rhi/backend_d3d12/**.cpp")
        remove_headerfiles("src/rhi/backend_d3d12/**.hpp")
    end
    after_build(function (target) 
        io.writefile(target:targetdir().."/bisemutum_path.txt", target:scriptdir())
    end)

target("bisemutum-engine")
    set_kind("binary")
    add_files("bin/main.cpp")
    add_deps("bisemutum-lib")
