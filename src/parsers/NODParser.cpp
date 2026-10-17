/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
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

#include "NODParser.h"
#include <algorithm>
#include <cmath>
#include <limits>

/*
 */
namespace {

	// total bytes per vertex in the file (both streams combined).
	// types 3-5 are stored as TWO SEQUENTIAL STREAMS:
	// stream 0: base vertex data (same layout as type 0/1/2)
	// stream 1: TBN data (tangent+binormal+normal, 36 bytes each)
	// total = base_stride + 36.
	uint32_t type_to_stride(uint32_t type) {

		switch(type) {
			case 0:
				return 32; // pos(12)+norm(12)+uv(8)
			case 1:
				return 36; // pos(12)+norm(12)+uv(8)+boneIdx(4)
			case 2:
				return 48; // pos(12)+norm(12)+uv(8)+weights(12)+boneIdx(4)
			case 3:
				return 68; // stream0: type0(32) + stream1: TBN(36)
			case 4:
				return 72; // stream0: type1(36) + stream1: TBN(36)
			case 5:
				return 84; // stream0: type2(48) + stream1: TBN(36)
			default:
				printf("[NOD] Warning: unknown vertex type %u, assuming stride 32\n", type);
				return 32;
		}
	}

	/*
	*/
	// base vertex stride (stream 0 only) for reading pos/norm/uv/weights/bones.
	// types 3-5 map to their non-bump counterparts (0/1/2).
	uint32_t base_stride(uint32_t type) {

		switch(type) {
			case 3:
				return 32; // same layout as type 0
			case 4:
				return 36; // same layout as type 1
			case 5:
				return 48; // same layout as type 2
			default:
				return type_to_stride(type); // types 0-2 have no TBN stream
		}
	}

	/*
	*/
	// base vertex type (strip TBN flag). Types 3->0, 4->1, 5->2.
	uint32_t base_type(uint32_t type) {

		return (type >= 3 && type <= 5) ? type - 3 : type;
	}

} // namespace

/*
 */
NODParser::~NODParser() {
	clear();
}

void NODParser::clear() {
	free(m.bones);
	free(m.vertex_data);
	free(m.indices);
	free(m.mesh_groups);
	free(m.raw_data);
	memset(&m, 0, sizeof(m));
}

int NODParser::fail(const char *message) {
	last_error = message;
	clear();
	return 0;
}

/*
 */
int NODParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	return parseOwnedBuffer(data, file_size);
}

int NODParser::parseBuffer(const uint8_t *data, size_t size) {
	if(!data || size < 0x5C || size > 256u * 1024u * 1024u) {
		return fail("Invalid NOD buffer size");
	}
	uint8_t *copy = static_cast<uint8_t *>(malloc(size));
	if(!copy) return fail("Unable to allocate NOD buffer");
	memcpy(copy, data, size);
	return parseOwnedBuffer(copy, size);
}

