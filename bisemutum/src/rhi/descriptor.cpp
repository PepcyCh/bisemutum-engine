#include <bisemutum/rhi/descriptor.hpp>

#include <bisemutum/prelude/hash.hpp>

auto std::hash<bi::rhi::DescriptorHandle>::operator()(bi::rhi::DescriptorHandle const& v) const noexcept -> size_t {
    return bi::hash(v.cpu, v.gpu);
}
