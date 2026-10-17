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

#ifndef __NODPARSER_H__
#define __NODPARSER_H__

#include "AssetParser.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

/*
 */
class NODParser : public IAssetParser {

	public:

		static const uint32_t MAX_VERTEX_SETS = 4;
		static const uint32_t MAX_MESH_GROUPS = 256;
		static const uint32_t MAX_LODS		  = 4;
		static const uint32_t MAX_SHADERS	  = 64;

		// file structures
		/*
		*/
		struct VertexSetDesc {
			uint32_t type; // 0=rigid(32B), 1=skinned(36B), 2=weighted(48B)
			uint32_t count;
		};

		/*
		*/
		struct FileHeader {
			uint32_t flags; // 0x00 (also checked as version >= 10)
			uint8_t shader_count; // 0x04
			uint8_t bone_count; // 0x05
			uint8_t vertex_set_count; // 0x06
			uint8_t mesh_group_count; // 0x07
			uint32_t unknown; // 0x08 (always 0?)
			glm::vec3 bounds_min; // 0x0C
			glm::vec3 bounds_max; // 0x18
			VertexSetDesc vertex_sets[MAX_VERTEX_SETS]; // 0x24 (4x8B = 32B)
			uint32_t index_count; // 0x44
														// 0x48..0x5B = padding (20 bytes, zeros)
		};

		/*
		*/
		struct Bone {
			float position[3]; // 0x00  restTranslate
			float orientation[9]; // 0x0C  (3x3 matrix, identity in practice)
			float offset[3]; // 0x30  invRootTranslate = -accumulated pos
			uint8_t pad0; // 0x3C  (secondary link / IK parent)
			uint8_t pad1; // 0x3D  (joint mapping tag)
			uint8_t parent; // 0x3E  skeleton parent (0xFF = root)
			uint8_t joint_id; // 0x3F  tag / marker ID
		};

		/*
		*/
		struct MeshGroupLOD {
			uint32_t strip_start; // index into global index buffer
			uint16_t strip_count; // number of strip indices
			uint32_t list_start; // index into global index buffer
			uint16_t list_count; // number of list indices
			uint16_t num_verts; // vertex count for this LOD
		};

		/*
		*/
		struct MeshGroup {
			uint32_t shader_index;
			MeshGroupLOD lods[MAX_LODS];
			uint32_t base_vertex; // offset into vertex set
			uint8_t vertex_set_index; // which vertex set (0..vertex_set_count-1)
			uint8_t num_bones;
			uint8_t a_bones[20]; // bone palette: vertex idx -> global bone idx
		};

		/*
		*/
		struct VertexSet {
			uint32_t type;
			uint32_t stride; // total bytes per vert in file (32..84)
			uint32_t base_stride; // stream-0 stride for pos/norm/uv (32/36/48)
			uint32_t count;
			uint32_t data_offset; // byte offset into vertex_data blob
		};

		/*
		*/
		// parsed model data
		struct ModelData {
			FileHeader header;

			char shader_names[MAX_SHADERS][33];
			uint32_t shader_count;

			Bone *bones;
			uint32_t bone_count;

			VertexSet vertex_sets[MAX_VERTEX_SETS];
			uint32_t vertex_set_count;

			uint8_t *vertex_data; // all vertex sets contiguous
			size_t vertex_data_size;

			uint16_t *indices; // flat shared index buffer
			uint32_t index_count;

			MeshGroup *mesh_groups;
			uint32_t mesh_group_count;

			uint32_t marker_count;

			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		NODParser() { memset(&m, 0, sizeof(m)); }
		~NODParser() override;

		int parse(const char *filePath) override;
		int parseBuffer(const uint8_t *data, size_t size);
		const std::string &getLastError() const { return last_error; }
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NOD"; }
		const ModelData &getModelData() const { return m; }
		bool computeBounds(float minimum[3], float maximum[3], std::string &error) const;

	private:
		ModelData m;
		std::string last_error;
		void clear();
		int fail(const char *message);
		int parseOwnedBuffer(uint8_t *data, size_t size);
};

#endif // __NODPARSER_H__
