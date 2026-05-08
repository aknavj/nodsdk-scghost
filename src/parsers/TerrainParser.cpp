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

#include "TerrainParser.h"

/*
 */
int TerrainParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 8) {
		free(data);
		return 0;
	}

	grid.file_size = file_size;
	grid.version   = FileUtils::readUInt32(data, 0);

	uint32_t packed	 = FileUtils::readUInt32(data, 4);
	grid.grid_width	 = (uint16_t)(packed & 0xFFFF);
	grid.grid_height = (uint16_t)((packed >> 16) & 0xFFFF);

	uint32_t patch_count = (uint32_t)grid.grid_width * grid.grid_height;
	if(patch_count == 0 || patch_count > 1000) patch_count = 1;

	grid.patches	 = (TerrainPatch *)calloc(patch_count, sizeof(TerrainPatch));
	grid.patch_count = patch_count;

	// read material/patch names.
	// NRT format: fixed 32-byte name entries starting at offset 8.
	// each name may be followed by a 64-byte UV transform (4x4 matrix).
	// we detect names by checking for printable ASCII at 32-byte boundaries.
	size_t off		   = 8;
	uint32_t nameCount = 0;

	while(off + 32 <= file_size && nameCount < patch_count) {
		// check if this 32-byte slot starts with printable ASCII
		if(data[off] < 32 || data[off] > 126) break;
		int isName = true;
		for(size_t i = 0; i < 32; ++i) {
			uint8_t c = data[off + i];
			if(c == 0) break;
			if(c < 32 || c > 126) {
				isName = false;
				break;
			}
		}
		if(!isName) break;

		size_t len = 0;
		while(len < 32 && data[off + len] != 0) ++len;
		if(len > 255) len = 255;
		memcpy(grid.patches[nameCount].name, data + off, len);
		grid.patches[nameCount].name[len] = '\0';
		off += 32;
		nameCount++;

	// check if followed by a UV transform (64 bytes = 16 floats).
	// heuristic: if first float is a reasonable scale value (0.1..100),
	// treat next 64 bytes as a matrix.
		if(off + 64 <= file_size) {
			float firstFloat = FileUtils::readFloat(data, off);
			if(firstFloat > 0.01f && firstFloat < 1000.0f) {
				for(int i = 0; i < 4; ++i)
					grid.patches[nameCount - 1].scale_matrix[i] = FileUtils::readFloat(data, off + i * 4);
				off += 64;
			}
		}
	}

	// name remaining unnamed patches
	for(uint32_t p = nameCount; p < patch_count; ++p) {
		snprintf(grid.patches[p].name, 256, "Patch_%u", p);
	}

	// store remaining data as terrain blob, split evenly among patches
	if(off < file_size) {
		size_t rem		= file_size - off;
		size_t perPatch = rem / patch_count;
		size_t extra	= rem % patch_count;

		for(uint32_t p = 0; p < patch_count; ++p) {
			size_t thisSize = perPatch + (p < extra ? 1 : 0);
			if(off + thisSize <= file_size) {
				grid.patches[p].data_size = thisSize;
				grid.patches[p].data	  = (uint8_t *)malloc(thisSize);
				memcpy(grid.patches[p].data, data + off, thisSize);
				off += thisSize;
			}
		}
	}

	grid.raw_data	   = data;
	grid.raw_data_size = file_size;
	return 1;
}

/*
 */
int TerrainParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, grid.raw_data, grid.raw_data_size);
}
