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

#ifndef __NFXPARSER_H__
#define __NFXPARSER_H__

#include "AssetParser.h"

/*
 */
class NFXParser : public IAssetParser {
	
	public:
		static const uint32_t MAX_BLOCKS	 = 2048;
		static const uint32_t MAX_PROPERTIES = 64;

		/*
		 */
		struct Property {
			char key[64];
			char value[128];
		};

		/*
		 */
		struct Block {
			char type[32]; // "controller", "smoke", "force", etc.
			char name[64]; // block identifier
			Property properties[MAX_PROPERTIES];
			uint32_t property_count;
		};

		/*
		 */
		struct EffectsData {
			Block *blocks;
			uint32_t block_count;

			uint32_t controller_count;
			uint32_t effect_count; // non-controller blocks

			char *raw_text;
			size_t raw_text_len;
			size_t file_size;
		};

		NFXParser() { memset(&m, 0, sizeof(m)); }
		~NFXParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NFX"; }
		const EffectsData &getEffectsData() const { return m; }

		// find a block by name (case-insensitive); returns nullptr if not found
		const Block *findBlock(const char *name) const;

		// count blocks of a given type
		uint32_t countBlockType(const char *type) const;

	private:
		EffectsData m;
};

#endif // __NFXPARSER_H__
