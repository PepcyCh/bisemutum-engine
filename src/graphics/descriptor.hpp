#pragma once

#include "core/container.hpp"
#include "core/span.hpp"
#include "core/variant.hpp"
#include "resource.hpp"
#include "sampler.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

using BindingResource = Variant<
    std::monostate,
    Ref<Sampler>,
    BufferRange,
    TextureView,
    Vec<Ref<Sampler>>,
    Vec<BufferRange>,
    Vec<TextureView>
>;

enum class DescriptorType : uint8_t {
    eNone,
    eSampler,
    eUniformBuffer,
    eStorageBuffer,
    eRWStorageBuffer,
    eSampledTexture,
    eStorageTexture,
    eRWStorageTexture,
};

inline bool IsDescriptorTypeBuffer(DescriptorType type) {
    return type == DescriptorType::eUniformBuffer || type == DescriptorType::eStorageBuffer
        || type == DescriptorType::eRWStorageBuffer;
}
inline bool IsDescriptorTypeTexture(DescriptorType type) {
    return type == DescriptorType::eSampledTexture || type == DescriptorType::eStorageTexture
        || type == DescriptorType::eRWStorageTexture;
}
inline bool IsDescriptorTypeSampler(DescriptorType type) {
    return type == DescriptorType::eSampler;
}

struct DescriptorSetLayoutBinding {
    DescriptorType type = DescriptorType::eNone;
    TextureViewDimension tex_dim = TextureViewDimension::e2D;
    ResourceFormat tex_format = ResourceFormat::eUndefined;
    uint32_t count = 1;
    uint32_t struct_stride = 0;
    Span<Ref<Sampler>> immutable_samples = {}; // TODO - support immutable/static sampler
};
struct DescriptorSetLayout {
    Vec<DescriptorSetLayoutBinding> bindings;
};

template <typename T>
struct PushConstants { T value; };

struct PipelineLayout {
    Vec<DescriptorSetLayout> sets_layout;
    uint32_t push_constants_size;
};

struct ShaderParams {
    Vec<BindingResource> resources;

    bool operator==(const ShaderParams &rhs) const = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END

template <>
struct std::hash<bismuth::gfx::DescriptorSetLayout> {
    size_t operator()(const bismuth::gfx::DescriptorSetLayout &v) const noexcept {
        size_t hash = 0;
        for (const auto &binding : v.bindings) {
            hash = bismuth::HashCombine(hash, bismuth::Hash(binding.type, binding.count));
        }
        return hash;
    }
};

template <>
struct std::hash<bismuth::gfx::ShaderParams> {
    size_t operator()(const bismuth::gfx::ShaderParams &v) const noexcept {
        size_t hash = 0;
        for (const auto &resource : v.resources) {
            size_t resource_hash = resource.Match(
                [](std::monostate) -> size_t {
                    return 0;
                },
                []<typename T>(const bismuth::Vec<T> &r) {
                    bismuth::ByteHash<T> hasher;
                    size_t hash = 0;
                    for (const auto &rr : r) {
                        hash = bismuth::HashCombine(hash, hasher(rr));
                    }
                    return hash;
                },
                []<typename T>(const T &r) {
                    bismuth::ByteHash<T> hasher;
                    return hasher(r);
                }
            );
            hash = bismuth::HashCombine(hash, resource_hash);
        }
        return hash;
    }
};
