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

#ifndef __ASSETPARSER_H__
#define __ASSETPARSER_H__

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
class IAssetParser {

	public:
		virtual ~IAssetParser()							  = default;
		virtual int parse(const char *filePath)			  = 0;
		virtual int serialize(const char *filePath) const = 0;
		virtual const char *getFileType() const			  = 0;
};

/*
 */
class FileUtils {

	public:
		// read entire file into malloc'd buffer; caller must free(). returns nullptr on failure.
		static uint8_t *readBinaryFile(const char *filePath, size_t *outSize);

		// write buffer to binary file; returns true on success.
		static int writeBinaryFile(const char *filePath, const uint8_t *data, size_t size);

		// get lowercased extension into outExt (maxLen includes '\0').
		static void getFileExtension(const char *filePath, char *outExt, size_t maxLen);

		// read little-endian primitives from raw buffer
		static uint32_t readUInt32(const uint8_t *data, size_t offset);
		static uint16_t readUInt16(const uint8_t *data, size_t offset);
		static float readFloat(const uint8_t *data, size_t offset);
};

#endif // __ASSETPARSER_H__
