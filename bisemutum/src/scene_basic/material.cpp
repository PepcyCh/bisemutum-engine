#include <bisemutum/scene_basic/material.hpp>

#include <bisemutum/scene_basic/texture.hpp>
#include <bisemutum/math/math.hpp>
#include <bisemutum/runtime/logger.hpp>
#include <bisemutum/prelude/misc.hpp>

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
            serde::Value param_value;
            param_value["name"] = name;
            std::visit(
                FunctorsHelper{
                    [&param_value](float v) { serde::to_value(param_value["value"], v); },
                    [&param_value](float2 v) { serde::to_value(param_value["value"], v); },
                    [&param_value](float3 v) { serde::to_value(param_value["value"], v); },
                    [&param_value](float4 v) { serde::to_value(param_value["value"], v); },
                    [&param_value](int v) { serde::to_value(param_value["value"], v); },
                    [&param_value](int2 v) { serde::to_value(param_value["value"], v); },
                    [&param_value](int3 v) { serde::to_value(param_value["value"], v); },
                    [&param_value](int4 v) { serde::to_value(param_value["value"], v); },
                },
                value
            );
            params_value.push_back(std::move(param_value));
        }
        for (auto& [name, value] : o.texture_params) {
            serde::Value param_value;
            param_value["name"] = name;
            serde::to_value(param_value["value"], value);
            params_value.push_back(std::move(param_value));
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

    MaterialAsset mat{
        .material = {
            .surface_model = desc.surface_model,
            .blend_mode = desc.blend_mode,
            .value_params = std::move(desc.value_params),
            .material_function = std::move(desc.material_function),
            .referenced_material = &desc.referenced_material.asset()->material,
        },
    };
    mat.referenced_material = desc.referenced_material.asset_id();
    mat.referenced_textures.reserve(desc.texture_params.size());
    mat.material.texture_params.reserve(desc.texture_params.size());
    mat.material.sampler_params.reserve(desc.texture_params.size());
    for (auto& [name, tex] : desc.texture_params) {
        mat.referenced_textures.emplace_back(name, tex.asset_id());
        mat.material.sampler_params.emplace_back(name + "_sampler", tex.asset()->sampler.value());
        mat.material.texture_params.emplace_back(std::move(name), make_ref(tex.asset()->texture));
    }
    mat.material.update_shader_parameter();
    return mat;
}

auto MaterialAsset::save(Dyn<rt::IFile>::Ref file) const -> void {
    MaterialDesc desc{
        .surface_model = material.surface_model,
        .blend_mode = material.blend_mode,
        .value_params = material.value_params,
        .material_function = material.material_function,
        .referenced_material = referenced_material,
    };
    desc.texture_params.resize(referenced_textures.size());
    for (size_t i = 0; auto& [name, tex] : referenced_textures) {
        desc.texture_params[i] = {name, tex};
        ++i;
    }

    serde::Value value;
    MaterialDesc::to_value(value, desc);
    file.write_string_data(value.to_toml());
}

}
