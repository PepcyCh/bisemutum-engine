#pragma once

namespace bi::gfx {

enum class DrawableHandle : size_t {
    invalid = static_cast<size_t>(-1),
};

enum class CameraHandle : size_t {
    invalid = static_cast<size_t>(-1),
};

enum class BufferHandle : size_t {
    invalid = static_cast<size_t>(-1),
};

enum class TextureHandle : size_t {
    invalid = static_cast<size_t>(-1),
};

enum class RenderedObjectListHandle : size_t {
    invalid = static_cast<size_t>(-1),
};

}
