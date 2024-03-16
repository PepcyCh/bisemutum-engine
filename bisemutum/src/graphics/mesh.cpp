#include <bisemutum/graphics/mesh.hpp>

namespace bi::gfx {

uint64_t MeshData::curr_id_ = 0;

MeshData::MeshData() : id_(curr_id_++) {
    submeshes_.push_back({.num_indices = ~0u});
}

auto MeshData::mutable_positions() -> std::vector<float3>& {
    set_buffer_dirty();
    set_geometry_dirty();
    return positions_;
}

auto MeshData::mutable_normals() -> std::vector<float3>& {
    set_buffer_dirty();
    return normals_;
}

auto MeshData::mutable_tangents() -> std::vector<float4>& {
    set_buffer_dirty();
    return tangents_;
}

auto MeshData::mutable_colors() -> std::vector<float3>& {
    set_buffer_dirty();
    return colors_;
}

auto MeshData::mutable_texcoords() -> std::vector<float2>& {
    set_buffer_dirty();
    return texcoords_;
}

auto MeshData::mutable_texcoords2() -> std::vector<float2>& {
    set_buffer_dirty();
    return texcoords2_;
}

auto MeshData::mutable_indices() -> std::vector<uint32_t>& {
    set_buffer_dirty();
    set_geometry_dirty();
    return indices_;
}

auto MeshData::set_submehes(std::vector<SubmeshDesc> submeshes) -> void {
    submeshes_ = std::move(submeshes);
    submesh_versions_.resize(submeshes_.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(submeshes_.size()); i++) {
        set_submesh_dirty(i);
    }
}

auto MeshData::set_submesh(uint32_t index, SubmeshDesc const& submeh) -> void {
    if (index >= submeshes_.size()) {
        submeshes_.resize(index + 1);
        submesh_versions_.resize(index + 1);
    }
    submeshes_[index] = submeh;
    set_submesh_dirty(index);
}

auto MeshData::bounding_box() const -> BoundingBox const& {
    if (bbox_.is_empty()) {
        for (auto& pos : positions_) {
            bbox_.add(pos);
        }
    }
    return bbox_;
}

auto MeshData::submesh_bounding_box(uint32_t index) const -> BoundingBox const& {
    if (index >= submeshes_.size()) {
        return BoundingBox::empty;
    }
    if (index >= submesh_bboxes_.size()) {
        submesh_bboxes_.resize(index + 1);
    }
    auto& bbox_ = submesh_bboxes_[index];
    if (bbox_.is_empty()) {
        auto& submesh = submeshes_[index];
        auto submesh_num_indices = submesh.num_indices;
        if (submesh.num_indices == ~0u) {
            submesh_num_indices = indices_.size() - submesh.index_offset;
        }
        if (!indices_.empty()) {
            for (uint32_t i = 0; i < submesh_num_indices; i++) {
                auto index = submesh.base_vertex + indices_[submesh.index_offset + i];
                bbox_.add(positions_[index]);
            }
        } else {
            for (uint32_t i = 0; i < submesh_num_indices; i++) {
                auto index = submesh.base_vertex + i;
                bbox_.add(positions_[index]);
            }
        }
    }
    return bbox_;
}

auto MeshData::set_buffer_dirty() -> void {
    ++buffer_version_;
}
auto MeshData::set_geometry_dirty() -> void {
    ++geometry_version_;
    bbox_.reset();
}
auto MeshData::set_submesh_dirty(uint32_t index) -> void {
    if (index < submesh_bboxes_.size()) {
        submesh_bboxes_[index].reset();
        ++submesh_versions_[index];
    }
}

auto MeshData::get_submesh_version(uint32_t index) const -> uint64_t {
    if (index < submesh_versions_.size()) {
        return submesh_versions_[index];
    } else {
        return 1;
    }
}

auto MeshData::save_to_byte_stream(WriteByteStream& bs) const -> void {
    bs.write(positions_);
    bs.write(normals_);
    bs.write(tangents_);
    bs.write(colors_);
    bs.write(texcoords_);
    bs.write(texcoords2_);
    bs.write(indices_);
    bs.write(submeshes_);
}
auto MeshData::load_from_byte_stream(ReadByteStream& bs) -> void {
    bs.read(positions_);
    bs.read(normals_);
    bs.read(tangents_);
    bs.read(colors_);
    bs.read(texcoords_);
    bs.read(texcoords2_);
    bs.read(indices_);
    bs.read(submeshes_);
}

}
