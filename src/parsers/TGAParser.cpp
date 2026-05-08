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

#include "TGAParser.h"

/*
 */
int TGAParser::parse(const char *filePath) {

	size_t file_size  = 0;
	uint8_t *fileData = FileUtils::readBinaryFile(filePath, &file_size);
	if(!fileData || file_size < 18) {
		free(fileData);
		return 0;
	}

	size_t off						 = 0;
	img.header.id_length			 = fileData[off++];
	img.header.color_map_type		 = fileData[off++];
	img.header.image_type			 = fileData[off++];
	img.header.color_map_first_entry = FileUtils::readUInt16(fileData, off);
	off += 2;
	img.header.color_map_length = FileUtils::readUInt16(fileData, off);
	off += 2;
	img.header.color_map_bit_depth = fileData[off++];
	img.header.x_origin			   = FileUtils::readUInt16(fileData, off);
	off += 2;
	img.header.y_origin = FileUtils::readUInt16(fileData, off);
	off += 2;
	img.header.width = FileUtils::readUInt16(fileData, off);
	off += 2;
	img.header.height = FileUtils::readUInt16(fileData, off);
	off += 2;
	img.header.bit_depth		= fileData[off++];
	img.header.image_descriptor = fileData[off++];

	if(!validate_header(img.header)) {
		free(fileData);
		return 0;
	}

	img.image_id[0] = '\0';
	if(img.header.id_length > 0) {
		if(off + img.header.id_length > file_size) {
			free(fileData);
			return 0;
		}
		size_t cpLen = img.header.id_length < 255 ? img.header.id_length : 255;
		memcpy(img.image_id, fileData + off, cpLen);
		img.image_id[cpLen] = '\0';
		off += img.header.id_length;
	}

	if(img.header.color_map_type == COLOR_MAP) {
		uint32_t cmBytes = img.header.color_map_length * (img.header.color_map_bit_depth / 8);
		off += cmBytes;
	}

	uint32_t pixelCount	  = (uint32_t)img.header.width * img.header.height;
	uint8_t bpp			  = img.header.bit_depth / 8;
	uint32_t expectedSize = pixelCount * bpp;

	if(img.header.image_type == RLE_RGB || img.header.image_type == RLE_COLOR_MAP ||
	   img.header.image_type == RLE_GRAYSCALE) {
		img.pixel_data = decompress_rle(fileData + off, file_size - off, pixelCount, bpp, &img.pixel_data_size);
	} else {
		if(off + expectedSize > file_size) {
			free(fileData);
			return 0;
		}
		img.pixel_data_size = expectedSize;
		img.pixel_data		= (uint8_t *)malloc(expectedSize);
		memcpy(img.pixel_data, fileData + off, expectedSize);
	}

	free(fileData);
	return 1;
}

/*
 */
int TGAParser::serialize(const char *filePath) const {

	size_t idLen	  = strlen(img.image_id);
	size_t total_size = 18 + idLen + img.pixel_data_size;
	uint8_t *buf	  = (uint8_t *)malloc(total_size);
	if(!buf) return 0;

	size_t off = 0;
	buf[off++] = (uint8_t)idLen;
	buf[off++] = img.header.color_map_type;
	buf[off++] = img.header.image_type;
	memcpy(buf + off, &img.header.color_map_first_entry, 2);
	off += 2;
	memcpy(buf + off, &img.header.color_map_length, 2);
	off += 2;
	buf[off++] = img.header.color_map_bit_depth;
	memcpy(buf + off, &img.header.x_origin, 2);
	off += 2;
	memcpy(buf + off, &img.header.y_origin, 2);
	off += 2;
	memcpy(buf + off, &img.header.width, 2);
	off += 2;
	memcpy(buf + off, &img.header.height, 2);
	off += 2;
	buf[off++] = img.header.bit_depth;
	buf[off++] = img.header.image_descriptor;

	if(idLen > 0) {
		memcpy(buf + off, img.image_id, idLen);
		off += idLen;
	}
	if(img.pixel_data && img.pixel_data_size > 0) memcpy(buf + off, img.pixel_data, img.pixel_data_size);

	int ok = FileUtils::writeBinaryFile(filePath, buf, total_size);
	free(buf);
	return ok;
}

/*
 */
int TGAParser::validate_header(const Header &h) const {

	if(h.width == 0 || h.height == 0) return 0;
	if(h.bit_depth != 8 && h.bit_depth != 16 && h.bit_depth != 24 && h.bit_depth != 32) return 0;
	if(h.image_type != UNCOMPRESSED_RGB && h.image_type != UNCOMPRESSED_GRAYSCALE &&
	   h.image_type != UNCOMPRESSED_COLOR_MAP && h.image_type != RLE_RGB && h.image_type != RLE_GRAYSCALE &&
	   h.image_type != RLE_COLOR_MAP)
		return 0;
	return 1;
}

/*
 */
uint8_t *TGAParser::decompress_rle(const uint8_t *compressed, size_t compSize, uint32_t pixelCount, uint8_t bpp,
								   size_t *outSize) {
	size_t resultCap = (size_t)pixelCount * bpp;
	uint8_t *result	 = (uint8_t *)malloc(resultCap);
	size_t rOff = 0, cOff = 0;
	uint32_t pixelsRead = 0;

	while(pixelsRead < pixelCount && cOff < compSize) {
		uint8_t pktHdr = compressed[cOff++];
		uint8_t count  = (pktHdr & 0x7F) + 1;

		if(pktHdr & 0x80) {
			if(cOff + bpp > compSize) break;
			for(uint8_t i = 0; i < count && pixelsRead < pixelCount; ++i) {
				memcpy(result + rOff, compressed + cOff, bpp);
				rOff += bpp;
				++pixelsRead;
			}
			cOff += bpp;
		} else {
			for(uint8_t i = 0; i < count && pixelsRead < pixelCount; ++i) {
				if(cOff + bpp > compSize) break;
				memcpy(result + rOff, compressed + cOff, bpp);
				rOff += bpp;
				cOff += bpp;
				++pixelsRead;
			}
		}
	}

	*outSize = rOff;
	return result;
}
