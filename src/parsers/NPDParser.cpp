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

#include "NPDParser.h"
#include <cmath>

/*
 */
int NPDParser::parse(const char *filePath) {
	size_t file_size = 0;
	uint8_t *data = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data) return reject("cannot read NPD file");
	int ok = parseBuffer(data, file_size);
	free(data);
	return ok;
}

void NPDParser::clear() {
	free(pd.records);
	free(pd.trailer);
	free(pd.links);
	memset(&pd, 0, sizeof(pd));
}

int NPDParser::reject(const char *message) {
	clear();
	last_error = message;
	return 0;
}

int NPDParser::parseBuffer(const uint8_t *data, size_t size) {
	static constexpr size_t record_size = 26;
	last_error = "";
	if(!data || size < 8 || FileUtils::readUInt16(data, 0) != 2)
		return reject("expected NPD version 2 header");
	const uint32_t count = FileUtils::readUInt16(data, 2);
	if(count > (size - 8) / record_size) return reject("truncated NPD waypoint array");
	const size_t end = 8 + static_cast<size_t>(count) * record_size;
	if((size - end) % 6) return reject("NPD trailer is not a complete 6-byte adjacency array");
	for(uint32_t i = 0; i < count; ++i)
		for(size_t j = 0; j < 3; ++j)
			if(!std::isfinite(FileUtils::readFloat(data, 8 + i * record_size + 2 + j * 4)))
				return reject("non-finite NPD waypoint position");
	for(size_t i = end; i < size; i += 6)
		if(FileUtils::readUInt16(data, i) >= count || FileUtils::readUInt16(data, i + 2) >= count)
			return reject("NPD adjacency endpoint outside waypoint array");

	PlacementData parsed{};
	parsed.records = static_cast<PlacementRecord *>(calloc(count ? count : 1, sizeof(PlacementRecord)));
	if(!parsed.records) return reject("out of memory reading NPD records");
	parsed.record_count = count;
	parsed.file_size = size;
	memcpy(parsed.header, data, 8);
	for(uint32_t i = 0; i < count; ++i) {
		const size_t off = 8 + i * record_size;
		auto &r = parsed.records[i];
		r.entity_id = FileUtils::readUInt16(data, off);
		r.position = glm::vec3(FileUtils::readFloat(data, off + 2),
		                      FileUtils::readFloat(data, off + 6), FileUtils::readFloat(data, off + 10));
		r.flags = FileUtils::readUInt16(data, off + 14);
		r.color = FileUtils::readUInt32(data, off + 16);
		memcpy(r.extra, data + off + 20, 6);
	}
	parsed.trailer_size = size - end;
	if(parsed.trailer_size) {
		parsed.trailer = static_cast<uint8_t *>(malloc(parsed.trailer_size));
		if(!parsed.trailer) {
			free(parsed.records);
			return reject("out of memory reading NPD adjacency data");
		}
		memcpy(parsed.trailer, data + end, parsed.trailer_size);
		parsed.link_count = parsed.trailer_size / 6;
		parsed.links = static_cast<PlacementData::LinkRecord *>(calloc(parsed.link_count, sizeof(*parsed.links)));
		if(!parsed.links) {
			free(parsed.records);
			free(parsed.trailer);
			return reject("out of memory reading NPD links");
		}
		for(size_t i = 0; i < parsed.link_count; ++i)
			parsed.links[i] = {FileUtils::readUInt16(data, end + i * 6),
			                  FileUtils::readUInt16(data, end + i * 6 + 2),
			                  FileUtils::readUInt16(data, end + i * 6 + 4)};
	}
	clear();
	pd = parsed;
	return 1;
}

/*
 */
int NPDParser::serialize(const char *filePath) const {

	static const size_t REC_SIZE = 26;
	size_t total_size			 = 8 + (size_t)pd.record_count * REC_SIZE + pd.trailer_size;
	uint8_t *buf				 = (uint8_t *)malloc(total_size);
	if(!buf) return 0;

	memcpy(buf, pd.header, 8);
	size_t off = 8;

	for(uint32_t i = 0; i < pd.record_count; ++i) {
		const PlacementRecord *r = &pd.records[i];
		memcpy(buf + off, &r->entity_id, 2);
		memcpy(buf + off + 2, &r->position.x, 4);
		memcpy(buf + off + 6, &r->position.y, 4);
		memcpy(buf + off + 10, &r->position.z, 4);
		memcpy(buf + off + 14, &r->flags, 2);
		memcpy(buf + off + 16, &r->color, 4);
		memcpy(buf + off + 20, r->extra, 6);
		off += REC_SIZE;
	}

	if(pd.trailer && pd.trailer_size > 0) memcpy(buf + off, pd.trailer, pd.trailer_size);

	int ok = FileUtils::writeBinaryFile(filePath, buf, total_size);
	free(buf);
	return ok;
}
