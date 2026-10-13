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

#include "NMBParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
NMBParser::~NMBParser() {
	clear();
}

void NMBParser::clear() {
	// free per-entry allocated data
	for(uint32_t i = 0; i < MAX_MODELS; ++i) {
		free(m.models[i].vertex_data);
		free(m.models[i].indices);
		free(m.models[i].bones);
		free(m.models[i].mesh_groups);
	}
	free(raw_data);
	raw_data = nullptr;
	memset(&m, 0, sizeof(m));
}

int NMBParser::fail(const char *message) {
	last_error = message;
	clear();
	return 0;
}

/*
 */
int NMBParser::parse_entry(const uint8_t *data, size_t file_size, size_t off, ModelEntry *e, size_t *nextOffset) {
	last_error = "Truncated NMB entry at byte " + std::to_string(off);
	if(off + 66 > file_size) return 0;

	e->shader_count		= data[off + 0];
	if(e->shader_count > MAX_SHADERS_PER_MODEL) return 0;
	e->bone_count		= data[off + 1];
	e->mesh_group_count = data[off + 2];
	memcpy(&e->marker_count, data + off + 3, 2);
	e->entry_offset = off;

	// 32-byte name field at off+5
	memset(e->name, 0, sizeof(e->name));
	memcpy(e->name, data + off + 5, 32);
	e->name[31] = '\0';

	// vertex_set_count (1B) at off+37
	e->vertex_set_count = data[off + 37];
	if(e->vertex_set_count == 0 || e->vertex_set_count > NODParser::MAX_VERTEX_SETS) {
		last_error = "Unsupported NMB vertex set count at byte " + std::to_string(off);
		return 0;
	}

	// flags (4B) at off+38
	memcpy(&e->flags, data + off + 38, 4);

	// bounds (24B) at off+42
	memcpy(e->bounds_min, data + off + 42, 12);
	memcpy(e->bounds_max, data + off + 54, 12);

	// vertex sets: INTERLEAVED descriptor + data starting at off+66
	size_t pos			   = off + 66;
	size_t vertexDataStart = 0;
	size_t totalVertBytes  = 0;
	e->total_vertices	   = 0;

	// first pass: collect descriptors and compute layout
	size_t vsDescOffs[NODParser::MAX_VERTEX_SETS];
	size_t vsDataOffs[NODParser::MAX_VERTEX_SETS];
	size_t vsSizes[NODParser::MAX_VERTEX_SETS];

	for(uint8_t i = 0; i < e->vertex_set_count; ++i) {
		if(pos + 5 > file_size) return 0;
		uint8_t vtype = data[pos];
		uint32_t vcount;
		memcpy(&vcount, data + pos + 1, 4);
		if(vtype > 5 || vcount > 500000) {
			last_error = "Unsupported NMB vertex type or count at byte " + std::to_string(pos);
			return 0;
		}

		uint32_t stride = type_to_stride(vtype);
		vsDescOffs[i]	= pos;
		vsDataOffs[i]	= pos + 5;
		vsSizes[i]		= (size_t)vcount * stride;

		e->vertex_sets[i].type		  = vtype;
		e->vertex_sets[i].stride	  = stride;
		e->vertex_sets[i].base_stride = vtype >= 3 ? type_to_stride(vtype - 3) : stride;
		e->vertex_sets[i].count		  = vcount;
		e->vertex_sets[i].data_offset = (uint32_t)totalVertBytes;

		e->total_vertices += vcount;
		totalVertBytes += vsSizes[i];
		pos += 5 + vsSizes[i];

		if(pos > file_size) return 0;
	}

	// copy vertex data into a contiguous buffer
	e->vertex_data_size = totalVertBytes;
	if(totalVertBytes > 0) {
		e->vertex_data = (uint8_t *)malloc(totalVertBytes);
		if(!e->vertex_data) return 0;
		size_t destOff = 0;
		for(uint8_t i = 0; i < e->vertex_set_count; ++i) {
			memcpy(e->vertex_data + destOff, data + vsDataOffs[i], vsSizes[i]);
			destOff += vsSizes[i];
		}
	} else {
		e->vertex_data = nullptr;
	}

	// index count (4B) + index data
	if(pos + 4 > file_size) {
		free(e->vertex_data);
		e->vertex_data = nullptr;
		return 0;
	}
	memcpy(&e->index_count, data + pos, 4);
	if(e->index_count > 2000000) {
		free(e->vertex_data);
		e->vertex_data = nullptr;
		return 0;
	}
	pos += 4;

	if(pos + (size_t)e->index_count * 2 > file_size) {
		free(e->vertex_data);
		e->vertex_data = nullptr;
		return 0;
	}
	if(e->index_count > 0) {
		e->indices = (uint16_t *)malloc(e->index_count * sizeof(uint16_t));
		if(!e->indices) {
			free(e->vertex_data);
			e->vertex_data = nullptr;
			return 0;
		}
		memcpy(e->indices, data + pos, e->index_count * 2);
	} else {
		e->indices = nullptr;
	}
	pos += (size_t)e->index_count * 2;

	// shader names (32B each)
	if(pos + (size_t)e->shader_count * 32 > file_size) goto fail;
	for(uint8_t s = 0; s < e->shader_count && s < MAX_SHADERS_PER_MODEL; ++s) {
		memset(e->shader_names[s], 0, 33);
		memcpy(e->shader_names[s], data + pos + s * 32, 32);
	}
	pos += (size_t)e->shader_count * 32;

	// bones (26 bytes each: pos:12 + offset:12 + parent:1 + joint_id:1)
	if(pos + (size_t)e->bone_count * 26 > file_size) goto fail;
	if(e->bone_count > 0) {
		e->bones = (NODParser::Bone *)calloc(e->bone_count, sizeof(NODParser::Bone));
		if(!e->bones) goto fail;
		for(uint8_t b = 0; b < e->bone_count; ++b) {
			const uint8_t *bp = data + pos + b * 26;
			memcpy(e->bones[b].position, bp, 12);
			memcpy(e->bones[b].offset, bp + 12, 12);
			e->bones[b].parent	 = bp[24];
			e->bones[b].joint_id = bp[25];
			// set identity orientation (NMB doesn't store it)
			memset(e->bones[b].orientation, 0, sizeof(e->bones[b].orientation));
			e->bones[b].orientation[0] = 1.0f; // [0][0]
			e->bones[b].orientation[4] = 1.0f; // [1][1]
			e->bones[b].orientation[8] = 1.0f; // [2][2]
		}
	} else {
		e->bones = nullptr;
	}
	pos += (size_t)e->bone_count * 26;

	// blend count (4B) + blend data (blend_count * 24B)
	if(pos + 4 > file_size) goto fail;
	memcpy(&e->blend_count, data + pos, 4);
	if(e->blend_count > 500000) goto fail;
	pos += 4 + (size_t)e->blend_count * 24;
	if(pos > file_size) goto fail;

	// mesh groups (variable length)
	if(e->mesh_group_count > 0) {
		e->mesh_groups = (NODParser::MeshGroup *)calloc(e->mesh_group_count, sizeof(NODParser::MeshGroup));
		if(!e->mesh_groups) goto fail;

		// Index ranges are shared across vertex sets; vertex bases are per-set.
		uint32_t indexOffset = 0;
		uint32_t baseVerts[NODParser::MAX_VERTEX_SETS]	  = {0};

		for(uint8_t g = 0; g < e->mesh_group_count; ++g) {
			if(pos + 33 > file_size) goto fail;
			NODParser::MeshGroup *mg = &e->mesh_groups[g];

			memcpy(&mg->shader_index, data + pos, 4);
			if(mg->shader_index >= e->shader_count) {
				last_error = "Invalid NMB mesh shader at byte " + std::to_string(pos);
				goto fail;
			}
			pos += 4;

			mg->vertex_set_index = data[pos];
			if(mg->vertex_set_index >= e->vertex_set_count) {
				last_error = "Invalid NMB mesh vertex set at byte " + std::to_string(pos);
				goto fail;
			}
			pos += 1;

			// 4 LODs (6 bytes each: strip_count:2B + list_count:2B + num_verts:2B)
			for(uint32_t lod = 0; lod < NODParser::MAX_LODS; ++lod) {
				uint16_t strip_count, list_count, num_verts;
				memcpy(&strip_count, data + pos, 2);
				memcpy(&list_count, data + pos + 2, 2);
				memcpy(&num_verts, data + pos + 4, 2);
				pos += 6;

				if(strip_count > 0 || list_count > 0 || lod == 0) {
					mg->lods[lod].strip_start = indexOffset;
					mg->lods[lod].strip_count = strip_count;
					mg->lods[lod].list_start  = indexOffset + strip_count;
					mg->lods[lod].list_count  = list_count;
					mg->lods[lod].num_verts	  = num_verts;
					indexOffset += strip_count + list_count;
					if(indexOffset > e->index_count) {
						last_error = "NMB mesh index range exceeds the shared index buffer";
						goto fail;
					}
				} else {
					mg->lods[lod] = mg->lods[0];
				}
			}

			// 3 bytes: originalGroup(1B), numBlendShapes(1B), groupLinkIndex(1B)
			if(pos + 3 > file_size) goto fail;
			// data[pos+0] = m_ucOriginalGroup  (blend shape source group)
			// data[pos+1] = m_ucNumBlendShapes (0 for most models)
			// data[pos+2] = group link index   (pairs mesh groups across vertex sets)
			pos += 3;

			// jointMapSize (1B) = num_bones in this mesh group's palette
			if(pos + 1 > file_size) goto fail;
			uint8_t jointMapSize = data[pos];
			if(jointMapSize > 20) {
				last_error = "Unsupported NMB bone palette at byte " + std::to_string(pos);
				goto fail;
			}
			mg->num_bones		 = jointMapSize;
			pos += 1;
			if(pos + jointMapSize > file_size) goto fail;
			memset(mg->a_bones, 0, sizeof(mg->a_bones));
			if(jointMapSize > 0 && jointMapSize <= 20) {
				memcpy(mg->a_bones, data + pos, jointMapSize);
			}
			pos += jointMapSize;

			// compute base vertex (LOD 0 vertex count advances the base)
			mg->base_vertex		 = baseVerts[mg->vertex_set_index];
			uint16_t baseAdvance = mg->lods[0].num_verts;
			baseVerts[mg->vertex_set_index] += baseAdvance;
		}
	} else {
		e->mesh_groups = nullptr;
	}

	// skip markers (68B each)
	pos += (size_t)e->marker_count * 68;
	if(pos > file_size) goto fail;

	if(m.version == 2) {
		if(pos >= file_size) goto fail;
		const uint8_t hasColors = data[pos++];
		if(hasColors > 1) goto fail;
		if(hasColors) {
			const size_t colorBytes = size_t(e->total_vertices) * 4;
			if(colorBytes > file_size - pos) goto fail;
			e->vertex_colors = data + pos;
			e->color_count = e->total_vertices;
			pos += colorBytes;
		}
	}
	*nextOffset = pos;
	last_error.clear();
	return 1;

fail:
	free(e->vertex_data);
	e->vertex_data = nullptr;
	free(e->indices);
	e->indices = nullptr;
	free(e->bones);
	e->bones = nullptr;
	free(e->mesh_groups);
	e->mesh_groups = nullptr;
	return 0;
}

