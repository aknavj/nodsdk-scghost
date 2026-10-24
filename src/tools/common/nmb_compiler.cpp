/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nmb_compiler.cpp
Version: 0.6.7

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "nmb_compiler.h"
#include "tool_common.h"
#include "NMBParser.h"
#include "NODParser.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>

/*
 */
namespace nod::bundle {
namespace {

/*
 */
constexpr size_t maximum_bytes			= 256u * 1024u * 1024u;
constexpr size_t nod_header_bytes		= 92;
constexpr size_t nod_bone_bytes			= 64;
constexpr size_t nod_group_bytes		= 56;
constexpr size_t nod_marker_bytes		= 52;
constexpr uint32_t nod_version			= 10;
constexpr uint32_t bundle_version		= 2;
constexpr uint32_t bundle_model_flags	= 0x00000a00;
constexpr uint32_t runtime_model_bytes	= 328;
constexpr uint32_t runtime_shader_bytes = 4;
constexpr uint32_t runtime_bone_bytes	= 32;
constexpr uint32_t runtime_group_bytes	= 108;
constexpr uint32_t runtime_marker_bytes = 80;

/*
 */
void append_u32(std::vector<uint8_t> &out, uint32_t value) {
	const size_t offset = out.size();
	out.resize(offset + 4);
	put32(out.data() + offset, value);
}

/*
 */
void append(std::vector<uint8_t> &out, const uint8_t *data, size_t size) {
	if(size) {
		out.insert(out.end(), data, data + size);
	}
}

/*
 */
bool finite_values(const uint8_t *data, size_t count) {
	for(size_t i = 0; i < count; ++i) {
		if(!std::isfinite(f32(data + i * 4))) {
			return false;
		}
	}

	return true;
}

/*
 */
bool member_name(const std::string &name, std::string &key) {
	if(name.size() < 5 || name.size() > 32) {
		return false;
	}

	key.clear();
	for(unsigned char c : name) {
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
			 c == '.')) {
			return false;
		}

		key += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
	}

	const auto extension = key.substr(key.size() - 4);
	return extension == ".nod" || extension == ".noc";
}

/*
 */