int NODParser::parseOwnedBuffer(uint8_t *data, size_t file_size) {
	clear();
	last_error.clear();
	m.raw_data = data;
	m.raw_data_size = file_size;
	if(!data || file_size < 0x5C) {
		return fail("Invalid or truncated NOD header");
	}

	m.file_size = file_size;

	// file header (92 bytes = 0x5C)
	m.header.flags			  = FileUtils::readUInt32(data, 0x00);
	m.header.shader_count	  = data[0x04];
	m.header.bone_count		  = data[0x05];
	m.header.vertex_set_count = data[0x06];
	m.header.mesh_group_count = data[0x07];
	m.header.unknown		  = FileUtils::readUInt32(data, 0x08);
	m.header.bounds_min.x	  = FileUtils::readFloat(data, 0x0C);
	m.header.bounds_min.y	  = FileUtils::readFloat(data, 0x10);
	m.header.bounds_min.z	  = FileUtils::readFloat(data, 0x14);
	m.header.bounds_max.x	  = FileUtils::readFloat(data, 0x18);
	m.header.bounds_max.y	  = FileUtils::readFloat(data, 0x1C);
	m.header.bounds_max.z	  = FileUtils::readFloat(data, 0x20);
	for(uint32_t i = 0; i < MAX_VERTEX_SETS; ++i) {
		m.header.vertex_sets[i].type  = FileUtils::readUInt32(data, 0x24 + i * 8);
		m.header.vertex_sets[i].count = FileUtils::readUInt32(data, 0x28 + i * 8);
	}
	m.header.index_count = FileUtils::readUInt32(data, 0x44);
	if(m.header.shader_count > MAX_SHADERS || m.header.vertex_set_count > MAX_VERTEX_SETS ||
	   m.header.index_count > 2000000) {
		return fail("NOD header counts exceed supported limits");
	}

	size_t off = 0x5C;

	// shader names
	m.shader_count = m.header.shader_count;
	for(uint32_t i = 0; i < m.shader_count && i < MAX_SHADERS; ++i) {
		if(off + 32 > file_size) return fail("Truncated NOD shader names");
		memset(m.shader_names[i], 0, 33);
		memcpy(m.shader_names[i], data + off, 32);
		off += 32;
	}

	// bones (64 bytes each)
	m.bone_count = m.header.bone_count;
	if(m.bone_count > 0) {
		m.bones = (Bone *)calloc(m.bone_count, sizeof(Bone));
		if(!m.bones) return fail("Unable to allocate NOD bones");
		for(uint32_t i = 0; i < m.bone_count; ++i) {
			if(off + 64 > file_size) return fail("Truncated NOD bones");
			const uint8_t *b = data + off;
			// position at +0x00 (12B)
			memcpy(m.bones[i].position, b + 0, 12);
			// orientation 3x3 at +0x0C (36B)
			memcpy(m.bones[i].orientation, b + 12, 36);
			// offset at +0x30 (12B) — invRootTranslate = -accumulated position
			memcpy(m.bones[i].offset, b + 48, 12);
			// bytes at +0x3C..+0x3D: secondary link, joint tag (unused)
			m.bones[i].pad0 = b[60];
			m.bones[i].pad1 = b[61];
			// parent at +0x3E, joint_id at +0x3F
			m.bones[i].parent	= b[62];
			m.bones[i].joint_id = b[63];
			off += 64;
		}
	}

	// vertex data
	m.vertex_set_count	  = m.header.vertex_set_count;
	size_t totalVertBytes = 0;
	for(uint32_t i = 0; i < m.vertex_set_count && i < MAX_VERTEX_SETS; ++i) {
		uint32_t type				 = m.header.vertex_sets[i].type;
		uint32_t count				 = m.header.vertex_sets[i].count;
		if(type > 5 || count > 500000) return fail("Unsupported NOD vertex type or count");
		uint32_t stride				 = type_to_stride(type);
		uint32_t bstride			 = base_stride(type);
		m.vertex_sets[i].type		 = type;
		m.vertex_sets[i].stride		 = stride;
		m.vertex_sets[i].base_stride = bstride;
		m.vertex_sets[i].count		 = count;
		m.vertex_sets[i].data_offset = (uint32_t)totalVertBytes;
		totalVertBytes += size_t(count) * stride;
	}

	if(off + totalVertBytes > file_size) {
		return fail("Truncated NOD vertex data");
	}

	m.vertex_data_size = totalVertBytes;
	if(totalVertBytes) {
		m.vertex_data = (uint8_t *)malloc(totalVertBytes);
		if(!m.vertex_data) return fail("Unable to allocate NOD vertices");
		memcpy(m.vertex_data, data + off, totalVertBytes);
	}
	off += totalVertBytes;

	// index buffer (flat uint16 array)
	m.index_count = m.header.index_count;
	if(size_t(m.index_count) * 2 > file_size - off) return fail("Truncated NOD indices");
	if(m.index_count > 0 && off + m.index_count * 2 <= file_size) {
		m.indices = (uint16_t *)malloc(m.index_count * sizeof(uint16_t));
		if(!m.indices) return fail("Unable to allocate NOD indices");
		memcpy(m.indices, data + off, m.index_count * sizeof(uint16_t));
		off += m.index_count * 2;
	}

	// mesh groups (56 bytes each)
	m.mesh_group_count = m.header.mesh_group_count;
	if(size_t(m.mesh_group_count) * 56 > file_size - off) return fail("Truncated NOD mesh groups");
	if(m.mesh_group_count > 0 && off + m.mesh_group_count * 56 <= file_size) {
		m.mesh_groups = (MeshGroup *)calloc(m.mesh_group_count, sizeof(MeshGroup));
		if(!m.mesh_groups) return fail("Unable to allocate NOD mesh groups");

		// track running index offset and per-set base vertex
		uint32_t curIdxOff					= 0;
		uint32_t baseVerts[MAX_VERTEX_SETS] = {0};

		for(uint32_t i = 0; i < m.mesh_group_count; ++i) {
			const uint8_t *buf = data + off + i * 56;
			MeshGroup *mg	   = &m.mesh_groups[i];

			mg->shader_index = FileUtils::readUInt32(buf, 0);

			// parse 4 LODs (6 bytes each: strip_count, list_count, num_verts)
			for(uint32_t lod = 0; lod < MAX_LODS; ++lod) {
				uint16_t sc = FileUtils::readUInt16(buf, 4 + lod * 6);
				uint16_t lc = FileUtils::readUInt16(buf, 6 + lod * 6);
				uint16_t nv = FileUtils::readUInt16(buf, 8 + lod * 6);

				if(sc > 0 || lc > 0 || lod == 0) {
					mg->lods[lod].strip_start = curIdxOff;
					mg->lods[lod].strip_count = sc;
					mg->lods[lod].list_start  = curIdxOff + sc;
					mg->lods[lod].list_count  = lc;
					mg->lods[lod].num_verts	  = nv;
					curIdxOff += sc + lc;
				} else {
					// copy LOD 0 when empty
					mg->lods[lod] = mg->lods[0];
				}
			}

			// base vertex advance at offset 0x1C (uint16)
			uint16_t baseVertAdvance = FileUtils::readUInt16(buf, 0x1C);

			// vertex_set_index at byte 0x36, num_bones at byte 0x35
			mg->num_bones		 = buf[0x35];
			mg->vertex_set_index = buf[0x36];

			// bone palette at offset 0x21 (up to 20 entries)
			memcpy(mg->a_bones, buf + 0x21, 20);

			// clamp to valid range
			if(mg->vertex_set_index >= m.vertex_set_count || mg->num_bones > 20 ||
			   curIdxOff > m.index_count) {
				return fail("Invalid NOD mesh group vertex set, palette or index range");
			}

			mg->base_vertex = baseVerts[mg->vertex_set_index];
			baseVerts[mg->vertex_set_index] += baseVertAdvance;
		}
		off += m.mesh_group_count * 56;
	}

	// markers (count + 52 bytes each)
	if(off + 4 <= file_size) {
		m.marker_count = FileUtils::readUInt32(data, off);
		off += 4;
		if(size_t(m.marker_count) > (file_size - off) / 52) return fail("Truncated NOD markers");
		off += size_t(m.marker_count) * 52;
	} else if(off != file_size) {
		return fail("Truncated NOD marker count");
	}

	// print summary
	printf("[NOD] buffer: flags=0x%X, %u shaders, %u bones, %u vertex sets", m.header.flags, m.shader_count,
		   m.bone_count, m.vertex_set_count);
	for(uint32_t i = 0; i < m.vertex_set_count; ++i)
		printf(" [S%u: type%u %u verts stride %u]", i, m.vertex_sets[i].type, m.vertex_sets[i].count,
			   m.vertex_sets[i].stride);
	printf(", %u indices, %u groups, %u markers\n", m.index_count, m.mesh_group_count, m.marker_count);

	// keep raw for serialization
	m.raw_data		= data;
	m.raw_data_size = file_size;
	return 1;
}

