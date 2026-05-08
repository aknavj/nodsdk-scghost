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

#ifndef __NCOPARSER_H__
#define __NCOPARSER_H__

#include "AssetParser.h"

/*
 */
class NCOParser : public IAssetParser {

	public:

		static const uint32_t MAX_LINES	   = 32;
		static const uint32_t MAX_SPEAKERS = 8;

		/*
		 */
		struct DialogueLine {
			uint32_t duration_ms; // display / timing duration in milliseconds
			uint32_t speaker_id; // numeric speaker index (per-file)
			char speaker_name[64]; // extracted from (Name) prefix, or empty
			char text[512]; // dialogue text (speaker prefix stripped)
		};

		/*
		 */
		struct ConversationData {
			uint32_t line_count;
			uint32_t speaker_count; // number of unique speaker IDs seen
			DialogueLine lines[MAX_LINES];
			char speaker_names[MAX_SPEAKERS][64]; // unique names by speaker ID
			size_t file_size;
		};

		NCOParser() { memset(&m, 0, sizeof(m)); }
		~NCOParser() override = default;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NCO"; }
		const ConversationData &getConversationData() const { return m; }

	private:
		ConversationData m;

		// parse one text line into a DialogueLine; returns 1 on success.
		static int parseLine(const char *line, DialogueLine *out);
};

#endif // __NCOPARSER_H__
