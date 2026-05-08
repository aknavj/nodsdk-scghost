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

#ifndef __XSBPARSER_H__
#define __XSBPARSER_H__

#include "AssetParser.h"
#include <cstdint>
#include <vector>
#include <string>

/*
 */
class XSBParser : public IAssetParser {
		
	public:
	
		static const uint32_t XSB_MAGIC = 0x4B424453; // "SDBK" little-endian

		/*
		 */
		// single wave reference resolved from a sound entry
		struct WaveRef {
			uint16_t wave_index; // track index in the XWB wave bank
			uint16_t wave_bank_index; // index into wave_bank_names[]
		};

		/*
		 */
		// named cue and associated wave references
		struct CueMapping {
			std::string name; // cue name (from hash/name entry)
			uint16_t category; // category id from cue entry
			std::vector<WaveRef> waves; // wave variations for this cue
		};

		/*
		 */
		struct SoundBankData {
			uint16_t version;
			char bank_name[17]; // 16 chars + null
			uint16_t simple_cue_count;
			uint16_t complex_cue_count;
			uint16_t cue_name_count; // number of named cues
			uint16_t wave_bank_count;

			// offsets (raw from header, for debugging)
			uint32_t wave_bank_name_offset;
			uint32_t sound_entry_offset;
			uint32_t cue_hash_offset;
			uint32_t cue_name_end_offset; // usually == file size

			uint32_t total_cue_count;

			std::vector<std::string> wave_bank_names; // 16-char names

			// cue names (those that have names)
			std::vector<std::string> cue_names;

			std::vector<CueMapping> cue_mappings;

			size_t file_size;
		};

		XSBParser() {}
		~XSBParser() override {}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override { return 0; }

		const char *getFileType() const override { return "XSB"; }
		const SoundBankData &getSoundBankData() const { return m_data; }

	private:
		SoundBankData m_data;
};

#endif // __XSBPARSER_H__
