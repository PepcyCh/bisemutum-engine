# Bismuth Renderer

WIP slowly...

My personal renderer using C++20 and modern graphics API (Vulkan and D3D12).

TODO

* [ ] A simple RHI on Vulkan and D3D12
* [ ] Render graph on that RHI

...

## Build

This project use [Xmake](https://xmake.io/) to build.

## Dependencies

* spdlog
* glfw
* glm
* volk
* Vulkan Memory Allocator
* SPIRV-reflect
* DirectXShaderCompiler (Windows only)
* glslang (through vcpkg)
* D3D12 Memory Allocator (through vcpkg) (Windows only)
