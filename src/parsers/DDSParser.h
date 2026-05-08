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

#ifndef __DDSPARSER_H__
#define __DDSPARSER_H__

#include "AssetParser.h"

class DDSParser : public IAssetParser {

	public:

		struct Header {
			uint32_t size;
			uint32_t flags;
			uint32_t height;
			uint32_t width;
			uint32_t pitch_or_linear_size;
			uint32_t depth;
			uint32_t mip_map_count;
			uint32_t reserved[11];
			uint32_t pixel_format_size;
			uint32_t pixel_format_flags;
			uint32_t four_cc;
			uint32_t rgb_bit_count;
			uint32_t r_bit_mask;
			uint32_t g_bit_mask;
			uint32_t b_bit_mask;
			uint32_t a_bit_mask;
			uint32_t caps1;
			uint32_t caps2;
			uint32_t reserved2[3];
		};

		struct TextureData {
			Header header;
			uint8_t *pixel_data;
			size_t pixel_data_size;
		};

		DDSParser() { memset(&tex, 0, sizeof(tex)); }
		~DDSParser() override { free(tex.pixel_data); }

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;
		const char *getFileType() const override { return "DDS"; }
		const TextureData &getTextureData() const { return tex; }

	private:
		TextureData tex;
		int validate_header(const Header &h) const;
};

#endif // __DDSPARSER_H__
