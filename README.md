# Bismuth Renderer

WIP slowly...

My personal renderer using C++20 and modern graphics API (Vulkan and D3D12).

TODO

* [x] A simple RHI on Vulkan and D3D12
  * [ ] multisample texture and resolve
  * [ ] tessellation
  * [ ] multi-queue

* [x] Render graph on that RHI

* [ ] basic components, serde

* [ ] shader manager & material system

...

## Build

This project use [Xmake](https://xmake.io/) to build.

## Dependencies

* spdlog
* stb
* glfw
* glm
* volk
* Vulkan Memory Allocator
* SPIRV-reflect
* glslang
* DirectXShaderCompiler (Windows only)
* D3D12 Memory Allocator (through vcpkg) (Windows only)
