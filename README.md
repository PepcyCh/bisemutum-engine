# Bisemutum Engine

WIP very slowly...

My personal renderer using C++20 and modern graphics API (Vulkan and D3D12).

## Features

* Graphics
  * RHI on Vulkan and D3D12
  * ImGui Integration on RHI
  * Render Graph
  * Generate shader resources definition and pipeline layout from C++ structs
* Rendering
  * Basic PBR
  * CSM for Directional Lights
  * Skybox & IBL
* Core
  * Custom Static Reflection
    * Reflection on `enum` is based on Magic Enum
  * Polymorphism using AnyAny
* Runtime
  * ECS using EnTT & Scene Hierarchy & Prefab
  * Basic Asset Manager
  * Basic Virtual File System
  * Saving & Loading
* Editor
  * Component Editor based on Reflection
  * Scene Objects
  * Import Model via Assimp

### Features to be Developed Next

* Shadow Mapping for Point & Spot Light
* Bloom
* Basic Resource Browser

## Build

This project uses [Xmake](https://xmake.io/) to build.

On Windows, clang-cl is preferred.

## Used Thirdparty

Core

* [AnyAny](https://github.com/kelbon/AnyAny) (Apache-2.0) (in source)
* [magic enum](https://github.com/Neargye/magic_enum) (MIT)
* [fmt](https://github.com/fmtlib/fmt) ([LICENSE](https://github.com/fmtlib/fmt/blob/master/LICENSE))
* [GLM](https://github.com/g-truc/glm) (MIT)

Runtime

* [EnTT](https://github.com/skypjack/entt) (MIT)
* [spdlog](https://github.com/gabime/spdlog) (MIT)

Graphics

* [volk](https://github.com/zeux/volk) (MIT) (in source)
* [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (MIT)
* [D3D12 Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator) (MIT)
* [pep-cprep](https://github.com/PepcyCh/pep-cprep) (MIT) (in source)
* [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) ([LICENSE](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT))

Window

* [GLFW](https://github.com/glfw/glfw) (zlib)

Editor

* [Dear ImGui](https://github.com/ocornut/imgui) (MIT)
* [ImGui File Dialog](https://github.com/aiekick/ImGuiFileDialog) (MIT) (in source)

Asset

* [stb](https://github.com/nothings/stb) (MIT)
* [tinyexr](https://github.com/syoyo/tinyexr) (BSD-3)
* [assimp](https://github.com/assimp/assimp) ([LICENSE](https://github.com/assimp/assimp/blob/master/LICENSE))

Misc

* [crypto algorithms](https://github.com/KorewaWatchful/crypto-algorithms)
* [mikktspace](https://github.com/mmikk/MikkTSpace)
* [nlohmann json](https://github.com/nlohmann/json) (MIT)
* [toml++](https://github.com/marzer/tomlplusplus) (MIT)

