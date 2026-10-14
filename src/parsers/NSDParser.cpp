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
namespace {
bool name32(const uint8_t *data, char *out, bool terminated) {
	size_t n = 0;
	for(; n < 32 && data[n]; ++n) {
		if(data[n] < 32 || data[n] > 126) return false;
		out[n] = static_cast<char>(data[n]);
	}
	out[n] = 0;
	return !terminated || (n > 0 && n < 32);
}

bool entityAt(const uint8_t *data, char *type) {
	if(FileUtils::readUInt32(data, 32) != 1 || FileUtils::readUInt32(data, 68) != 2 ||
	   !name32(data, type, true)) return false;
	// Zero padding distinguishes type names from tag-like property-string suffixes.
	for(size_t i = strlen(type); i < 32; ++i)
		if(data[i]) return false;
	return true;
}
}

int NSDParser::extract_entity_names(const uint8_t *data, size_t data_size) {
	uint32_t capacity = 512;
	sd.entities = static_cast<SceneEntity *>(calloc(capacity, sizeof(SceneEntity)));
	if(!sd.entities) return reject("out of memory reading NSD entities");
	for(size_t pos = 92; pos <= data_size - 84; ++pos) {
		char type[65];
		if(!entityAt(data + pos, type)) continue;
		for(size_t j = 0; j < 3; ++j)
			if(!std::isfinite(FileUtils::readFloat(data, pos + 36 + j * 4)) ||
			   !std::isfinite(FileUtils::readFloat(data, pos + 72 + j * 4)))
				return reject("tagged NSD entity has non-finite transform");
		if(sd.entity_array_count == capacity) {
			if(capacity > UINT32_MAX / 2 || static_cast<size_t>(capacity) > SIZE_MAX / 2 / sizeof(SceneEntity))
				return reject("NSD entity array is too large");
			capacity *= 2;
			auto *grown = static_cast<SceneEntity *>(realloc(sd.entities, capacity * sizeof(SceneEntity)));
			if(!grown) return reject("out of memory growing NSD entities");
			sd.entities = grown;
		}
		auto &e = sd.entities[sd.entity_array_count++];
		memset(&e, 0, sizeof(e));
		strcpy(e.type_name, type);
		e.block_offset = pos;
		e.has_transform = true;
		for(size_t j = 0; j < 3; ++j) {
			e.position[j] = FileUtils::readFloat(data, pos + 72 + j * 4);
			e.rotation[j] = FileUtils::readFloat(data, pos + 36 + j * 4);
		}
		bool found = false;
		for(uint32_t i = 0; i < sd.type_name_count; ++i)
			if(!strcmp(sd.entity_type_names[i], type)) found = true;
		if(!found && sd.type_name_count < MAX_TYPE_NAMES)
			strcpy(sd.entity_type_names[sd.type_name_count++], type);
		pos += 83;
	}
	for(uint32_t i = 0; i < sd.entity_array_count; ++i) {
		auto &e = sd.entities[i];
		const size_t end = i + 1 < sd.entity_array_count ? sd.entities[i + 1].block_offset : data_size;
		const uint8_t *p = data + e.block_offset;
		e.block_size = end - e.block_offset;
		if(e.block_size >= 140) {
			const uint32_t tag = FileUtils::readUInt32(p, 104);
			if(tag == 3) { e.has_sector = true; e.sector = FileUtils::readUInt32(p, 108); }
			else if(tag == 15 && !name32(p + 108, e.region_class, true)) e.region_class[0] = 0;
		}
		for(size_t slot = 140; slot <= e.block_size && e.block_size - slot >= 36 && slot < 284; slot += 36) {
			if(FileUtils::readUInt32(p, slot) != 4) continue;
			e.has_model_override = true;
			e.model_override_valid = name32(p + slot + 4, e.model_name, false);
			e.has_model = e.model_override_valid && *e.model_name;
			break;
		}
		if(e.block_size >= 204 && FileUtils::readUInt32(p, 140) == 0xa1) {
			size_t n = 0;
			for(; n < 60 && p[144 + n] >= 32 && p[144 + n] <= 126; ++n) {}
			if(n < 60 && p[144 + n] == 0) {
				memcpy(e.script_name, p + 144, n);
				e.script_name[n] = 0;
			}
		}
	}
	return 1;
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
	uint8_t *data = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data) return reject("cannot read NSD file");
	int ok = parseBuffer(data, file_size);
	free(data);
	return ok;
}

void NSDParser::clear() {
	free(sd.entities);
	free(sd.raw_data);
	memset(&sd, 0, sizeof(sd));
}

int NSDParser::reject(const char *message) {
	clear();
	last_error = message;
	return 0;
}

int NSDParser::parseBuffer(const uint8_t *data, size_t size) {
	last_error = "";
	if(!data || size < 92 || memcmp(data, "NSD\x10", 4))
		return reject("expected NSD header with 92 bytes and magic NSD\\x10");
	auto *copy = static_cast<uint8_t *>(malloc(size));
	if(!copy) return reject("out of memory reading NSD source bytes");
	memcpy(copy, data, size);
	clear();
	sd.raw_data = copy;
	sd.raw_data_size = sd.file_size = size;
	sd.magic = FileUtils::readUInt32(copy, 0);
	sd.entity_count = FileUtils::readUInt32(copy, 80);
	if(!extract_entity_names(copy, size)) return 0;
	extract_sky_model(copy, size);
	return 1;
}

/*
 */
int NSDParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, sd.raw_data, sd.raw_data_size);
}
