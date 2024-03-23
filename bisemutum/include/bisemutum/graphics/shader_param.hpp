#pragma once

#include <string>
#include <vector>

#include <fmt/format.h>

#include "resource.hpp"
#include "sampler.hpp"
#include "../math/math.hpp"
#include "../prelude/template_utils.hpp"
#include "../rhi/descriptor.hpp"

namespace bi::gfx {

struct AccelerationStructure;

struct BufferParam final {
    Ptr<Buffer> buffer;
    uint64_t offset = 0;
    uint64_t size = ~0ull;
};
struct TextureParam final {
    Ptr<Texture> texture;
    uint32_t base_level = 0;
    uint32_t num_levels = ~0u;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;
};
struct RWTextureParam final {
    Ptr<Texture> texture;
    uint32_t mip_level = 0;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;
};

// The below 5 'TXxx' structs are just hint for C++ codes.

template <typename T>
struct TCbv final {
    Ptr<Buffer> buffer;
    uint64_t offset = 0;
    uint64_t size = ~0ull;
};

template <typename T>
struct TSrvBuffer final {
    Ptr<Buffer> buffer;
    uint64_t offset = 0;
    uint64_t size = ~0ull;
};
template <typename T>
struct TSrvTexture final {
    Ptr<Texture> texture;
    uint32_t base_level = 0;
    uint32_t num_levels = ~0u;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;
};

template <typename T>
struct TUavBuffer final {
    Ptr<Buffer> buffer;
    uint64_t offset = 0;
    uint64_t size = ~0ull;
};
template <typename T>
struct TUavTexture final {
    Ptr<Texture> texture;
    uint32_t mip_level = 0;
    uint32_t base_layer = 0;
    uint32_t num_layers = ~0u;
};

namespace shader {

template <typename T>
struct ConstantBuffer final {};
template <typename T>
struct StructuredBuffer final {};
template <typename T>
struct RWStructuredBuffer final {};

struct ByteAddressBuffer final {};
struct RWByteAddressBuffer final {};

struct Texture1D final {};
struct Texture2D final {};
struct Texture3D final {};
struct TextureCube final {};
struct Texture1DArray final {};
struct Texture2DArray final {};
struct TextureCubeArray final {};

template <typename T>
concept RWTextureElement = traits::OneOf<
    T,
    float, float2, float3, float4,
    int, int2, int3, int4,
    uint, uint2, uint3, uint4
>;
template <RWTextureElement T>
struct RWTexture1D final {};
template <RWTextureElement T>
struct RWTexture2D final {};
template <RWTextureElement T>
struct RWTexture3D final {};
template <RWTextureElement T>
struct RWTexture1DArray final {};
template <RWTextureElement T>
struct RWTexture2DArray final {};

struct RaytracingAccelerationStructure final {
    Ptr<AccelerationStructure> accel;
};

struct SamplerState final {
    Ptr<Sampler> sampler;
};

} // namespace shader


template <
    typename Type,
    ConstexprStringLit TypeName,
    ConstexprStringLit Name,
    typename ArraySizeT = int,
    bool is_include = false,
    rhi::ResourceFormat format = rhi::ResourceFormat::undefined
>
struct TShaderParameterMetadata final {};

template <typename T>
struct ShaderParameterGpuAlignment final {
    static constexpr size_t value = alignof(T);
};

#define BASIC_SHADER_PARAM_METADATA(ty, sz, align) \
    template <> \
    struct ShaderParameterGpuAlignment<ty> final { \
        static constexpr size_t value = align; \
    }; \
    template <ConstexprStringLit Name, typename ArraySizeT> \
    struct TShaderParameterMetadata<ty, "", Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final { \
        static constexpr ConstexprStringLit type_name{#ty}; \
        static constexpr size_t size = sz; \
        static constexpr size_t alignment = align; \
        static constexpr size_t cpu_alignment = align; \
        static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::none; \
        static constexpr size_t cpu_size = sizeof(ty); \
    };
BASIC_SHADER_PARAM_METADATA(float, 4, 4)
BASIC_SHADER_PARAM_METADATA(float2, 8, 8)
BASIC_SHADER_PARAM_METADATA(float3, 12, 16)
BASIC_SHADER_PARAM_METADATA(float4, 16, 16)
BASIC_SHADER_PARAM_METADATA(int, 4, 4)
BASIC_SHADER_PARAM_METADATA(int2, 8, 8)
BASIC_SHADER_PARAM_METADATA(int3, 12, 16)
BASIC_SHADER_PARAM_METADATA(int4, 16, 16)
BASIC_SHADER_PARAM_METADATA(uint, 4, 4)
BASIC_SHADER_PARAM_METADATA(uint2, 8, 8)
BASIC_SHADER_PARAM_METADATA(uint3, 12, 16)
BASIC_SHADER_PARAM_METADATA(uint4, 16, 16)
BASIC_SHADER_PARAM_METADATA(float4x4, 64, 16)
#undef BASIC_SHADER_PARAM_METADATA

template <typename T, ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::ConstantBuffer<T>, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(TCbv<shader::ConstantBuffer<T>>);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(TCbv<shader::ConstantBuffer<T>>);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::uniform_buffer;
    using ElementType = T;
    static constexpr size_t structure_stride = sizeof(T);
};

template <typename T, ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::StructuredBuffer<T>, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(TSrvBuffer<shader::StructuredBuffer<T>>);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(TSrvBuffer<shader::StructuredBuffer<T>>);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::read_only_storage_buffer;
    using ElementType = T;
    static constexpr size_t structure_stride = sizeof(T);
};