/*
 */
bool NODParser::computeBounds(float minimum[3], float maximum[3], std::string &error) const {
	error.clear();
	float lower[3] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
					  std::numeric_limits<float>::max()};
	float upper[3] = {-lower[0], -lower[1], -lower[2]};
	bool found = false;
	for(uint32_t g = 0; g < m.mesh_group_count; ++g) {
		const auto &group = m.mesh_groups[g];
		const auto &lod = group.lods[0];
		if(group.vertex_set_index >= m.vertex_set_count || lod.list_count % 3 ||
		   lod.list_start > m.index_count || lod.list_count > m.index_count - lod.list_start ||
		   lod.strip_start > m.index_count || lod.strip_count > m.index_count - lod.strip_start) {
			error = "Invalid NOD LOD-0 group while computing bounds";
			return false;
		}
		const auto &set = m.vertex_sets[group.vertex_set_index];
		auto triangle = [&](uint16_t a, uint16_t b, uint16_t c) {
			if(a == b || b == c || a == c) return true;
			for(uint16_t index : {a, b, c}) {
				if(group.base_vertex > set.count || index >= set.count - group.base_vertex) {
					error = "NOD bounds index outside vertex set";
					return false;
				}
				const size_t offset = set.data_offset + size_t(group.base_vertex + index) * set.base_stride;
				if(offset > m.vertex_data_size || m.vertex_data_size - offset < 3 * sizeof(float)) {
					error = "NOD bounds vertex outside buffer";
					return false;
				}
				for(size_t axis = 0; axis < 3; ++axis) {
					const float value = FileUtils::readFloat(m.vertex_data, offset + axis * sizeof(float));
					if(!std::isfinite(value)) {
						error = "Non-finite NOD bounds vertex";
						return false;
					}
					lower[axis] = std::min(lower[axis], value);
					upper[axis] = std::max(upper[axis], value);
				}
				found = true;
			}
			return true;
		};
		for(uint32_t i = 0; i < lod.list_count; i += 3) {
			const uint16_t *indices = m.indices + lod.list_start + i;
			if(!triangle(indices[0], indices[1], indices[2])) return false;
		}
		uint16_t previous[2] = {};
		uint32_t seen = 0;
		for(uint32_t i = 0; i < lod.strip_count; ++i) {
			const uint16_t index = m.indices[lod.strip_start + i];
			if(index == UINT16_MAX) {
				seen = 0;
			} else if(seen < 2) {
				previous[seen++] = index;
			} else {
				if(!triangle(previous[0], previous[1], index)) return false;
				previous[0] = previous[1];
				previous[1] = index;
			}
		}
	}
	if(!found) {
		error = "NOD has no LOD-0 triangles for bounds";
		return false;
	}
	std::copy(lower, lower + 3, minimum);
	std::copy(upper, upper + 3, maximum);
	return true;
}

int NODParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, m.raw_data, m.raw_data_size);
}
