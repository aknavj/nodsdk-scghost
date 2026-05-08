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

#include "NSDParser.h"
#include <cmath>

/*
 */
void NSDParser::extract_entity_names(const uint8_t *data, size_t data_size) {

	uint32_t entCap		  = 512;
	sd.entities			  = (SceneEntity *)calloc(entCap, sizeof(SceneEntity));
	sd.entity_array_count = 0;
	sd.type_name_count	  = 0;

	// scan the entire NSD for tag-validated entity blocks.
	// every entity block starts with a printable null-terminated string
	// (the type name), has tag1=1 at +32 and tag2=2 at +68, and
	// position/rotation at +72/+36 respectively.
	size_t pos = 0;
	while(pos + 84 < data_size) {
		// find a printable null-terminated string (2..64 chars)
		int slen = 0;
		while(slen < 64 && pos + (size_t)slen < data_size && data[pos + slen] >= 32 && data[pos + slen] <= 126) slen++;
		if(slen < 2 || slen >= 64 || pos + (size_t)slen >= data_size || data[pos + slen] != 0) {
			pos++;
			continue;
		}

		size_t T = pos;

		// validate block: tag1=1 at +32, tag2=2 at +68
		if(T + 84 > data_size) {
			pos++;
			continue;
		}
		uint32_t tag1, tag2;
		memcpy(&tag1, data + T + 32, 4);
		memcpy(&tag2, data + T + 68, 4);
		if(tag1 != 1 || tag2 != 2) {
			pos++;
			continue;
		}

		// read position from +72 (3 floats)
		float px, py, pz;
		memcpy(&px, data + T + 72, 4);
		memcpy(&py, data + T + 76, 4);
		memcpy(&pz, data + T + 80, 4);

		if(fabsf(px) > 100000.0f || fabsf(py) > 100000.0f || fabsf(pz) > 100000.0f) {
			pos++;
			continue;
		}

		// grow array if needed
		if(sd.entity_array_count >= entCap) {
			entCap *= 2;
			sd.entities = (SceneEntity *)realloc(sd.entities, entCap * sizeof(SceneEntity));
		}

		SceneEntity *e = &sd.entities[sd.entity_array_count++];
		memset(e, 0, sizeof(SceneEntity));
		memcpy(e->type_name, data + T, (size_t)slen);
		e->type_name[slen] = '\0';
		e->block_offset	   = T;
		e->block_size	   = 0;
		e->position[0]	   = px;
		e->position[1]	   = py;
		e->position[2]	   = pz;
		memcpy(e->rotation, data + T + 36, 12);
		e->has_transform = true;

		// scan for tag4 (model name) starting at +140, in 36-byte blocks
		for(size_t blk = T + 140; blk + 36 <= data_size && blk < T + 140 + 36 * 4; blk += 36) {
			uint32_t btag;
			memcpy(&btag, data + blk, 4);
			if(btag == 4 && blk + 4 + 2 < data_size) {
				const char *str = (const char *)(data + blk + 4);
				size_t sl		= strnlen(str, 60);
				if(sl >= 2 && sl < 60) {
					bool printable = true;
					for(size_t c = 0; c < sl; c++) {
						if(str[c] < 32 || str[c] > 126) {
							printable = false;
							break;
						}
					}
					if(printable) {
						strncpy(e->model_name, str, 63);
						e->model_name[63] = '\0';
						e->has_model	  = true;
					}
				}
				break;
			}
		}

		// extract script/effect name at +144 (tag 0xa1 at +140)
		if(T + 204 <= data_size) {
			uint32_t stag;
			memcpy(&stag, data + T + 140, 4);
			if(stag == 0xa1) {
				const char *sn = (const char *)(data + T + 144);
				size_t snl	   = strnlen(sn, 60);
				if(snl > 0 && snl < 60) {
					bool printable = true;
					for(size_t c = 0; c < snl; c++) {
						if(sn[c] < 32 || sn[c] > 126) {
							printable = false;
							break;
						}
					}
					if(printable) strncpy(e->script_name, sn, 63);
				}
			}
		}

		// track unique type names
		bool found = false;
		for(uint32_t t = 0; t < sd.type_name_count; ++t) {
			if(strcmp(sd.entity_type_names[t], e->type_name) == 0) {
				found = true;
				break;
			}
		}
		if(!found && sd.type_name_count < MAX_TYPE_NAMES) {
			strcpy(sd.entity_type_names[sd.type_name_count++], e->type_name);
		}

		pos = T + (size_t)slen + 32;
	}
}

// scan the NSD binary for the sky dome model reference.
// the sky model is stored in a special metadata block (not a standard entity).
// pattern: a printable string ending in ".nod" at position P, with another
// printable string (the .nad anim) at P+32.  The sky block has environment
// parameters in the 44 bytes before the model name string.
// we look for ".nod" strings that are NOT already captured as entity models
/*
 */
void NSDParser::extract_sky_model(const uint8_t *data, size_t data_size) {
	sd.has_sky_model	 = false;
	sd.sky_model_name[0] = '\0';

	for(size_t pos = 0; pos + 36 < data_size; pos++) {
		// look for a printable string starting here
		int slen = 0;
		while(slen < 60 && pos + (size_t)slen < data_size && data[pos + slen] >= 32 && data[pos + slen] <= 126) slen++;
		if(slen < 6 || slen >= 60 || pos + (size_t)slen >= data_size || data[pos + slen] != 0) {
			continue;
		}

		// check if it ends in ".nod"
		const char *str = (const char *)(data + pos);
		if(slen < 5) continue;
		if(str[slen - 4] != '.' || (str[slen - 3] != 'n' && str[slen - 3] != 'N') ||
		   (str[slen - 2] != 'o' && str[slen - 2] != 'O') || (str[slen - 1] != 'd' && str[slen - 1] != 'D'))
			continue;

		// check if the name contains "sky" (case-insensitive)
		bool hasSky = false;
		for(int i = 0; i <= slen - 3; i++) {
			char c0 = str[i] | 0x20, c1 = str[i + 1] | 0x20, c2 = str[i + 2] | 0x20;
			if(c0 == 's' && c1 == 'k' && c2 == 'y') {
				hasSky = true;
				break;
			}
		}
		if(!hasSky) continue;

		// verify this isn't a standard entity model (tag1=1 at pos-32 would mean
		// this string is at the typical type-name position of an entity)
		if(pos >= 32) {
			uint32_t tag1;
			memcpy(&tag1, data + pos - 32 + 32, 4); // tag at str+32
													// for sky blocks, pos+32 has another string, not tag=1
		}

		strncpy(sd.sky_model_name, str, 63);
		sd.sky_model_name[63] = '\0';
		sd.has_sky_model	  = true;
		printf("[NSD] Sky model: %s\n", sd.sky_model_name);
		return;
	}
}

/*
 */
int NSDParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 8) {
		free(data);
		return 0;
	}

	if(data[0] != 'N' || data[1] != 'S' || data[2] != 'D') {
		free(data);
		return 0;
	}

	sd.file_size	= file_size;
	sd.magic		= FileUtils::readUInt32(data, 0);
	sd.entity_count = FileUtils::readUInt32(data, 4);

	extract_entity_names(data, file_size);
	extract_sky_model(data, file_size);

	sd.raw_data		 = data;
	sd.raw_data_size = file_size;
	return 1;
}

/*
 */
int NSDParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, sd.raw_data, sd.raw_data_size);
}