template <typename T, ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::RWStructuredBuffer<T>, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(TUavBuffer<shader::RWStructuredBuffer<T>>);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(TUavBuffer<shader::RWStructuredBuffer<T>>);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::read_write_storage_buffer;
    using ElementType = T;
    static constexpr size_t structure_stride = sizeof(T);
};

template <ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::ByteAddressBuffer, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(TSrvBuffer<shader::ByteAddressBuffer>);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(TSrvBuffer<shader::ByteAddressBuffer>);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::read_only_storage_buffer;
    static constexpr size_t structure_stride = 0;
};

template <ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::RWByteAddressBuffer, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(TUavBuffer<shader::RWByteAddressBuffer>);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(TUavBuffer<shader::RWByteAddressBuffer>);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::read_write_storage_buffer;
    static constexpr size_t structure_stride = 0;
};

#define TEXTURE_SHADER_PARAM_METADATA(tex_ty, view_ty) \
    template <ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT> \
    struct TShaderParameterMetadata<shader::tex_ty, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final { \
        static constexpr ConstexprStringLit type_name{TypeName.value}; \
        static constexpr size_t size = sizeof(TSrvTexture<shader::tex_ty>); \
        static constexpr size_t alignment = 8; \
        static constexpr size_t cpu_alignment = alignof(TSrvTexture<shader::tex_ty>); \
        static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::sampled_texture; \
        static constexpr rhi::TextureViewType texture_view_type = rhi::TextureViewType::view_ty; \
    };
TEXTURE_SHADER_PARAM_METADATA(Texture1D, d1)
TEXTURE_SHADER_PARAM_METADATA(Texture2D, d2)
TEXTURE_SHADER_PARAM_METADATA(Texture3D, d3)
TEXTURE_SHADER_PARAM_METADATA(TextureCube, cube)
TEXTURE_SHADER_PARAM_METADATA(Texture1DArray, d1_array)
TEXTURE_SHADER_PARAM_METADATA(Texture2DArray, d2_array)
TEXTURE_SHADER_PARAM_METADATA(TextureCubeArray, cube_array)
#undef TEXTURE_SHADER_PARAM_METADATA

#define RWTEXTURE_SHADER_PARAM_METADATA(tex_ty, view_ty) \
    template <shader::RWTextureElement T, ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT, rhi::ResourceFormat format> \
    struct TShaderParameterMetadata<shader::tex_ty<T>, TypeName, Name, ArraySizeT, false, format> final { \
        static constexpr ConstexprStringLit type_name{TypeName.value}; \
        static constexpr size_t size = sizeof(TUavTexture<shader::tex_ty<T>>); \
        static constexpr size_t alignment = 8; \
        static constexpr size_t cpu_alignment = alignof(TUavTexture<shader::tex_ty<T>>); \
        static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::read_write_storage_texture; \
        static constexpr rhi::TextureViewType texture_view_type = rhi::TextureViewType::view_ty; \
    };
