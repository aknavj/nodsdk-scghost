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

#ifndef __TGAPARSER_H__
#define __TGAPARSER_H__

#include "AssetParser.h"

/*
 */
class TGAParser : public IAssetParser {

	public:

		/*
		 */
		enum ColorMapType { 
			NO_COLOR_MAP = 0, 
			COLOR_MAP = 1 
		};
		
		/*
		 */
		enum ImageType {
			NO_IMAGE			   = 0,
			UNCOMPRESSED_COLOR_MAP = 1,
			UNCOMPRESSED_RGB	   = 2,
			UNCOMPRESSED_GRAYSCALE = 3,
			RLE_COLOR_MAP		   = 9,
			RLE_RGB				   = 10,
			RLE_GRAYSCALE		   = 11
		};

		/*
		 */
		struct Header {
			uint8_t id_length;
			uint8_t color_map_type;
			uint8_t image_type;
			uint16_t color_map_first_entry;
			uint16_t color_map_length;
			uint8_t color_map_bit_depth;
			uint16_t x_origin;
			uint16_t y_origin;
			uint16_t width;
			uint16_t height;
			uint8_t bit_depth;
			uint8_t image_descriptor;
		};

		/*
		 */
		struct ImageData {
			Header header;
			char image_id[256];
			uint8_t *pixel_data;
			size_t pixel_data_size;
		};

		TGAParser() { 
			memset(&img, 0, sizeof(img)); 
		}

		~TGAParser() override { 
			free(img.pixel_data); 
		}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "TGA"; }
		const ImageData &getImageData() const { return img; }

	private:
		ImageData img;
		int validate_header(const Header &h) const;
		uint8_t *decompress_rle(const uint8_t *compressed, size_t compSize, uint32_t pixelCount, uint8_t bpp,
								size_t *outSize);
};

#endif // __TGAPARSER_H__
