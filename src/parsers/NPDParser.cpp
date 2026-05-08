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

/*
 */
int NPDParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 8) {
		free(data);
		return 0;
	}

	pd.file_size = file_size;
	memcpy(pd.header, data, 8);

	// header layout: u16 version (always 2), u16 nodeCount, u32 unknown.
	// the node count at offset 2 tells us how many valid waypoint records
	// follow.  Data beyond those records is the adjacency list.
	uint16_t headerRecCount = FileUtils::readUInt16(data, 2);

	static const size_t REC_SIZE = 26;
	size_t off					 = 8;
	uint32_t maxRecs			 = headerRecCount;
	// clamp to file bounds
	uint32_t fileMaxRecs = (uint32_t)((file_size - 8) / REC_SIZE);
	if(maxRecs > fileMaxRecs) maxRecs = fileMaxRecs;

	pd.records		= (PlacementRecord *)calloc(maxRecs ? maxRecs : 1, sizeof(PlacementRecord));
	pd.record_count = 0;

	while(pd.record_count < maxRecs && off + REC_SIZE <= file_size) {
		PlacementRecord *r = &pd.records[pd.record_count++];
		r->entity_id	   = FileUtils::readUInt16(data, off);
		r->position.x	   = FileUtils::readFloat(data, off + 2);
		r->position.y	   = FileUtils::readFloat(data, off + 6);
		r->position.z	   = FileUtils::readFloat(data, off + 10);
		r->flags		   = FileUtils::readUInt16(data, off + 14);
		r->color		   = FileUtils::readUInt32(data, off + 16);
		memcpy(r->extra, data + off + 20, 6);
		off += REC_SIZE;
	}

	if(off < file_size) {
		pd.trailer_size = file_size - off;
		pd.trailer		= (uint8_t *)malloc(pd.trailer_size);
		memcpy(pd.trailer, data + off, pd.trailer_size);
	}

	free(data);
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
