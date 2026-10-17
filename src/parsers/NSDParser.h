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

#ifndef __NSDPARSER_H__
#define __NSDPARSER_H__

#include "AssetParser.h"

/*
 */
class NSDParser : public IAssetParser {
		
	public:
		static const uint32_t MAX_ENTITIES	 = 1024;
		static const uint32_t MAX_TYPE_NAMES = 128;

		struct SceneEntity {
			char type_name[65];
			size_t block_offset;
			size_t block_size;
			float position[3];	  // world-space XYZ (D3D coordinates, from block +72)
			float rotation[3];	  // euler degrees (pitch X, yaw Y, roll Z, from block +36)
			char model_name[64];  // model reference (from tag4 block, e.g. "door.nod")
			char script_name[64]; // attached script/effect name (from tag 0xa1 at +140)
			bool has_transform;	  // true if tag-validated block with position/rotation
			bool has_model;		  // valid, nonempty tag4 model reference
			bool has_model_override; // includes an explicit empty tag4 value
			bool model_override_valid;
			bool has_sector;
			uint32_t sector;
			char region_class[33];
			bool has_bounds;
			float bounds_min[3]; // local-space AABB of referenced inline LOD-0 vertices
			float bounds_max[3];
			const char *bounds_status;
		};

		struct SceneData {
			uint32_t magic;
			uint32_t entity_count; // declared count at header offset 80

			SceneEntity *entities;
			uint32_t entity_array_count;

			char entity_type_names[MAX_TYPE_NAMES][65];
			uint32_t type_name_count;

			// sky dome model (extracted from NSD metadata block)
			char sky_model_name[64]; // e.g. "MS_Sky_Refinery.nod"
			bool has_sky_model;

			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		NSDParser() { 
			memset(&sd, 0, sizeof(sd)); 
		}

		~NSDParser() override {
			free(sd.entities);
			free(sd.raw_data);
		}

		int parse(const char *filePath) override;
		// Copies source bytes; parsing again invalidates previous entity/raw-data views.
		int parseBuffer(const uint8_t *data, size_t size);
		const char *getLastError() const { return last_error; }
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NSD"; }
		const SceneData &getSceneData() const { return sd; }

	private:
		SceneData sd;
		const char *last_error = "";
		void clear();
		int reject(const char *message);
		int extract_entity_names(const uint8_t *data, size_t data_size);
		int extract_region_bounds(SceneEntity &entity, const uint8_t *data);
		void extract_sky_model(const uint8_t *data, size_t data_size);
};

#endif // __NSDPARSER_H__
