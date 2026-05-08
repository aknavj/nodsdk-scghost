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

#include "NILParser.h"
#include <cstdio>

/*
 */
namespace {

	/*
	*/
	// helper: read uint16 little-endian
	uint16_t RU16(const uint8_t *d, size_t o) {
		return (uint16_t)(d[o] | (d[o + 1] << 8));
	}

	/*
	*/
	// helper: read uint32 little-endian
	uint32_t RU32(const uint8_t *d, size_t o) {
		return FileUtils::readUInt32(d, o);
	}

	/*
	*/
	float RF32(const uint8_t *d, size_t o) {
		return FileUtils::readFloat(d, o);
	}

	/*
	*/
	// helper: read 32-byte null-padded name
	void ReadName32(const uint8_t *d, size_t o, char *out) {
		memcpy(out, d + o, 32);
		out[32] = '\0';
	}

} // namespace

/*
 */
NILParser::~NILParser() {

	free_sectors();
	free(lvl.raw_data);
}

/*
 */
void NILParser::free_sectors() {

	if(lvl.sectors) {
		for(uint32_t i = 0; i < lvl.num_sectors; i++) {
			Sector &s = lvl.sectors[i];
			free(s.vertex_data);
			free(s.tbn_data);
			free(s.groups);
			free(s.indices);
			free(s.things);
			free(s.surf_things);
		}
		free(lvl.sectors);
		lvl.sectors = nullptr;
	}
}

/*
 */
int NILParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 96) {
		free(data);
		return 0;
	}

	if(data[0] != 'N' || data[1] != 'I' || data[2] != 'L') {
		free(data);
		return 0;
	}

	lvl.file_size	  = file_size;
	lvl.raw_data	  = data;
	lvl.raw_data_size = file_size;

	size_t off = 0;

	// header: magic(4) + version(4)
	lvl.magic = RU32(data, off);
	off += 4;
	lvl.version = RU32(data, off);
	off += 4;

	memcpy(lvl.global_light, data + off, 72);
	off += 72;

	// ambient_color(12) - 3 floats
	lvl.ambient_color[0] = RF32(data, off);
	off += 4;
	lvl.ambient_color[1] = RF32(data, off);
	off += 4;
	lvl.ambient_color[2] = RF32(data, off);
	off += 4;

	// shader names: count(4) + N*32
	lvl.num_shaders = RU32(data, off);
	off += 4;
	if(lvl.num_shaders > MAX_SHADERS) {
		printf("NILParser: too many shaders (%u)\n", lvl.num_shaders);
		return 0;
	}
	for(uint32_t i = 0; i < lvl.num_shaders; i++) {
		ReadName32(data, off, lvl.shader_names[i]);
		off += 32;
	}

	// envmap names: count(2) + N*32
	lvl.num_env_maps = RU16(data, off);
	off += 2;
	for(uint16_t i = 0; i < lvl.num_env_maps; i++) {
		ReadName32(data, off, lvl.env_map_names[i]);
		off += 32;
	}

	lvl.num_sectors = RU32(data, off);
	off += 4;
	if(lvl.num_sectors > MAX_SECTORS) {
		printf("NILParser: too many sectors (%u)\n", lvl.num_sectors);
		return 0;
	}

	lvl.sectors		  = (Sector *)calloc(lvl.num_sectors, sizeof(Sector));
	lvl.total_verts	  = 0;
	lvl.total_indices = 0;
	lvl.total_groups  = 0;

	for(uint32_t si = 0; si < lvl.num_sectors; si++) {
		Sector &s = lvl.sectors[si];

		if(off + 17 > file_size) {
			printf("NILParser: S%02u truncated at header (off=0x%zx)\n", si, off);
			return 0;
		}

		s.flags = RU32(data, off);
		off += 4;
		s.center[0] = RF32(data, off);
		off += 4;
		s.center[1] = RF32(data, off);
		off += 4;
		s.center[2] = RF32(data, off);
		off += 4;

		// portals: count(1) + N*16
		s.num_portals = data[off];
		off += 1;
		if(s.num_portals > 0) {
			size_t psz = (size_t)s.num_portals * 16;
			memcpy(s.portal_data, data + off, psz);
			off += psz;
		}

		s.has_geo = data[off];
		off += 1;

		s.vert_type = 0;
		if(s.has_geo) {
			// vert_type byte (1) - only present when has_geo>=1
			s.vert_type = data[off];
			off += 1;
		}

		// num_verts(2) + pad(2) - always present
		s.num_verts = RU16(data, off);
		off += 2;
		off += 2; // padding

		if(s.num_verts > 0) {
			// vertex data: nv * 36
			size_t vsz = (size_t)s.num_verts * 36;
			if(off + vsz > file_size) {
				printf("NILParser: S%02u vertex overflow: nv=%u, off=0x%zx\n", si, s.num_verts, off);
				return 0;
			}
			s.vertex_data = (uint8_t *)malloc(vsz);
			memcpy(s.vertex_data, data + off, vsz);
			off += vsz;

			// TBN data: nv * 36 (tangent/binormal/normal matrices)
			if(s.vert_type == 1) {
				if(off + vsz > file_size) {
					printf("NILParser: S%02u TBN overflow: nv=%u, off=0x%zx\n", si, s.num_verts, off);
					return 0;
				}
				s.tbn_data = (uint8_t *)malloc(vsz);
				memcpy(s.tbn_data, data + off, vsz);
				off += vsz;
			}
		}

		// lightmap patches (has_geo>=2 only)
		s.lm_flag		 = 0;
		s.lm_patch_count = 0;
		if(s.has_geo >= 2) {
			s.lm_flag = data[off];
			off += 1;
			s.lm_patch_count = RU32(data, off);
			off += 4;
			off += (size_t)s.lm_patch_count * 72; // skip patch data
		}

		// num_groups(2) - always present
		s.num_groups = RU16(data, off);
		off += 2;

		// surface groups: fixed 47 bytes each
		if(s.num_groups > 0) {
			s.groups = (SurfaceGroup *)calloc(s.num_groups, sizeof(SurfaceGroup));
			for(uint16_t gi = 0; gi < s.num_groups; gi++) {
				SurfaceGroup &g = s.groups[gi];
				g.shader_idx	= RU32(data, off);
				off += 4;
				g.start_idx = RU32(data, off);
				off += 4;
				g.field8 = RU32(data, off);
				off += 4;
				g.num_strip_indices = RU16(data, off);
				off += 2;
				g.num_list_indices = RU16(data, off);
				off += 2;
				g.field16 = RU32(data, off);
				off += 4;
				g.vert_type = data[off];
				off += 1;
				g.flag = data[off];
				off += 1;
				g.field22 = data[off];
				off += 1;
				// bbox: min(12) + max(12)
				g.bbox_min[0] = RF32(data, off);
				off += 4;
				g.bbox_min[1] = RF32(data, off);
				off += 4;
				g.bbox_min[2] = RF32(data, off);
				off += 4;
				g.bbox_max[0] = RF32(data, off);
				off += 4;
				g.bbox_max[1] = RF32(data, off);
				off += 4;
				g.bbox_max[2] = RF32(data, off);
				off += 4;
			}
		}

		if(s.has_geo) {
			// indices: count(4) + N*2 - only when has_geo>=1
			s.num_indices = RU32(data, off);
			off += 4;
			if(s.num_indices > 0) {
				if(off + (size_t)s.num_indices * 2 > file_size) {
					printf("NILParser: S%02u index overflow: ni=%u, off=0x%zx\n", si, s.num_indices, off);
					return 0;
				}
				s.indices = (uint16_t *)malloc(s.num_indices * sizeof(uint16_t));
				for(uint32_t ii = 0; ii < s.num_indices; ii++) {
					s.indices[ii] = RU16(data, off);
					off += 2;
				}
			}
		} else {
			// no geometry: no index count field
			s.num_indices = 0;
		}

		// things: count(1) + N*68
		s.num_things = data[off];
		off += 1;
		if(s.num_things > 0) {
			s.things = (Thing *)malloc(s.num_things * sizeof(Thing));
			for(uint8_t ti = 0; ti < s.num_things; ti++) {
				Thing &t	  = s.things[ti];
				t.position[0] = RF32(data, off);
				off += 4;
				t.position[1] = RF32(data, off);
				off += 4;
				t.position[2] = RF32(data, off);
				off += 4;
				t.field1 = RU32(data, off);
				off += 4;
				t.field2 = RU32(data, off);
				off += 4;
				for(int m = 0; m < 12; m++) {
					t.matrix[m] = RF32(data, off);
					off += 4;
				}
			}
		}

		// surf_things: count(2) + N*72
		s.num_surf_things = RU16(data, off);
		off += 2;
		if(s.num_surf_things > 0) {
			s.surf_things = (SurfThing *)malloc(s.num_surf_things * sizeof(SurfThing));
			for(uint16_t sti = 0; sti < s.num_surf_things; sti++) {
				memcpy(s.surf_things[sti].data, data + off, 72);
				off += 72;
			}
		}

		// envRefs: count(2) + N*32
		s.num_env_refs = RU16(data, off);
		off += 2;
		for(uint16_t ei = 0; ei < s.num_env_refs && ei < 64; ei++) {
			ReadName32(data, off, s.env_ref_names[ei]);
			off += 32;
		}

		// tail: field1(2) + field2(4)
		s.tail_field1 = RU16(data, off);
		off += 2;
		s.tail_field2 = RU32(data, off);
		off += 4;

		lvl.total_verts += s.num_verts;
		lvl.total_indices += s.num_indices;
		lvl.total_groups += s.num_groups;
	}

	// populate compatibility fields for renderer
	lvl.material_count = lvl.num_shaders;
	for(uint32_t i = 0; i < lvl.num_shaders; i++) {
		memcpy(lvl.material_names[i], lvl.shader_names[i], 33);
	}
	// level_bounds from first sector center
	if(lvl.num_sectors > 0) {
		lvl.level_bounds[0] = lvl.sectors[0].center[0];
		lvl.level_bounds[1] = lvl.sectors[0].center[1];
		lvl.level_bounds[2] = lvl.sectors[0].center[2];
	}

	printf("NILParser: v%u, %u shaders, %u envmaps, %u sectors\n", lvl.version, lvl.num_shaders, lvl.num_env_maps,
		   lvl.num_sectors);
	printf("NILParser: %u verts, %u indices, %u groups\n", lvl.total_verts, lvl.total_indices, lvl.total_groups);

	// post-sector data: collision mesh + BSP/lightmap atlas (not fully decoded)
	size_t postOff	= off;
	size_t postSize = file_size - off;
	printf("NILParser: sector data ends at 0x%zx\n", off);
	if(postSize >= 32) {
		float aabbMin[3], aabbMax[3];
		for(int i = 0; i < 3; i++) aabbMin[i] = RF32(data, off + 4 + i * 4);
		for(int i = 0; i < 3; i++) aabbMax[i] = RF32(data, off + 16 + i * 4);
		uint8_t collVertCount = data[off + 31];
		size_t collDataSize	  = 32 + (size_t)collVertCount * 12 + 2;
		printf("NILParser: post-sector: %zu bytes (%.1f%% of file)\n", postSize, 100.0 * postSize / file_size);
		printf("NILParser:   AABB (%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)\n", aabbMin[0], aabbMin[1], aabbMin[2], aabbMax[0],
			   aabbMax[1], aabbMax[2]);
		printf("NILParser:   collision mesh: %u verts, %zu bytes\n", collVertCount, collDataSize);
		printf("NILParser:   BSP/lightmap atlas: ~%zu bytes (not parsed)\n", postSize - collDataSize);
	} else {
		printf("NILParser: post-sector: %zu bytes (not parsed)\n", postSize);
	}

	return 1;
}

/*
 */
int NILParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, lvl.raw_data, lvl.raw_data_size);
}
