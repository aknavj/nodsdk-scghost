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

#ifndef __NLTPARSER_H__
#define __NLTPARSER_H__

#include "AssetParser.h"

/*
 */
class NLTParser : public IAssetParser {
		
	public:

		struct TextData {
			char *text; // malloc'd
			size_t text_len;
		};

		NLTParser() { memset(&td, 0, sizeof(td)); }
		~NLTParser() override { free(td.text); }

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NLT"; }
		const TextData &getTextData() const { return td; }

	private:
		TextData td;
};

#endif // __NLTPARSER_H__