RWTEXTURE_SHADER_PARAM_METADATA(RWTexture1D, d1)
RWTEXTURE_SHADER_PARAM_METADATA(RWTexture2D, d2)
RWTEXTURE_SHADER_PARAM_METADATA(RWTexture3D, d3)
RWTEXTURE_SHADER_PARAM_METADATA(RWTexture1DArray, d1_array)
RWTEXTURE_SHADER_PARAM_METADATA(RWTexture2DArray, d2_array)
#undef RWTEXTURE_SHADER_PARAM_METADATA

template <ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::RaytracingAccelerationStructure, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(shader::RaytracingAccelerationStructure);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(shader::RaytracingAccelerationStructure);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::acceleration_structure;
};

template <ConstexprStringLit TypeName, ConstexprStringLit Name, typename ArraySizeT>
struct TShaderParameterMetadata<shader::SamplerState, TypeName, Name, ArraySizeT, false, rhi::ResourceFormat::undefined> final {
    static constexpr ConstexprStringLit type_name{TypeName.value};
    static constexpr size_t size = sizeof(shader::SamplerState);
    static constexpr size_t alignment = 8;
    static constexpr size_t cpu_alignment = alignof(shader::SamplerState);
    static constexpr rhi::DescriptorType descriptor_type = rhi::DescriptorType::sampler;
};

struct ShaderParameterMetadata final {
    rhi::DescriptorType descriptor_type = rhi::DescriptorType::none;
    rhi::ResourceFormat format = rhi::ResourceFormat::undefined;
    rhi::TextureViewType texture_view_type = rhi::TextureViewType::automatic;
    std::string type_name;
    std::string var_name;
    std::vector<uint32_t> array_sizes;
    // If `descriptor_type` is `none`, `size` means size in uniform buffer.
    // Otherwise, `size` means size in cpu buffer.
    size_t size = 0;
    size_t alignment = 0;
    size_t cpu_alignment = 0;
    size_t structure_stride = 0;
};
struct ShaderParameterMetadataList final {
    auto bind_group_layout(
        uint32_t set, BitFlags<rhi::ShaderStage> visibility = rhi::ShaderStage::all_stages
    ) const -> rhi::BindGroupLayout;

    auto generated_shader_definition(uint32_t set, uint32_t samplers_set) const -> std::string;

    std::vector<ShaderParameterMetadata> params;
};

template <typename T, typename ValueT = void>
struct IsShaderParameterStruct final {
    static constexpr bool value = false;
};
template <typename T>
struct IsShaderParameterStruct<T, std::enable_if_t<std::is_class_v<typename T::ParamsTypeList>, void>> final {
    static constexpr bool value = true;
};

template <
    typename Type,
    ConstexprStringLit TypeName,
    ConstexprStringLit Name,
    typename ArraySizeT,
    bool is_include,
    rhi::ResourceFormat format
>
auto shader_parameter_metadata_of(
    TShaderParameterMetadata<Type, TypeName, Name, ArraySizeT, is_include, format>
) -> std::vector<ShaderParameterMetadata>;

template <typename T>
struct ShaderParameterMetadataListHelper;
template <typename... Ts>
struct ShaderParameterMetadataListHelper<TypeList<Ts...>> final {
    static auto get_params() -> std::vector<ShaderParameterMetadata> {
        if constexpr (sizeof...(Ts) == 0) {
            return {};
        } else {
            // We need to write template parameter of std::vector here,
            // or type of `params_2d` will be incorrect when sizeof...(Ts) == 1.
            auto params_2d = std::vector<std::vector<ShaderParameterMetadata>>{
                shader_parameter_metadata_of(Ts{})...
            };
            size_t count = 0;
            for (auto const& inner_vec : params_2d) {
                count += inner_vec.size();
            }
            std::vector<ShaderParameterMetadata> params(count);
            size_t index = 0;
            for (auto& inner_vec : params_2d) {
                for (auto& param : inner_vec) {
                    params[index++] = std::move(param);
                }
            }
            return params;
        }
    };
};

template <
    typename Type,
    ConstexprStringLit TypeName,
    ConstexprStringLit Name,
    typename ArraySizeT,
    bool is_include,
    rhi::ResourceFormat format
