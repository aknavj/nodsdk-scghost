/*
===========================================================================
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

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

// NODExport nod <in.nod> <out.json> <out.bin>: a model as draw-ready data.
//
// out.bin: every vertex of every vertex set, in set order, 52 bytes each:
// float3 position, float3 normal, float2 uv, uint8[4] bone numbers of the
// model, float[4] weights. Then uint32 triangle indices into those vertices.
// Detail level 0 only; strips are turned into triangles, degenerate ones
// dropped. Values are in the file's own axes. Little-endian.
//
// out.json: bounds, material names, bones (parent, position, 3x3
// orientation, offset), and per mesh group its material, vertex range,
// index range and bone palette.

#include "NODCheck.h"
#include "NODParser.h"
#include "export.h"

#include <cstring>

namespace nodexport {

	namespace {

		constexpr uint32_t STRIDE = 52;

		float f32(const uint8_t *p) {
			float v;
			std::memcpy(&v, p, 4);
			return v;
		}

		struct Vertex {
			float position[3], normal[3], uv[2];
			uint8_t bones[4];
			float weights[4];
		};
		static_assert(sizeof(Vertex) == STRIDE, "vertex layout");

		struct Group {
			uint32_t firstIndex = 0, indexCount = 0, dropped = 0;
		};

		bool finite(const float *v, size_t n) {
			for(size_t i = 0; i < n; ++i)
				if(!std::isfinite(v[i])) return false;
			return true;
		}

		// Reads vertex `local` of group `g` into `out`; false with `error` on a fault.
		bool readVertex(const NODParser::ModelData &m, uint32_t g, uint32_t local, Vertex &out, std::string &error) {
			const auto &group = m.mesh_groups[g];
			const auto &set	  = m.vertex_sets[group.vertex_set_index];
			const uint8_t *p  = m.vertex_data + set.data_offset + size_t(group.base_vertex + local) * set.base_stride;
			for(int a = 0; a < 3; ++a) {
				out.position[a] = f32(p + 4 * a);
				out.normal[a]	= f32(p + 12 + 4 * a);
			}
			out.uv[0] = f32(p + 24);
			out.uv[1] = f32(p + 28);
			std::memset(out.bones, 0, sizeof out.bones);
			out.weights[0] = 1;
			out.weights[1] = out.weights[2] = out.weights[3] = 0;
			const uint32_t kind = set.type % 3;
			uint8_t slots[4]	= {0, 0, 0, 0};
			if(kind == 1) slots[0] = p[32];
			if(kind == 2) {
				out.weights[0] = f32(p + 32);
				out.weights[1] = f32(p + 36);
				out.weights[2] = f32(p + 40);
				out.weights[3] = 1 - out.weights[0] - out.weights[1] - out.weights[2];
				std::memcpy(slots, p + 44, 4);
			}
			if(!finite(out.position, 3) || !finite(out.normal, 3) || !finite(out.uv, 2) || !finite(out.weights, 4)) {
				error = "group " + std::to_string(g) + " vertex " + std::to_string(local) + ": value not finite";
				return false;
			}
			for(int k = 0; k < 4; ++k) {
				if(out.weights[k] == 0) continue;
				if(group.num_bones == 0) break; // a model without bones: positions are final
				if(slots[k] >= group.num_bones) {
					error = "group " + std::to_string(g) + " vertex " + std::to_string(local) + ": palette slot " +
							std::to_string(slots[k]) + " of " + std::to_string(group.num_bones);
					return false;
				}
				out.bones[k] = group.a_bones[slots[k]];
			}
			return true;
		}

		bool degenerate(uint32_t a, uint32_t b, uint32_t c) { return a == b || b == c || a == c; }

		// Appends the detail level 0 triangles of group `g`, as indices into the
		// whole vertex array (`base` is the group's first vertex there).
		bool triangles(const NODParser::ModelData &m, uint32_t g, uint32_t base, std::vector<uint32_t> &out, Group &info,
					   std::string &error) {
			const auto &lod = m.mesh_groups[g].lods[0];
			const uint16_t *s = m.indices + lod.strip_start;
			info.firstIndex	  = uint32_t(out.size());
			for(uint32_t i = 0; i + 2 < lod.strip_count; ++i) {
				uint32_t a = s[i], b = s[i + 1], c = s[i + 2];
				if(degenerate(a, b, c)) {
					++info.dropped;
					continue;
				}
				if(i & 1) std::swap(a, b);
				out.insert(out.end(), {base + a, base + b, base + c});
			}
			if(lod.list_count % 3) {
				error = "group " + std::to_string(g) + ": " + std::to_string(lod.list_count) + " list indices, not whole triangles";
				return false;
			}
			const uint16_t *l = m.indices + lod.list_start;
			for(uint32_t i = 0; i < lod.list_count; i += 3) {
				if(degenerate(l[i], l[i + 1], l[i + 2])) {
					++info.dropped;
					continue;
				}
				out.insert(out.end(), {base + l[i], base + l[i + 1], base + l[i + 2]});
			}
			info.indexCount = uint32_t(out.size()) - info.firstIndex;
			return true;
		}

	} // namespace

	bool exportNod(const char *in, const char *outBin, std::string &json) {
		NODParser parser;
		if(!parser.parse(in)) {
			json = refusal("nod", parser.getLastError());
			return false;
		}
		const auto &m = parser.getModelData();
		if(m.header.flags != 10) {
			json = refusal("nod", "version " + std::to_string(m.header.flags) + ", not 10");
			return false;
		}
		if(m.bytes_read != m.file_size) {
			json = refusal("nod", std::to_string(m.file_size - m.bytes_read) + " bytes the layout does not explain");
			return false;
		}
		const auto faults = nodFaults(m);
		if(!faults.empty()) {
			const auto &f = faults.front();
			json = refusal("nod", (f.group >= 0 ? "group " + std::to_string(f.group) + ": " : "") + f.what);
			return false;
		}

		uint32_t setStart[NODParser::MAX_VERTEX_SETS] = {0};
		uint32_t vertexCount = 0;
		for(uint32_t s = 0; s < m.vertex_set_count; ++s) {
			setStart[s] = vertexCount;
			vertexCount += m.vertex_sets[s].count;
		}
		std::vector<Vertex> vertices(vertexCount);
		std::vector<uint32_t> indices;
		std::vector<Group> groups(m.mesh_group_count);
		std::string error;
		for(uint32_t g = 0; g < m.mesh_group_count; ++g) {
			const auto &group	= m.mesh_groups[g];
			const uint32_t base = setStart[group.vertex_set_index] + group.base_vertex;
			const uint16_t count = nodGroupVertexCount(m, g);
			for(uint32_t v = 0; v < count; ++v)
				if(!readVertex(m, g, v, vertices[base + v], error)) {
					json = refusal("nod", error);
					return false;
				}
			if(!triangles(m, g, base, indices, groups[g], error)) {
				json = refusal("nod", error);
				return false;
			}
		}

		std::string bin(vertices.size() * STRIDE + indices.size() * 4, '\0');
		if(!vertices.empty()) std::memcpy(&bin[0], vertices.data(), vertices.size() * STRIDE);
		if(!indices.empty()) std::memcpy(&bin[vertices.size() * STRIDE], indices.data(), indices.size() * 4);
		if(!survey::writeFile(outBin, bin)) {
			json = refusal("nod", std::string("cannot write ") + outBin);
			return false;
		}

		std::vector<std::string> materials, bones, groupJson;
		for(uint32_t i = 0; i < m.shader_count; ++i) materials.push_back(survey::quote(m.shader_names[i]));
		for(uint32_t b = 0; b < m.bone_count; ++b) {
			const auto &bone = m.bones[b];
			bones.push_back("{\"parent\":" + std::to_string(bone.parent == 0xFF ? -1 : int(bone.parent)) +
							",\"position\":" + floats(bone.position, 3) + ",\"orientation\":" + floats(bone.orientation, 9) +
							",\"offset\":" + floats(bone.offset, 3) + "}");
		}
		for(uint32_t g = 0; g < m.mesh_group_count; ++g) {
			const auto &group = m.mesh_groups[g];
			std::string palette = "[";
			for(uint32_t p = 0; p < group.num_bones; ++p) palette += (p ? "," : "") + std::to_string(group.a_bones[p]);
			groupJson.push_back("{\"material\":" + std::to_string(group.shader_index) +
								",\"vertexType\":" + std::to_string(m.vertex_sets[group.vertex_set_index].type) +
								",\"firstVertex\":" + std::to_string(setStart[group.vertex_set_index] + group.base_vertex) +
								",\"vertexCount\":" + std::to_string(nodGroupVertexCount(m, g)) +
								",\"firstIndex\":" + std::to_string(groups[g].firstIndex) +
								",\"indexCount\":" + std::to_string(groups[g].indexCount) +
								",\"droppedTriangles\":" + std::to_string(groups[g].dropped) + ",\"bones\":" + palette + "]}");
		}
		const float bmin[3] = {m.header.bounds_min.x, m.header.bounds_min.y, m.header.bounds_min.z};
		const float bmax[3] = {m.header.bounds_max.x, m.header.bounds_max.y, m.header.bounds_max.z};
		json = "{\"format\":\"nod\",\"version\":10,\"boundsMin\":" + floats(bmin, 3) + ",\"boundsMax\":" + floats(bmax, 3) +
			   ",\"materials\":" + survey::list(materials) + ",\"bones\":" + survey::list(bones) +
			   ",\"vertexStride\":" + std::to_string(STRIDE) + ",\"vertexCount\":" + std::to_string(vertexCount) +
			   ",\"indexOffset\":" + std::to_string(vertices.size() * STRIDE) + ",\"indexCount\":" + std::to_string(indices.size()) +
			   ",\"groups\":" + survey::list(groupJson) + "}\n";
		return true;
	}

} // namespace nodexport
