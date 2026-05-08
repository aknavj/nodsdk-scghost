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

#ifndef __XWBPARSER_H__
#define __XWBPARSER_H__

#include "AssetParser.h"
#include <cstdint>

/*
 */
class XWBParser : public IAssetParser {

	public:
		static const uint32_t MAX_ENTRIES = 4096;

		enum Codec { 
			CODEC_PCM = 0, 
			CODEC_XBOX_ADPCM = 1,
			CODEC_WMA = 2, 
			CODEC_XMA = 3 
		};

		/*
		 */
		struct WaveEntry {
			uint32_t codec;
			uint32_t channels;
			uint32_t sample_rate;
			uint32_t block_align;
			uint32_t bits_per_sample;
			uint32_t data_offset; // relative to wave data segment
			uint32_t data_length;
			uint32_t loop_start;
			uint32_t loop_length;
			uint32_t duration; // in samples (from flagsAndDuration)
		};

		/*
		 */
		struct WaveBankData {
			uint32_t version;
			uint32_t flags;
			char bank_name[64];
			uint32_t entry_count;
			uint32_t alignment;
			WaveEntry *entries;

			// pointer into raw_data for wave data segment
			const uint8_t *wave_data;
			size_t wave_data_size;

			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		XWBParser() { memset(&m_data, 0, sizeof(m_data)); }
		~XWBParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "XWB"; }
		const WaveBankData &getWaveBankData() const { return m_data; }

		// decode entry to 16-bit PCM; returns malloc'd buffer (caller frees).
		// outFrameCount = per-channel sample frames; nullptr for unsupported codecs.
		int16_t *decodeToPCM16(uint32_t entryIndex, uint32_t *outFrameCount, uint32_t *outChannels,
							uint32_t *outSampleRate) const;

		// return a human-readable codec name.
		static const char *codecName(uint32_t codec);

		// duration in seconds for a given entry.
		float getEntryDuration(uint32_t entryIndex) const;

	private:
		WaveBankData m_data;

		// xbox IMA ADPCM decoder
		static int16_t *decode_xbox_adpcm(const uint8_t *data, size_t dataLen, uint32_t channels, uint32_t *outFrameCount);
};

#endif // __XWBPARSER_H__