bool write_model(const Input &input, std::vector<uint8_t> &out, uint64_t &allocation, std::string &error) {
	NODParser parser;
	if(!parser.parseBuffer(input.bytes.data(), input.bytes.size())) {
		error = parser.getLastError();
		return false;
	}

	const auto &model = parser.getModelData();
	const auto *raw	  = input.bytes.data();
	if(model.header.flags != nod_version || model.header.unknown != 0 ||
	   std::any_of(raw + 72, raw + nod_header_bytes, [](uint8_t byte) { return byte != 0; })) {
		error = "Only the verified NOD v10 header layout is supported";
		return false;
	}

	if(!model.vertex_set_count || !finite_values(raw + 12, 6)) {
		error = "Missing vertex sets or non-finite model bounds";
		return false;
	}

	for(size_t axis = 0; axis < 3; ++axis) {
		if(f32(raw + 12 + axis * 4) > f32(raw + 24 + axis * 4)) {
			error = "Inverted model bounds";
			return false;
		}
	}

	for(uint32_t s = 0; s < model.shader_count; ++s) {
		const auto *name = raw + nod_header_bytes + size_t(s) * 32;
		if(!name[0]) {
			error = "Empty shader name";
			return false;
		}

		for(size_t i = 0; i < 32 && name[i]; ++i) {
			if(name[i] < 32 || name[i] > 126) {
				error = "Non-ASCII shader name";
				return false;
			}
		}
	}

	for(uint32_t i = 0; i < model.vertex_set_count; ++i) {
		const auto &set		 = model.vertex_sets[i];
		const auto *vertices = model.vertex_data ? model.vertex_data + set.data_offset : nullptr;
		for(uint32_t v = 0; v < set.count; ++v) {
			const auto *vertex = vertices + size_t(v) * set.base_stride;
			if(!finite_values(vertex, 8) || (set.type % 3 == 2 && !finite_values(vertex + 32, 3))) {
				error = "Non-finite vertex position, normal, UV or weight";
				return false;
			}
		}

		if(set.count && set.type >= 3 &&
		   !finite_values(vertices + size_t(set.count) * set.base_stride, size_t(set.count) * 9)) {
			error = "Non-finite tangent stream";
			return false;
		}
	}

	const size_t bones_offset  = nod_header_bytes + size_t(model.shader_count) * 32;
	const size_t groups_offset = bones_offset + size_t(model.bone_count) * nod_bone_bytes + model.vertex_data_size +
								 size_t(model.index_count) * 2;
	const size_t markers_offset = groups_offset + size_t(model.mesh_group_count) * nod_group_bytes;
	if(markers_offset + 4 > input.bytes.size() || model.marker_count > UINT16_MAX) {
		error = "Missing marker count or too many markers for NMB";
		return false;
	}

	const size_t tail = markers_offset + 4 + size_t(model.marker_count) * nod_marker_bytes;
	if(input.bytes.size() - tail < 4 || u32(raw + tail) != 0) {
		error = "Unsupported NOD post-marker extension (expected the verified zero reserved word)";
		return false;
	}

	uint32_t total_vertices = 0;
	for(uint32_t s = 0; s < model.vertex_set_count; ++s) {
		total_vertices += model.vertex_sets[s].count;
	}

	size_t colors_offset = tail + 4;
	bool has_colors		 = false;
	if(colors_offset != input.bytes.size()) {
		if(input.bytes.size() - colors_offset < 4 || u32(raw + colors_offset) != total_vertices ||
		   input.bytes.size() - colors_offset - 4 != size_t(total_vertices) * 4) {
			error = "Unsupported or truncated NOD vertex-colour extension";
			return false;
		}

		colors_offset += 4;
		has_colors = true;
	}

	for(uint32_t b = 0; b < model.bone_count; ++b) {
		const auto *bone = raw + bones_offset + size_t(b) * nod_bone_bytes;
		if(!finite_values(bone, 15) || (bone[62] != 255 && bone[62] >= model.bone_count)) {
			error = "Invalid bone transform or parent";
			return false;
		}
	}

	for(uint32_t b = 0; b < model.bone_count; ++b) {
		uint32_t parent = b;
		for(uint32_t depth = 0; parent != 255; ++depth) {
			if(depth >= model.bone_count) {
				error = "Cyclic bone hierarchy";
				return false;
			}

			parent = model.bones[parent].parent;
		}
	}

	for(uint32_t g = 0; g < model.mesh_group_count; ++g) {
		const auto &group  = model.mesh_groups[g];
		const auto *record = raw + groups_offset + size_t(g) * nod_group_bytes;
		const auto &set	   = model.vertex_sets[group.vertex_set_index];
		if(record[31] || record[55]) {
			error = "Unsupported blend-shape or reserved mesh-group extension";
			return false;
		}

		if(group.shader_index >= model.shader_count || u16(record + 28) != group.lods[0].num_verts ||
		   group.base_vertex > set.count) {
			error = "Invalid shader, unrepresentable vertex advance or group vertex base";
			return false;
		}

		for(uint8_t b = 0; b < group.num_bones; ++b) {
			if(group.a_bones[b] >= model.bone_count) {
				error = "Bone palette index outside skeleton";
				return false;
			}
		}

		for(size_t lod = 0; lod < NODParser::MAX_LODS; ++lod) {
			const auto &level = group.lods[lod];
			if(level.list_count % 3 || level.num_verts > set.count - group.base_vertex) {
				error = "Invalid LOD triangle count or vertex range";
				return false;
			}

			for(size_t list = 0; list < 2; ++list) {
				const uint32_t start = list ? level.list_start : level.strip_start;
				const uint32_t count = list ? level.list_count : level.strip_count;
				if(start > model.index_count || count > model.index_count - start) {
					error = "LOD index range outside buffer";
					return false;
				}

				for(uint32_t i = 0; i < count; ++i) {
					const uint16_t index = model.indices[start + i];
					if(!list && index == UINT16_MAX) {
						continue;
					}

					if(index >= level.num_verts) {
						error = "LOD index outside local vertex range";
						return false;
					}
				}
			}
		}
	}

	for(uint32_t m = 0; m < model.marker_count; ++m) {
		if(!finite_values(raw + markers_offset + 4 + size_t(m) * nod_marker_bytes + 4, 12)) {
			error = "Non-finite marker matrix";
			return false;
		}
	}

	out.push_back(static_cast<uint8_t>(model.shader_count));
	out.push_back(static_cast<uint8_t>(model.bone_count));
	out.push_back(static_cast<uint8_t>(model.mesh_group_count));
	out.push_back(static_cast<uint8_t>(model.marker_count));
	out.push_back(static_cast<uint8_t>(model.marker_count >> 8));
	const size_t name_offset = out.size();
	out.resize(name_offset + 32, 0);
	std::copy(input.name.begin(), input.name.end(), out.begin() + name_offset);
	out.push_back(static_cast<uint8_t>(model.vertex_set_count));
	append_u32(out, bundle_model_flags);
	append(out, raw + 12, 24);
	for(uint32_t s = 0; s < model.vertex_set_count; ++s) {
		const auto &set = model.vertex_sets[s];
		out.push_back(static_cast<uint8_t>(set.type));
		append_u32(out, set.count);
		append(out, model.vertex_data ? model.vertex_data + set.data_offset : nullptr, size_t(set.count) * set.stride);
	}

	append_u32(out, model.index_count);
	append(out, raw + groups_offset - size_t(model.index_count) * 2, size_t(model.index_count) * 2);
	append(out, raw + nod_header_bytes, size_t(model.shader_count) * 32);
	for(uint32_t b = 0; b < model.bone_count; ++b) {
		const auto *bone = raw + bones_offset + size_t(b) * nod_bone_bytes;
		append(out, bone, 12);
		append(out, bone + 48, 12);
		append(out, bone + 62, 2);
	}

	append_u32(out, 0);
	for(uint32_t g = 0; g < model.mesh_group_count; ++g) {
		const auto *record = raw + groups_offset + size_t(g) * nod_group_bytes;
		append(out, record, 4);
		out.push_back(record[54]);
		append(out, record + 4, 24);
		append(out, record + 30, 3);
		out.push_back(record[53]);
		append(out, record + 33, record[53]);
	}

	for(uint32_t m = 0; m < model.marker_count; ++m) {
		const auto *marker = raw + markers_offset + 4 + size_t(m) * nod_marker_bytes;
		append(out, marker, 4);
		for(size_t row = 0; row < 4; ++row) {
			append(out, marker + 4 + row * 12, 12);
			append_u32(out, row == 3 ? 0x3f800000 : 0);
		}
	}

	out.push_back(has_colors ? 1 : 0);
	if(has_colors) {
		append(out, raw + colors_offset, size_t(total_vertices) * 4);
	}

	// Native arena excludes GPU buffers, names, palettes and vertex-set descriptors.
	allocation += runtime_model_bytes + uint64_t(model.shader_count) * runtime_shader_bytes +
				  uint64_t(model.bone_count) * runtime_bone_bytes +
				  uint64_t(model.mesh_group_count) * runtime_group_bytes +
				  uint64_t(model.marker_count) * runtime_marker_bytes;
	return true;
}

} // namespace

