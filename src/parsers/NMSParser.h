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

#ifndef __NMSPARSER_H__
#define __NMSPARSER_H__

#include "AssetParser.h"

/*
 */
class NMSParser : public IAssetParser {

	public:
		static const uint32_t MAX_FLAGS = 8;

		/*
		*/
		struct MotionEntry {
			char motion_name[128];
			char animation_file[128];
			char flags[MAX_FLAGS][64];
			uint32_t flag_count;
		};

		/*
		*/
		struct MotionSetData {
			char *header_comment; // malloc'd
			size_t header_comment_len;
			MotionEntry *entries; // malloc'd array
			uint32_t entry_count;
		};

		NMSParser() { 
			memset(&ms, 0, sizeof(ms)); 
		}

		~NMSParser() override {
			free(ms.header_comment);
			free(ms.entries);
		}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NMS"; }
		const MotionSetData &getMotionSetData() const { return ms; }

	private:
		MotionSetData ms;
};

#endif // __NMSPARSER_H__
