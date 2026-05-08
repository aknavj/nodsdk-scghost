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

#ifndef __TERRAINPARSER_H__
#define __TERRAINPARSER_H__

#include "AssetParser.h"
#include <glm/glm.hpp>

/*
 */
class TerrainParser : public IAssetParser {

	public:

		/*
		 */
		struct TerrainPatch {
			char name[256];
			float scale_matrix[4];
			uint8_t *data;
			size_t data_size;
		};

		/*
		 */
		struct TerrainGrid {
			uint32_t version;
			uint16_t grid_width;
			uint16_t grid_height;
			TerrainPatch *patches;
			uint32_t patch_count;
			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		TerrainParser() { 
			memset(&grid, 0, sizeof(grid)); 
		}

		~TerrainParser() override {
			if(grid.patches) {
				for(uint32_t i = 0; i < grid.patch_count; ++i) free(grid.patches[i].data);
				free(grid.patches);
			}
			free(grid.raw_data);
		}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "TERRAIN"; }
		const TerrainGrid &getTerrainGrid() const { return grid; }

	private:
		TerrainGrid grid;
};

#endif // __TERRAINPARSER_H__
