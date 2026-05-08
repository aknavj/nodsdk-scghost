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

#include "XPRParser.h"

/*
 */
void XPRParser::read_null_str(const uint8_t *data, size_t data_size, size_t offset, char *out, size_t outMax) {

	out[0] = '\0';
	if(offset >= data_size) return;
	size_t i = 0;
	while(offset + i < data_size && data[offset + i] != 0 && i + 1 < outMax) {
		out[i] = (char)data[offset + i];
		++i;
	}
	out[i] = '\0';
}

/*
 */
int XPRParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 16) {
		free(data);
		return 0;
	}

	if(data[0] != 'X' || data[1] != 'P' || data[2] != 'R' || data[3] != '1') {
		free(data);
		return 0;
	}

	xpr.file_size	   = file_size;
	xpr.magic		   = FileUtils::readUInt32(data, 0);
	xpr.total_size	   = FileUtils::readUInt32(data, 4);
	xpr.header_size	   = FileUtils::readUInt32(data, 8);
	xpr.resource_count = FileUtils::readUInt32(data, 12);

	if(xpr.header_size > file_size || xpr.resource_count > 10000) {
		xpr.raw_data	  = data;
		xpr.raw_data_size = file_size;
		return 1;
	}

	xpr.resource_array_count = xpr.resource_count;
	xpr.resources			 = (XPRResource *)calloc(xpr.resource_count, sizeof(XPRResource));

	size_t tableOff = 16;
	for(uint32_t i = 0; i < xpr.resource_count; ++i) {
		size_t entOff = tableOff + (size_t)i * 8;
		if(entOff + 8 > xpr.header_size) break;

		XPRResource *r		 = &xpr.resources[i];
		r->name_offset		 = FileUtils::readUInt32(data, entOff);
		r->descriptor_offset = FileUtils::readUInt32(data, entOff + 4);

		if(r->name_offset < xpr.header_size) read_null_str(data, file_size, r->name_offset, r->name, sizeof(r->name));

		if(r->descriptor_offset + 20 <= xpr.header_size) {
			r->descriptor.common	  = FileUtils::readUInt32(data, r->descriptor_offset);
			r->descriptor.data_offset = FileUtils::readUInt32(data, r->descriptor_offset + 4);
			r->descriptor.lock		  = FileUtils::readUInt32(data, r->descriptor_offset + 8);
			r->descriptor.format	  = FileUtils::readUInt32(data, r->descriptor_offset + 12);
			r->descriptor.size		  = FileUtils::readUInt32(data, r->descriptor_offset + 16);
		}
	}

	if(xpr.header_size < file_size) {
		xpr.texture_data_size = file_size - xpr.header_size;
		xpr.texture_data	  = (uint8_t *)malloc(xpr.texture_data_size);
		memcpy(xpr.texture_data, data + xpr.header_size, xpr.texture_data_size);
	}

	xpr.raw_data	  = data;
	xpr.raw_data_size = file_size;
	return 1;
}

/*
 */
int XPRParser::serialize(const char *filePath) const {

	return FileUtils::writeBinaryFile(filePath, xpr.raw_data, xpr.raw_data_size);
}