/*
 */
bool compile(const std::vector<Input> &inputs, std::vector<uint8_t> &output, std::string &error) {
	output.clear();
	error.clear();
	if(inputs.size() > NMBParser::MAX_MODELS) {
		error = "NMB supports at most 256 models";
		return false;
	}

	std::set<std::string> names;
	size_t source_bytes = 0;
	for(const auto &input : inputs) {
		std::string key;
		if(!member_name(input.name, key)) {
			error = "Invalid member name: " + input.name +
					" (use 1-28 ASCII filename characters plus .nod or .noc; no paths)";
			return false;
		}

		if(!names.insert(key).second) {
			error = "Duplicate case-insensitive member name: " + input.name;
			return false;
		}

		if(input.bytes.size() > maximum_bytes - source_bytes) {
			error = "NOD inputs exceed 256 MiB";
			return false;
		}

		source_bytes += input.bytes.size();
	}

	std::vector<uint8_t> staged;
	append_u32(staged, bundle_version);
	append_u32(staged, static_cast<uint32_t>(inputs.size()));
	append_u32(staged, 0);
	uint64_t allocation = 0;
	for(const auto &input : inputs) {
		if(!write_model(input, staged, allocation, error)) {
			error = input.name + ": " + error;
			return false;
		}

		if(staged.size() > maximum_bytes || allocation > UINT32_MAX) {
			error = "Generated NMB exceeds supported size or runtime allocation limits";
			return false;
		}
	}

	put32(staged.data() + 8, static_cast<uint32_t>(allocation));
	auto check = std::make_unique<NMBParser>();
	if(!check->parseBuffer(staged.data(), staged.size())) {
		error = "Generated NMB failed the native parser: " + check->getLastError();
		return false;
	}

	output.swap(staged);
	return true;
}

} // namespace nod::bundle
