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

#include "XSBParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
static inline uint16_t rd16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/*
 */
static inline uint32_t rd32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*
 */
static bool resolveWaveRef(const uint8_t *data, size_t file_size, uint32_t soundOff, uint32_t soundEnd, uint32_t v0,
						   uint16_t &outWave, uint16_t &outBank) {
	// sub-type A: v0 is an offset into the sound data region
	if(soundOff <= v0 && v0 < soundEnd) {
		if(v0 + 3 >= file_size) return false;
		uint8_t trkCnt = data[v0];
		if(trkCnt < 1 || trkCnt > 16) return false;
		uint16_t trkOff = rd16(data + v0 + 1);
		if(trkOff + 12 > file_size) return false;
		outWave = rd16(data + trkOff + 8);
		outBank = rd16(data + trkOff + 10);
		return true;
	}

	// sub-type B: direct wave reference (u16 wave + u16 bank)
	outWave = (uint16_t)(v0 & 0xFFFF);
	outBank = (uint16_t)((v0 >> 16) & 0xFFFF);
	return true;
}

/*
 */
int XSBParser::parse(const char *filePath) {
	FILE *f = fopen(filePath, "rb");
	if(!f) return 0;

	fseek(f, 0, SEEK_END);
	size_t file_size = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);

	// need at least 0x38 bytes for the header
	if(file_size < 0x38) {
		fclose(f);
		return 0;
	}

	uint8_t *data = (uint8_t *)malloc(file_size);
	if(!data) {
		fclose(f);
		return 0;
	}
	if(fread(data, 1, file_size, f) != file_size) {
		free(data);
		fclose(f);
		return 0;
	}
	fclose(f);

	// validate magic
	uint32_t magic = rd32(data + 0x00);
	if(magic != XSB_MAGIC) {
		free(data);
		return 0;
	}

	// parse header
	m_data.file_size			 = file_size;
	m_data.version				 = rd16(data + 0x04);
	m_data.wave_bank_name_offset = rd32(data + 0x08);
	m_data.sound_entry_offset	 = rd32(data + 0x0C);
	m_data.cue_hash_offset		 = rd32(data + 0x10);
	m_data.cue_name_end_offset	 = rd32(data + 0x14);
	m_data.simple_cue_count		 = rd16(data + 0x1A);
	m_data.complex_cue_count	 = rd16(data + 0x1C);
	m_data.cue_name_count		 = rd16(data + 0x1E);
	m_data.wave_bank_count		 = rd16(data + 0x22);
	m_data.total_cue_count		 = m_data.simple_cue_count + m_data.complex_cue_count;

	// bank name (16 chars, null-padded)
	memset(m_data.bank_name, 0, sizeof(m_data.bank_name));
	memcpy(m_data.bank_name, data + 0x28, 16);
	m_data.bank_name[16] = '\0';

	// wave bank names
	m_data.wave_bank_names.clear();
	uint32_t wbOff = m_data.wave_bank_name_offset;
	for(uint16_t i = 0; i < m_data.wave_bank_count; i++) {
		uint32_t nameOff = wbOff + (uint32_t)i * 16;
		if(nameOff + 16 > file_size) break;

		char name[17] = {};
		memcpy(name, data + nameOff, 16);
		name[16] = '\0';
		m_data.wave_bank_names.push_back(std::string(name));
	}

	// cue name strings
	// cue names are packed null-terminated ASCII strings located
	// immediately after the wave bank names, up to cue_hash_offset.
	m_data.cue_names.clear();
	uint32_t nameStart = wbOff + (uint32_t)m_data.wave_bank_count * 16;
	uint32_t nameEnd   = m_data.cue_hash_offset;

	if(nameEnd > file_size) nameEnd = (uint32_t)file_size;
	if(nameStart > nameEnd) nameStart = nameEnd;

	uint32_t pos = nameStart;
	while(pos < nameEnd) {
		// find end of current string
		uint32_t strEnd = pos;
		while(strEnd < nameEnd && data[strEnd] != '\0') strEnd++;

		if(strEnd > pos) {
			// validate it's printable ASCII
			bool valid = true;
			for(uint32_t c = pos; c < strEnd; c++) {
				if(data[c] < 0x20 || data[c] > 0x7E) {
					valid = false;
					break;
				}
			}
			if(valid) {
				m_data.cue_names.push_back(std::string((const char *)data + pos, strEnd - pos));
			} else {
				break; // hit non-ASCII data, stop
			}
		}
		pos = strEnd + 1; // skip null terminator
	}

	// cue-to-wave mappings
	// entry region: 0x38 to wave_bank_name_offset, each entry 20 bytes.
	// type 1 entries (sentinel 0x07FFFFFF at byte 16) define named cues.
	// type 2 entries that follow a Type 1 define its wave variations.
	m_data.cue_mappings.clear();

	uint32_t entryRegionEnd = m_data.wave_bank_name_offset;
	uint32_t entry_count	= (entryRegionEnd - 0x38) / 20;
	uint32_t sndOff			= m_data.sound_entry_offset;
	uint32_t sndEnd			= m_data.cue_name_end_offset;

	CueMapping *currentCue = nullptr;

	for(uint32_t e = 0; e < entry_count; e++) {
		uint32_t base = 0x38 + e * 20;
		if(base + 20 > file_size) break;

		uint32_t v0	 = rd32(data + base);
		uint32_t v16 = rd32(data + base + 16);
		bool isType1 = (v16 == 0x07FFFFFF);

		if(isType1) {
			// type 1: cue name entry
			uint16_t category = rd16(data + base + 2);
			uint32_t nameOff  = rd32(data + base + 4);
			uint32_t soundRef = rd32(data + base + 8);

			// resolve the cue name from the name offset
			std::string cueName;
			if(nameOff > 0 && nameOff < file_size) {
				uint32_t nEnd = nameOff;
				while(nEnd < file_size && data[nEnd] != '\0') nEnd++;
				if(nEnd > nameOff) cueName.assign((const char *)data + nameOff, nEnd - nameOff);
			}

			CueMapping cm;
			cm.name		= cueName;
			cm.category = category;

			// if the cue points directly to sound data, try to resolve it.
			// this handles variation tables where the cue entry itself
			// references a sound.
			if(soundRef != 0xFFFFFFFF && sndOff <= soundRef && soundRef < sndEnd) {
				// todo
			}

			m_data.cue_mappings.push_back(cm);
			currentCue = &m_data.cue_mappings.back();
		} else {
			// type 2: sound/wave entry
			if(!currentCue) {
				// orphan sound entry before any cue — create unnamed cue
				CueMapping cm;
				cm.name		= "";
				cm.category = 0;
				m_data.cue_mappings.push_back(cm);
				currentCue = &m_data.cue_mappings.back();
			}

			uint16_t wi = 0, wb = 0;
			if(resolveWaveRef(data, file_size, sndOff, sndEnd, v0, wi, wb)) {
				WaveRef wr;
				wr.wave_index	   = wi;
				wr.wave_bank_index = wb;
				currentCue->waves.push_back(wr);
			}
		}
	}

	free(data);
	return 1;
}
