#include <bisemutum/graphics/mesh.hpp>

namespace bi::gfx {

uint64_t MeshData::curr_id_ = 0;

MeshData::MeshData() : id_(curr_id_++) {
    submeshes_.push_back({.num_indices = ~0u});
}

auto MeshData::mutable_positions() -> std::vector<float3>& {
    data_dirty();
    return positions_;
}

auto MeshData::mutable_normals() -> std::vector<float3>& {
    data_dirty();
    return normals_;
}

auto MeshData::mutable_tangents() -> std::vector<float4>& {
    data_dirty();
    return tangents_;
}

auto MeshData::mutable_colors() -> std::vector<float3>& {
    data_dirty();
    return colors_;
}

auto MeshData::mutable_texcoords() -> std::vector<float2>& {
    data_dirty();
    return texcoords_;
}

auto MeshData::mutable_texcoords2() -> std::vector<float2>& {
    data_dirty();
    return texcoords2_;
}

auto MeshData::mutable_indices() -> std::vector<uint32_t>& {
    data_dirty();
    return indices_;
}

auto MeshData::set_submehes(std::vector<SubmeshDesc> submehes) -> void {
    submehes = std::move(submehes);
    for (uint32_t i = 0; i < static_cast<uint32_t>(submehes.size()); i++) {
        submesh_dirty(i);
    }
}

auto MeshData::set_submesh(uint32_t index, SubmeshDesc const& submeh) -> void {
    if (index >= submeshes_.size()) {
        submeshes_.resize(index + 1);
    }
    submeshes_[index] = submeh;
    submesh_dirty(index);
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
    if (index >= submeh_bboxes_.size()) {
        submeh_bboxes_.resize(index + 1);
    }
    auto& bbox_ = submeh_bboxes_[index];
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

auto MeshData::data_dirty() -> void {
    ++version_;
    bbox_.reset();
}

auto MeshData::submesh_dirty(uint32_t index) -> void {
    if (index < submeh_bboxes_.size()) {
        submeh_bboxes_[index].reset();
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
