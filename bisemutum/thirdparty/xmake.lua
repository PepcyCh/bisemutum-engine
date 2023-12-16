target("anyany")
    set_kind("headeronly")
    add_headerfiles("AnyAny/include/**.hpp")
    add_includedirs("AnyAny/include", {public = true})
    add_cxxflags("/Zc:__cplusplus", "/permissive-", "/wd5051", "/wd4848", {tools = {"cl", "clang_cl"}, public = true})
    add_cxxflags("/Zc:preprocessor", {tools = {"cl"}, public = true})

target("winpix")
    set_kind("headeronly")
    add_headerfiles("WinPixEventRuntime/include/**.h")
    add_includedirs("WinPixEventRuntime/include", {public = true})
    if is_plat("windows") then
        add_linkdirs("WinPixEventRuntime/bin/x64", {public = true})
        add_links("WinPixEventRuntime", {public = true})
        if is_arch("arm.*") then
            after_build(function (target)
                os.cp(path.join(os.scriptdir(), "WinPixEventRuntime/bin/ARM64/WinPixEventRuntime.dll"), target:targetdir())
            end)
        else
            after_build(function (target)
                os.cp(path.join(os.scriptdir(), "WinPixEventRuntime/bin/x64/WinPixEventRuntime.dll"), target:targetdir())
            end)
        end
    end

target("pep-cprep")
    set_kind("static")
    add_files("pep-cprep/**.cpp")
    add_headerfiles("pep-cprep/**.hpp")
    add_includedirs("pep-cprep/include", {public = true})

target("imgui-file-dialog")
    set_kind("static")
    add_files("ImGuiFileDialog/*.cpp")
    add_headerfiles("ImGuiFileDialog/*.h")
    add_includedirs("ImGuiFileDialog", {public = true})
    add_packages("imgui")