>
auto shader_parameter_metadata_of(
    TShaderParameterMetadata<Type, TypeName, Name, ArraySizeT, is_include, format>
) -> std::vector<ShaderParameterMetadata> {
    using MetadataType = TShaderParameterMetadata<Type, TypeName, Name, ArraySizeT, is_include, format>;

    auto array_sizes = get_array_sizes<ArraySizeT, uint32_t>();

    if constexpr (IsShaderParameterStruct<Type>::value) {
        auto params = ShaderParameterMetadataListHelper<typename Type::ParamsTypeList>::get_params();
        if constexpr (!is_include) {
            for (auto& param : params) {
                param.var_name = fmt::format("{}_{}", Name.value, param.var_name);
            }
        }
        return params;
    } else {
        ShaderParameterMetadata metadata{
            .descriptor_type = MetadataType::descriptor_type,
            .format = format,
            .type_name = MetadataType::type_name.value,
            .var_name = Name.value,
            .array_sizes = std::move(array_sizes),
            .size = MetadataType::size,
            .alignment = MetadataType::alignment,
            .cpu_alignment = MetadataType::cpu_alignment,
        };
        if constexpr (
            MetadataType::descriptor_type == rhi::DescriptorType::uniform_buffer
            || MetadataType::descriptor_type == rhi::DescriptorType::read_only_storage_buffer
            || MetadataType::descriptor_type == rhi::DescriptorType::read_write_storage_buffer
        ) {
            metadata.structure_stride = MetadataType::structure_stride;
        }
        if constexpr (
            MetadataType::descriptor_type == rhi::DescriptorType::sampled_texture
            || MetadataType::descriptor_type == rhi::DescriptorType::read_write_storage_texture
        ) {
            metadata.texture_view_type = MetadataType::texture_view_type;
        }
        return {metadata};
    }
}

template <typename ParamsStruct>
auto shader_parameter_metadata_list_of() -> ShaderParameterMetadataList {
    ShaderParameterMetadataList list{
        .params = ShaderParameterMetadataListHelper<typename ParamsStruct::ParamsTypeList>::get_params(),
    };
    return list;
}

#define BI_SHADER_PARAMETERS_BEGIN(name) struct name final { \
    static constexpr ::bi::ConstexprStringLit type_name{#name}; \
    template <size_t index> struct XParamsTuple { using type = XParamsTuple<index - 1>::type; }; \
    template <> struct XParamsTuple<__LINE__> { using type = ::bi::TypeList<>; }; \
    static auto metadata_list() -> ::bi::gfx::ShaderParameterMetadataList const& { \
        static const auto metadata_list = ::bi::gfx::shader_parameter_metadata_list_of<name>(); \
        return metadata_list; \
    }

#define BI_SHADER_PARAMETER(ty, name) alignas(::bi::gfx::ShaderParameterGpuAlignment<ty>::value) ty name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype(::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<ty, "", #name>>(XParamsTuple<__LINE__ - 1>::type{})); \
    };
#define BI_SHADER_PARAMETER_ARRAY(ty, name, arr) alignas(::bi::gfx::ShaderParameterGpuAlignment<ty>::value) ty name arr; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype(::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<ty, "", #name, int arr>>(XParamsTuple<__LINE__ - 1>::type{})); \
    };

#define BI_SHADER_PARAMETER_INCLUDE(ty, name) alignas(::bi::gfx::ShaderParameterGpuAlignment<ty>::value) ty name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype(::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<ty, "", #name, int, true>>(XParamsTuple<__LINE__ - 1>::type{})); \
    };

#define BI_SHADER_PARAMETER_CBV(ty, name) ::bi::gfx::TCbv<::bi::gfx::shader::ty> name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };

#define BI_SHADER_PARAMETER_SRV_BUFFER(ty, name) ::bi::gfx::TSrvBuffer<::bi::gfx::shader::ty> name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_SRV_BUFFER_ARRAY(ty, name, arr) ::bi::gfx::TSrvBuffer<::bi::gfx::shader::ty> name arr; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name, int arr>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_SRV_TEXTURE(ty, name) ::bi::gfx::TSrvTexture<::bi::gfx::shader::ty> name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_SRV_TEXTURE_ARRAY(ty, name, arr) ::bi::gfx::TSrvTexture<::bi::gfx::shader::ty> name arr; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name, int arr>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_SRV_ACCEL(ty, name) ::bi::gfx::shader::ty name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };

#define BI_SHADER_PARAMETER_UAV_BUFFER(ty, name) ::bi::gfx::TUavBuffer<::bi::gfx::shader::ty> name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_UAV_BUFFER_ARRAY(ty, name, arr) ::bi::gfx::TUavBuffer<::bi::gfx::shader::ty> name arr; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name, int arr>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_UAV_TEXTURE(ty, name, format) ::bi::gfx::TUavTexture<::bi::gfx::shader::ty> name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name, int, false, format>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_UAV_TEXTURE_ARRAY(ty, name, arr, format) ::bi::gfx::TUavTexture<::bi::gfx::shader::ty> name arr; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name, int arr, false, format>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };

#define BI_SHADER_PARAMETER_SAMPLER(ty, name) ::bi::gfx::shader::ty name; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };
#define BI_SHADER_PARAMETER_SAMPLER_ARRAY(ty, name, arr) ::bi::gfx::shader::ty name arr; \
    template <> struct XParamsTuple<__LINE__> { \
        using type = decltype( \
            ::bi::type_push_back<::bi::gfx::TShaderParameterMetadata<::bi::gfx::shader::ty, #ty, #name, int arr>>(XParamsTuple<__LINE__ - 1>::type{}) \
        ); \
    };

#define BI_SHADER_PARAMETERS_END() using ParamsTypeList = XParamsTuple<__LINE__ - 1>::type; };


struct GraphicsManager;
struct GraphicsPassContext;

struct ShaderParameter {
    auto initialize(ShaderParameterMetadataList metadata_list, bool use_gpu_only_memory = false) -> void;
    template <typename ParamStruct>
    auto initialize(bool use_gpu_only_memory = false) -> void {
        initialize(shader_parameter_metadata_list_of<ParamStruct>(), use_gpu_only_memory);
    }

    operator bool() const { return allocated_; }
    auto reset() -> void;

    auto metadata_list() const -> ShaderParameterMetadataList const& { return metadata_list_; }

    template <typename ParamStruct>
    auto typed_data() const -> ParamStruct const* {
        return reinterpret_cast<ParamStruct const*>(data());
    }
    template <typename ParamStruct>
    auto mutable_typed_data() -> ParamStruct* {
        return reinterpret_cast<ParamStruct*>(mutable_data());
    }
    auto data() const -> std::byte const*;
    auto mutable_data() -> std::byte*;

    template <typename T>
    auto typed_data_offset(size_t offset) const -> T const* {
        return reinterpret_cast<T const*>(data_offset(offset));
    }
    template <typename T>
    auto mutable_typed_data_offset(size_t offset) -> T* {
        return reinterpret_cast<T*>(mutable_data_offset(offset));
    }
    auto data_offset(size_t offset) const -> std::byte const*;
    auto mutable_data_offset(size_t offset) -> std::byte*;

    auto uniform_buffer() -> Buffer& { return uniform_buffer_; }
    auto uniform_buffer() const -> Buffer const& { return uniform_buffer_; }

    auto update_uniform_buffer() -> void;

private:
    auto start_lifetime() -> void;

    ShaderParameterMetadataList metadata_list_;
    std::vector<std::byte> data_;
    std::byte* data_start_ = nullptr;
    Buffer uniform_buffer_;
    struct UniformRange final {
        size_t cpu_offset;
        size_t gpu_offset;
        size_t size;
    };
    std::vector<UniformRange> uniform_ranges_;
    uint32_t last_update_frame_ = 0xffffffffu;
    uint32_t dirty_count_ = 0;
    bool allocated_ = false;
};

template <typename ParamStruct>
struct TShaderParameter final : ShaderParameter {
    auto initialize(bool use_gpu_only_memory = false) -> void {
        ShaderParameter::initialize(shader_parameter_metadata_list_of<ParamStruct>(), use_gpu_only_memory);
    }

    auto typed_data() const -> ParamStruct const* {
        return ShaderParameter::typed_data<ParamStruct>();
    }
    auto mutable_typed_data() -> ParamStruct* {
        return ShaderParameter::mutable_typed_data<ParamStruct>();
    }
};

}