/*
 */
int NMBParser::parse(const char *filePath) {
	size_t size = 0;
	uint8_t *data = FileUtils::readBinaryFile(filePath, &size);
	return parseOwnedBuffer(data, size);
}

int NMBParser::parseBuffer(const uint8_t *data, size_t size) {
	if(!data || size < 12 || size > 256u * 1024u * 1024u) return fail("Invalid NMB buffer size");
	uint8_t *copy = static_cast<uint8_t *>(malloc(size));
	if(!copy) return fail("Unable to allocate NMB buffer");
	memcpy(copy, data, size);
	return parseOwnedBuffer(copy, size);
}

int NMBParser::parseOwnedBuffer(uint8_t *data, size_t fsize) {
	clear();
	last_error.clear();
	raw_data = data;
	if(!data || fsize < 12) return fail("Invalid or truncated NMB header");
	m.file_size = fsize;

	memcpy(&m.version, raw_data + 0, 4);
	memcpy(&m.model_count, raw_data + 4, 4);
	memcpy(&m.total_mem, raw_data + 8, 4);

	if(m.version != 1 && m.version != 2) {
		return fail("Unsupported NMB version");
	}
	if(m.model_count > MAX_MODELS) return fail("NMB model count exceeds supported limits");

	// empty bundle (count=0) is valid
	if(m.model_count == 0) {
		m.parsed_count = 0;
		printf("[NMB] buffer: version=%u, 0 models (empty)\n", m.version);
		return 1;
	}

	size_t off		 = 12;
	m.parsed_count	 = 0;
	m.total_vertices = 0;
	m.total_indices	 = 0;

	for(uint32_t i = 0; i < m.model_count && i < MAX_MODELS; ++i) {
		if(off >= (size_t)fsize) break;

		ModelEntry *e = &m.models[m.parsed_count];
		memset(e, 0, sizeof(ModelEntry));
		size_t nextOff = 0;

		if(parse_entry(raw_data, (size_t)fsize, off, e, &nextOff)) {
			m.total_vertices += e->total_vertices;
			m.total_indices += e->index_count;
			++m.parsed_count;
			off = nextOff;
		} else {
			// entry parse failed — stop
			break;
		}
	}

	printf("[NMB] buffer: version=%u, %u/%u models parsed, %u verts, %u indices\n", m.version, m.parsed_count,
		   m.model_count, m.total_vertices, m.total_indices);

	if(m.parsed_count != m.model_count) {
		if(last_error.empty()) last_error = "Truncated or unsupported NMB model entry";
		clear();
		return 0;
	}
	return 1;
}

/*
 */
int NMBParser::serialize(const char * /*filePath*/) const {

	return 0;
}
