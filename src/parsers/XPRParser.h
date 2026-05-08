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

#ifndef __XPRPARSER_H__
#define __XPRPARSER_H__

#include "AssetParser.h"

/*
*/
class XPRParser : public IAssetParser {
		
	public:

		/*
		 */
		struct TextureDescriptor {
			uint32_t common;
			uint32_t data_offset;
			uint32_t lock;
			uint32_t format;
			uint32_t size;
		};

		/*
		 */
		struct XPRResource {
			char name[128];
			uint32_t name_offset;
			uint32_t descriptor_offset;
			TextureDescriptor descriptor;
		};

		/*
		 */
		struct XPRData {
			uint32_t magic;
			uint32_t total_size;
			uint32_t header_size;
			uint32_t resource_count;

			XPRResource *resources;
			uint32_t resource_array_count;
			uint8_t *texture_data;
			size_t texture_data_size;
			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		XPRParser() { 
			memset(&xpr, 0, sizeof(xpr)); 
		}

		~XPRParser() override {
			free(xpr.resources);
			free(xpr.texture_data);
			free(xpr.raw_data);
		}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "XPR"; }
		const XPRData &getXPRData() const { return xpr; }

	private:
		XPRData xpr;
		static void read_null_str(const uint8_t *data, size_t data_size, size_t offset, char *out, size_t outMax);
};

#endif // __XPRPARSER_H__
