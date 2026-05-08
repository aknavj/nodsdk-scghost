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

#include "DDSParser.h"

/*
 */
int DDSParser::parse(const char *filePath) {

	size_t file_size  = 0;
	uint8_t *fileData = FileUtils::readBinaryFile(filePath, &file_size);
	if(!fileData || file_size < 4 + sizeof(Header)) {
		free(fileData);
		return 0;
	}

	uint32_t magic;
	memcpy(&magic, fileData, 4);
	if(magic != 0x20534444) { // "DDS "
		free(fileData);
		return 0;
	}

	memcpy(&tex.header, fileData + 4, sizeof(Header));
	if(!validate_header(tex.header)) {
		free(fileData);
		return 0;
	}

	size_t pixStart		= 4 + sizeof(Header);
	tex.pixel_data_size = file_size - pixStart;
	tex.pixel_data		= (uint8_t *)malloc(tex.pixel_data_size);
	memcpy(tex.pixel_data, fileData + pixStart, tex.pixel_data_size);

	free(fileData);
	return 1;
}

/*
 */
int DDSParser::serialize(const char *filePath) const {

	size_t total_size = 4 + sizeof(Header) + tex.pixel_data_size;
	uint8_t *buf	  = (uint8_t *)malloc(total_size);
	if(!buf) return 0;

	uint32_t magic = 0x20534444;
	memcpy(buf, &magic, 4);
	memcpy(buf + 4, &tex.header, sizeof(Header));
	if(tex.pixel_data && tex.pixel_data_size > 0) memcpy(buf + 4 + sizeof(Header), tex.pixel_data, tex.pixel_data_size);

	int ok = FileUtils::writeBinaryFile(filePath, buf, total_size);
	free(buf);
	return ok;
}

/*
 */
int DDSParser::validate_header(const Header &h) const {

	if(h.size != 124) return 0;
	if(h.width == 0 || h.height == 0) return 0;
	return 1;
}
