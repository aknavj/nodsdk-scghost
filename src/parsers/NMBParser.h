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

#ifndef __NMBPARSER_H__
#define __NMBPARSER_H__

#include "AssetParser.h"
#include "NODParser.h"

/*
 */
class NMBParser : public IAssetParser {

	public:

		static constexpr uint32_t MAX_MODELS			= 256;
		static constexpr uint32_t MAX_SHADERS_PER_MODEL = 64;

		/*
		 */
		struct ModelEntry {
			char name[64];
			uint8_t shader_count;
			uint8_t bone_count;
			uint8_t mesh_group_count;
			uint16_t marker_count;
			uint8_t vertex_set_count;
			uint32_t flags;
			float bounds_min[3];
			float bounds_max[3];

			// per vertex set info (same as NOD)
			NODParser::VertexSet vertex_sets[NODParser::MAX_VERTEX_SETS];

			// pointers into raw data buffer (valid while parser alive)
			uint8_t *vertex_data; // all vertex sets contiguous
			size_t vertex_data_size;
			uint16_t *indices; // flat shared index buffer
			uint32_t index_count;
			const uint8_t *vertex_colors; // optional version-2 packed colour stream in raw_data
			uint32_t color_count;

			char shader_names[MAX_SHADERS_PER_MODEL][33];

			// parsed bones (26-byte bundle format -> NOD bone struct)
			NODParser::Bone *bones; // heap-allocated [bone_count]

			// parsed mesh groups (variable-length -> NOD mesh group struct)
			NODParser::MeshGroup *mesh_groups; // heap-allocated [mesh_group_count]

			uint32_t total_vertices;
			uint32_t blend_count;
			size_t entry_offset; // byte offset of this entry in file
		};

		/*
		 */
		struct BundleData {
			uint32_t version;
			uint32_t model_count;
			uint32_t total_mem;
			uint32_t parsed_count; // how many entries were successfully parsed
			uint32_t total_vertices; // sum across all entries
			uint32_t total_indices; // sum across all entries
			size_t file_size;
			ModelEntry models[MAX_MODELS];
		};

		NMBParser() : raw_data(nullptr) {
			memset(&m, 0, sizeof(m));
		}

		~NMBParser() override;

		int parse(const char *filePath) override;
		int parseBuffer(const uint8_t *data, size_t size);
		const std::string &getLastError() const { return last_error; }
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NMB"; }
		const BundleData &getBundleData() const { return m; }

	private:
		static uint32_t type_to_stride(uint8_t type) {
			switch(type) {
				case 0:
					return 32;
				case 1:
					return 36;
				case 2:
					return 48;
				case 3:
					return 68;
				case 4:
					return 72;
				case 5:
					return 84;
				default:
					return 0;
			}
		}

		int parse_entry(const uint8_t *data, size_t file_size, size_t off, ModelEntry *entry, size_t *nextOffset);

		uint8_t *raw_data; // kept alive for vertex/index pointers
		BundleData m;
		std::string last_error;
		void clear();
		int fail(const char *message);
		int parseOwnedBuffer(uint8_t *data, size_t size);
};

#endif // __NMBPARSER_H__
