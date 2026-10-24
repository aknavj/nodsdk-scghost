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
#include "NODParser.h"
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
	for(size_t pos = sd.object_data_offset + 8; pos <= data_size - 84; ++pos) {
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
		if(*e.region_class && !extract_region_bounds(e, data)) return 0;
	}
	return 1;
}

int NSDParser::extract_region_bounds(SceneEntity &entity, const uint8_t *data) {
	entity.bounds_status = "unsupported-layout";
	const size_t start = entity.block_offset;
	const uint8_t *p = data + start;
	// A type name is the payload of modifier zero; its count precedes that modifier.
	if(start < 8 || FileUtils::readUInt32(data, start - 4) != 0) return 1;
	const uint32_t modifiers = FileUtils::readUInt32(data, start - 8);
	if(modifiers < 4 || modifiers > 128) return 1;
	size_t offset = 32;
	auto consume = [&](size_t count, size_t stride) {
		if(offset > entity.block_size || count > (entity.block_size - offset) / stride) return false;
		offset += count * stride;
		return true;
	};
	if(!consume(modifiers - 1, 36)) return 1;
	if(!consume(1, 4)) return reject("truncated NSD region frame count");
	const uint32_t frames = FileUtils::readUInt32(p, offset - 4);
	if(!consume(frames, 28) || !consume(1, 4)) return reject("truncated NSD region frames or parameter count");
	const uint32_t parameters = FileUtils::readUInt32(p, offset - 4);
	for(uint32_t i = 0; i < parameters; ++i) {
		if(!consume(1, 4)) return reject("truncated NSD region parameter type");
		const uint32_t type = FileUtils::readUInt32(p, offset - 4);
		if(type == 6) continue;
		if(type < 1 || type > 11) {
			entity.bounds_status = "unsupported-parameter-type";
			return 1;
		}
		if(!consume(1, 4)) return reject("truncated NSD region parameter value");
		if(type == 7 && !consume(FileUtils::readUInt32(p, offset - 4), 1))
			return reject("truncated NSD region parameter string");
	}
	if(!consume(1, 4)) return reject("truncated NSD region inline-model flag");
	if(!FileUtils::readUInt32(p, offset - 4)) {
		entity.bounds_status = "no-inline-model";
		return 1;
	}
	const size_t model_offset = offset;
	if(!consume(1, 92)) return reject("truncated NSD region inline-model header");
	const uint8_t *header = p + model_offset;
	if(header[6] > NODParser::MAX_VERTEX_SETS) return reject("invalid NSD region vertex set count");
	if(!consume(header[4], 32) || !consume(header[5], 64))
		return reject("truncated NSD region shaders or bones");
	for(uint8_t i = 0; i < header[6]; ++i) {
		const uint32_t type = FileUtils::readUInt32(header, 36 + size_t(i) * 8);
		const uint32_t count = FileUtils::readUInt32(header, 40 + size_t(i) * 8);
		static const size_t strides[] = {32, 36, 48, 68, 72, 84};
		if(type > 5 || !consume(count, strides[type])) return reject("invalid NSD region vertex stream");
	}
	if(!consume(FileUtils::readUInt32(header, 68), 2) || !consume(header[7], 56))
		return reject("truncated NSD region indices or groups");
	NODParser model;
	std::string error;
	if(!model.parseBuffer(p + model_offset, offset - model_offset) ||
	   !model.computeBounds(entity.bounds_min, entity.bounds_max, error)) {
		fprintf(stderr, "NSDParser: region at byte %zu: %s\n", start,
				error.empty() ? model.getLastError().c_str() : error.c_str());
		return reject("invalid NSD region inline geometry");
	}
	entity.has_bounds = true;
	entity.bounds_status = "inline-mesh";
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
		if(pos >= 80 && pos < sd.object_data_offset) {
			pos = sd.object_data_offset - 1;
			continue;
		}
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
		fprintf(stderr, "[NSD] Sky model: %s\n", sd.sky_model_name);
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
	template_files.clear();
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
	sd.version = FileUtils::readUInt32(copy, 4);
	const uint32_t archive_count = FileUtils::readUInt32(copy, 76);
	if(archive_count > MAX_TEMPLATE_FILES || archive_count > (size - 84) / 32)
		return reject("invalid or truncated NSD template archive list");
	for(uint32_t i = 0; i < archive_count; ++i) {
		char name[33];
		if(!name32(copy + 80 + size_t(i) * 32, name, true))
			return reject("invalid NSD template archive name");
		template_files.emplace_back(name);
	}
	const size_t count_offset = 80 + size_t(archive_count) * 32;
	sd.entity_count = FileUtils::readUInt32(copy, count_offset);
	sd.object_data_offset = count_offset + 4;
	if(!extract_entity_names(copy, size)) return 0;
	extract_sky_model(copy, size);
	return 1;
}

/*
 */
int NSDParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, sd.raw_data, sd.raw_data_size);
}

bool NSDParser::getModifiers(const SceneEntity &entity, std::vector<NODModifier> &out) const {
	return readModifiers(sd.raw_data, sd.raw_data_size, entity, out);
}

bool NSDParser::readModifiers(const uint8_t *data, size_t size, const SceneEntity &entity,
							 std::vector<NODModifier> &out) {
	out.clear();
	const auto offset = entity.block_offset;
	if(!data || offset < 8 || offset > size || entity.block_size > size - offset) return false;
	if(FileUtils::readUInt32(data, offset - 4) != 0) return false;
	const auto count = FileUtils::readUInt32(data, offset - 8);
	if(count < 3 || count > 128 || size_t(count) * 36 > entity.block_size + 4) return false;
	for(uint32_t i = 0; i < count; ++i) {
		const size_t start = offset - 4 + size_t(i) * 36;
		NODModifier property{};
		property.prop_id = FileUtils::readUInt32(data, start);
		property.value_size = 32;
		property.value_offset = start + 4;
		memcpy(property.value, data + start + 4, 32);
		out.push_back(property);
	}
	return true;
}
