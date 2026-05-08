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

#include "AssetParser.h"

/*
 */
uint8_t *FileUtils::readBinaryFile(const char *filePath, size_t *outSize) {

	*outSize = 0;
	FILE *f	 = fopen(filePath, "rb");
	if(!f) return nullptr;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if(len <= 0) {
		fclose(f);
		return nullptr;
	}

	uint8_t *buf = (uint8_t *)malloc((size_t)len);
	if(!buf) {
		fclose(f);
		return nullptr;
	}

	size_t rd = fread(buf, 1, (size_t)len, f);
	fclose(f);

	if(rd != (size_t)len) {
		free(buf);
		return nullptr;
	}

	*outSize = (size_t)len;
	return buf;
}

/*
 */
int FileUtils::writeBinaryFile(const char *filePath, const uint8_t *data, size_t size) {

	FILE *f = fopen(filePath, "wb");
	if(!f) return 0;

	size_t wr = fwrite(data, 1, size, f);
	fclose(f);
	return wr == size;
}

/*
 */
void FileUtils::getFileExtension(const char *filePath, char *outExt, size_t maxLen) {

	outExt[0]		= '\0';
	const char *dot = strrchr(filePath, '.');
	if(!dot || dot == filePath) return;

	++dot; // skip '.'
	size_t i = 0;
	for(; dot[i] && i + 1 < maxLen; ++i) {
		char c = dot[i];
		if(c >= 'A' && c <= 'Z') c += 32; // tolower
		outExt[i] = c;
	}
	outExt[i] = '\0';
}

/*
 */
uint32_t FileUtils::readUInt32(const uint8_t *data, size_t offset) {

	uint32_t v;
	memcpy(&v, data + offset, 4);
	return v;
}

/*
 */
uint16_t FileUtils::readUInt16(const uint8_t *data, size_t offset) {

	uint16_t v;
	memcpy(&v, data + offset, 2);
	return v;
}

/*
 */
float FileUtils::readFloat(const uint8_t *data, size_t offset) {

	float v;
	memcpy(&v, data + offset, 4);
	return v;
}
