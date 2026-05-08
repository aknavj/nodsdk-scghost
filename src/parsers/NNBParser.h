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

#ifndef __NNBPARSER_H__
#define __NNBPARSER_H__

#include "AssetParser.h"

/*
 */
class NNBParser : public IAssetParser {

	public:

		static constexpr uint32_t MAX_ANIMS = 512;

		struct AnimEntry {
			char name[64]; // .nad filename
			size_t offset; // byte offset where name was found
		};

		struct BundleData {
			uint32_t version;
			uint32_t anim_count;
			uint32_t data_size;
			uint32_t names_found; // how many .nad names were located
			size_t file_size;
			AnimEntry anims[MAX_ANIMS];
		};

		NNBParser() { memset(&m, 0, sizeof(m)); }
		~NNBParser() override = default;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NNB"; }
		const BundleData &getBundleData() const { return m; }

	private:
		BundleData m;
};

#endif // __NNBPARSER_H__
