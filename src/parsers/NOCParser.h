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

#ifndef __NOCPARSER_H__
#define __NOCPARSER_H__

#include "AssetParser.h"
#include <glm/glm.hpp>

/*
 */
class NOCParser : public IAssetParser {

	public:
		static const uint32_t MAX_SHAPES	= 64;
		static const uint32_t VERTEX_STRIDE = 32;

		/*
		*/
		struct CollisionShape {
			char name[33]; // shape or collision type name
			char collision_type[33]; // e.g., "physclip" (only if flags[0]=2)
			float matrix[9]; // 3x3 rotation matrix, row-major
			float position[3]; // shape offset position
			uint8_t color[4]; // RGBA debug color
		};

		/*
		*/
		struct CollisionData {
			uint32_t version; // always 10
			uint8_t flags[4]; // flags[0]: name count, flags[3]: shape count

			glm::vec3 bounds_min;
			glm::vec3 bounds_max;

			uint32_t shape_count; // from flags[3]
			CollisionShape shapes[MAX_SHAPES];

			uint32_t num_vertices;
			uint32_t num_indices;

			// vertex data: pos(3f) + normal(3f) + uv(2f) = 32 bytes
			float *vertex_data; // interleaved, num_vertices * 8 floats
			uint16_t *indices; // triangle list

			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		NOCParser() { memset(&m, 0, sizeof(m)); }
		~NOCParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NOC"; }
		const CollisionData &getCollisionData() const { return m; }

	private:
		CollisionData m;
};

#endif // __NOCPARSER_H__
