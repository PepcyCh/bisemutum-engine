#include <bisemutum/rhi/pipeline.hpp>

#include <bisemutum/prelude/hash.hpp>

auto std::hash<bi::rhi::BindGroupLayoutEntry>::operator()(
    bi::rhi::BindGroupLayoutEntry const& v
) const noexcept -> size_t {
    return bi::hash(v.space, v.binding_or_register, v.count, v.type, v.visibility);
}

auto std::hash<bi::rhi::BindGroupLayout>::operator()(
    bi::rhi::BindGroupLayout const& v
) const noexcept -> size_t {
    return bi::hash_linear_container(v);
}

auto std::hash<bi::rhi::StaticSampler>::operator()(
    bi::rhi::StaticSampler const& v
) const noexcept -> size_t {
    return bi::hash(bi::hash_by_byte(v.sampler->desc()), v.space, v.binding_or_register, v.visibility);
}

auto std::hash<bi::rhi::PushConstantsDesc>::operator()(
    bi::rhi::PushConstantsDesc const& v
) const noexcept -> size_t {
    return bi::hash(v.space, v.register_, v.visibility, v.size);
}
