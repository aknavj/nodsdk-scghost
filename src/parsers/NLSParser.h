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

#ifndef __NLSPARSER_H__
#define __NLSPARSER_H__

#include "AssetParser.h"

/*
*/
class NLSParser : public IAssetParser {
		
	public:
		static const uint32_t MAX_ENTRIES = 4096;

		/*
		 */
		struct StringEntry {
			char key[128];
			char value[512];
		};

		/*
		 */
		struct LocalizationData {
			StringEntry *entries;
			uint32_t entry_count;
			char *raw_text;
			size_t raw_text_len;
			size_t file_size;
		};

		NLSParser() { memset(&m, 0, sizeof(m)); }
		~NLSParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NLS"; }
		const LocalizationData &getLocalizationData() const { return m; }

		// look up a string by key (case-insensitive); returns nullptr if not found
		const char *lookupString(const char *key) const;

	private:
		LocalizationData m;
};

#endif // __NLSPARSER_H__
