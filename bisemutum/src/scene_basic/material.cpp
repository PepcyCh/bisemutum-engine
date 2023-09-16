#include <bisemutum/scene_basic/material.hpp>

#include <bisemutum/runtime/logger.hpp>

namespace bi {

namespace {

struct MaterialDesc final {
    gfx::SurfaceModel surface_model = gfx::SurfaceModel::lit;
    gfx::BlendMode blend_mode = gfx::BlendMode::opaque;
    std::vector<std::pair<std::string, gfx::MaterialParameter>> value_params;
    std::vector<std::pair<std::string, rt::TAssetPtr<TextureAsset>>> texture_params;
    std::string material_function;

    rt::TAssetPtr<MaterialAsset> referenced_material;

    static auto from_value(serde::Value const& v, MaterialDesc &o) -> void {
        o.surface_model = v.value("surface_model", gfx::SurfaceModel::lit);
        o.blend_mode = v.value("blend_mode", gfx::BlendMode::opaque);
        o.material_function = v.value("material_function", std::string{""});
        o.referenced_material = v.value("referenced_material", rt::TAssetPtr<MaterialAsset>{});
        o.value_params.clear();
        o.texture_params.clear();
        if (auto params_value = v.try_at("params"); params_value) {
            for (auto& param_value : params_value->get_ref<serde::Value::Array>()) {
                auto name = param_value["name"].get<std::string>();
                auto& value_value = param_value["value"];
                if (auto value = value_value.try_get<float>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<float2>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<float3>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<float4>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<int>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<int2>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<int3>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<int4>(); value) {
                    o.value_params.emplace_back(std::move(name), value.value());
                } else if (auto value = value_value.try_get<rt::TAssetPtr<TextureAsset>>(); value) {
                    o.texture_params.emplace_back(std::move(name), value.value());
                } else {
                    throw serde::Exception{"Unknown material params type."};
                }
            }
        }
    }
    static auto to_value(serde::Value& v, MaterialDesc const& o) -> void {
        serde::to_value(v["surface_model"], o.surface_model);
        serde::to_value(v["blend_mode"], o.blend_mode);
        if (!o.referenced_material.empty()) {
            serde::to_value(v["referenced_material"], o.referenced_material);
        } else {
            serde::to_value(v["material_function"], o.material_function);
        }
        auto& params_value = v["params"];
        for (auto& [name, value] : o.value_params) {
            serde::to_value(params_value[name], value);
        }
        for (auto& [name, value] : o.texture_params) {
            serde::to_value(params_value[name], value);
        }
    }
};

} // namespace

auto MaterialAsset::load(Dyn<rt::IFile>::Ref file) -> rt::AssetAny {
    MaterialDesc desc{};
    try {
        desc = serde::Value::from_toml(file.read_string_data()).get<MaterialDesc>();
    } catch (std::exception const& e) {
        log::critical("general", "Failed to load material: {}.", e.what());
        return {};
    }

    if (!desc.referenced_material.empty()) {
        desc.referenced_material.load();
        if (!desc.referenced_material.asset()) {
            log::critical("general", "Failed to load material: Invalid referenced material.");
            return {};
        }
    }
    for (auto& [_, tex] : desc.texture_params) {
        tex.load();
        if (!tex.asset()) {
            log::critical("general", "Failed to load material: Invalid texture.");
            return {};
        }
    }

    gfx::Material mat{
        .surface_model = desc.surface_model,
        .blend_mode = desc.blend_mode,
        .value_params = std::move(desc.value_params),
        .material_function = std::move(desc.material_function),
        .referenced_material = &desc.referenced_material.asset()->material,
    };
    mat.texture_params.reserve(desc.texture_params.size());
    mat.sampler_params.reserve(desc.texture_params.size());
    for (size_t index = 0; auto& [name, tex] : desc.texture_params) {
        mat.texture_params[index] = {name, make_ref(tex.asset()->texture)};
        mat.sampler_params[index] = {std::move(name), tex.asset()->sampler.value()};
        ++index;
    }
    return mat;
}

}
