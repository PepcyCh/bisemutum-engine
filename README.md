# Bisemutum Engine

WIP very slowly...

My personal renderer using C++20 and modern graphics API (Vulkan and D3D12).

## Modules and TODO

* [x] core
  * [x] main loop
  * [x] some helpers
* [x] RHI on Vulkan and D3D12
  * [ ] ImGUI integration
  * [ ] tessellation
  * [ ] meshlet pipeline
  * [ ] raytracing pipeline
  * [ ] multisample texture and resolve
* [x] graphics
  * [x] render graph
  * [x] mipmap generation
  * [x] shader resources definition and pipeline layout generated from C++ structs
  * [ ] PBR material
* [x] runtime
  * [x] window and inputs based on GLFW
  * [x] scene objects hierarchy
  * [x] virtual filesystem
    * [ ] pak file ?
  * [x] ECS wrapper over EnTT
  * [x] simple asset manager
    * [ ] async load ?
  * [x] simple frame timer
* [ ] basic scene objects
  * [x] camera
  * [x] static mesh
  * [x] material
  * [ ] lights
* [ ] basic renderer
  * [ ] forward pipeline
  * [ ] deferred pipeline

## Build

This project uses [Xmake](https://xmake.io/) to build.

On Windows, clang-cl is preferred.

## Used Thirdparty

in source

* AnyAny (Apache-2.0)
* volk (MIT)
* pep-cprep (MIT)

directly via xmake

* fmt ([LICENSE](https://github.com/fmtlib/fmt/blob/master/LICENSE))
* spdlog (MIT)
* EnTT (MIT)
* magic enum (MIT)
* GLFW (zlib)
* GLM (MIT)
* Vulkan Memory Allocator (MIT)
* D3D12 Memory Allocator (MIT)
* DirectXShaderCompiler ([LICENSE](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT))
* crypto algorithms
* stb
* assimp ([LICENSE](https://github.com/assimp/assimp/blob/master/LICENSE))
* nlohmann json (MIT)
* toml++ (MIT)
* mikktspace
