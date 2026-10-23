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

#ifndef __NILPARSER_H__
#define __NILPARSER_H__

#include "AssetParser.h"

/*
 */
class NILParser : public IAssetParser {
	
	public:
		static const uint32_t MAX_SHADERS = 256;
		static const uint32_t MAX_SECTORS = 512;
		static const uint32_t MAX_GROUPS  = 512;

		/*
		 */
		// 47 bytes per group (fixed size)
		struct SurfaceGroup {
			uint32_t shader_idx; // +0
			uint32_t start_idx; // +4  offset into index buffer
			uint32_t field8; // +8  (always 0?)
			uint16_t num_strip_indices; // +12 triangle strip index count
			uint16_t num_list_indices; // +14 triangle list index count
			uint32_t field16; // +16
			uint8_t vert_type; // +20
			uint8_t flag; // +21
			uint8_t field22; // +22
			float bbox_min[3]; // +23
			float bbox_max[3]; // +35
		};

		/*
		 */
		struct Thing {
			float position[3];
			uint32_t field1;
			uint32_t field2;
			float matrix[12]; // 4 vec3 rows
		};

		/*
		 */
		struct SurfThing {
			// Legacy name: native tRenderLight static-light record, not geometry.
			uint8_t data[72];
		};

		/*
		 */
		struct Sector {
			uint32_t flags;
			float center[3];
			uint8_t num_portals;
			uint8_t portal_data[256 * 16]; // up to 256 portals, 16B each

			uint8_t has_geo;
			uint8_t vert_type; // 0=standard, 1=with TBN
			uint16_t num_verts;
			uint8_t *vertex_data; // stride 36
			uint8_t *tbn_data; // stride 36, only when vert_type==1

			uint8_t lm_flag; // Legacy name: secondary vertex-pool type, not a lightmap flag
			uint32_t lm_patch_count; // Legacy name: secondary vertex count

			uint16_t num_groups;
			SurfaceGroup *groups;

			uint32_t num_indices;
			uint16_t *indices;

			uint8_t num_things;
			Thing *things;

			uint16_t num_surf_things;
			SurfThing *surf_things;

			uint16_t num_env_refs;
			char env_ref_names[64][33];

			uint16_t tail_field1;
			uint32_t tail_field2;
		};

		/*
		 */
		struct LevelData {
			uint32_t magic;
			uint32_t version;
			uint8_t global_light[72];
			float ambient_color[3];

			uint32_t num_shaders;
			char shader_names[MAX_SHADERS][33];

			uint16_t num_env_maps;
			char env_map_names[64][33];

			uint32_t num_sectors;
			Sector *sectors;

			uint32_t total_verts;
			uint32_t total_indices;
			uint32_t total_groups;

			// compatibility fields for renderer
			uint32_t material_count;
			char material_names[MAX_SHADERS][33];
			float level_bounds[3];

			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		NILParser() { memset(&lvl, 0, sizeof(lvl)); }
		~NILParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NIL"; }
		const LevelData &getLevelData() const { return lvl; }

	private:
		LevelData lvl;
		void free_sectors();
};

#endif // __NILPARSER_H__
