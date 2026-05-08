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

#include "NCSParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
static inline uint32_t r_u32(const uint8_t *p) {
	uint32_t v;
	memcpy(&v, p, 4);
	return v;
}
/*
 */
static inline uint16_t r_u16(const uint8_t *p) {
	uint16_t v;
	memcpy(&v, p, 2);
	return v;
}
/*
 */
static inline float r_f32(const uint8_t *p) {
	float v;
	memcpy(&v, p, 4);
	return v;
}
/*
 */
static inline void r_str(char *dst, const uint8_t *src, size_t maxLen) {
	size_t i = 0;
	while(i < maxLen - 1 && src[i] != 0) {
		dst[i] = (char)src[i];
		i++;
	}
	dst[i] = '\0';
}

/*
 */
int NCSParser::parse(const char *filePath) {

	FILE *fp = fopen(filePath, "rb");
	if(!fp) return 0;

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(fsize < 24) {
		fclose(fp);
		return 0;
	}
	m.file_size = (size_t)fsize;

	uint8_t *data = (uint8_t *)malloc((size_t)fsize);
	if(!data) {
		fclose(fp);
		return 0;
	}
	if((long)fread(data, 1, (size_t)fsize, fp) != fsize) {
		free(data);
		fclose(fp);
		return 0;
	}
	fclose(fp);

	// HEADER (24 bytes)
	memcpy(&m.version, data, 4);
	if(m.version != 1) {
		free(data);
		return 0;
	}

	m.variant1	  = r_u16(data + 4);
	m.variant2	  = r_u16(data + 6);
	m.variant3	  = r_u16(data + 8);
	m.shot_count  = r_u16(data + 10);
	m.event_count = r_u16(data + 18);
	m.data_size	  = (uint32_t)((size_t)fsize - 24);

	// CAMERA NAD
	// find first actor to determine camera block end
	size_t firstActor = (size_t)fsize;
	{
		size_t sp = 24;
		while(sp + 16 < (size_t)fsize) {
			if(data[sp] == 1 && data[sp + 1] == 0 && data[sp + 2] == 0 && data[sp + 3] == 0 &&
			   r_u32(data + sp + 8) == 5 && sp + 12 < (size_t)fsize && data[sp + 12] >= 0x20 && data[sp + 12] < 0x7F) {
				firstActor = sp;
				break;
			}
			sp++;
		}
	}

	parse_camera_nad(data, 24, firstActor);
	parse_actors(data, 24, (size_t)fsize);

	// GLOBAL EVENTS (after last actor)
	if(m.actor_count > 0) {
		// find end of last actor by re-scanning
		const uint8_t pat[4] = {1, 0, 0, 0};
		size_t lastEnd		 = 0;
		size_t sp			 = 24;
		while(sp + 16 < (size_t)fsize) {
			if(data[sp] == 1 && data[sp + 1] == 0 && data[sp + 2] == 0 && data[sp + 3] == 0 &&
			   r_u32(data + sp + 8) == 5 && sp + 12 < (size_t)fsize && data[sp + 12] >= 0x20 && data[sp + 12] < 0x7F) {
				uint32_t mk2 = r_u32(data + sp + 4);
				size_t end	 = sp + 140;
				if(mk2 == 2 && end + 4 <= (size_t)fsize) {
					uint32_t ac = r_u32(data + end);
					if(ac > 0 && ac <= NCS_MAX_ANIM_STATES) {
						end += 4;
						for(uint32_t s = 0; s < ac; s++) {
							if(end + 4 > (size_t)fsize) break;
							uint32_t st = r_u32(data + end);
							if(st == 2 && end + 44 <= (size_t)fsize)
								end += 44;
							else if(st == 1 && end + 48 <= (size_t)fsize)
								end += 48;
							else
								break;
						}
					}
				}
				if(end > lastEnd) lastEnd = end;
				sp = end;
				continue;
			}
			sp++;
		}
		if(lastEnd > 0 && lastEnd < (size_t)fsize) {
			parse_global_events(data, lastEnd, (size_t)fsize);
		}
	} else {
		// no actors — events follow camera block
		// search for type-3 sound header or type-2 conversation header
		size_t evtStart = firstActor; // same as file_size if no actors
		// look for structured event blocks after the camera region
		for(size_t sp = 24; sp + 8 < (size_t)fsize; sp++) {
			uint32_t t = r_u32(data + sp);
			if(t == 3) {
				uint32_t cnt = r_u32(data + sp + 12);
				if(cnt > 0 && cnt <= NCS_MAX_SOUND_CUES) {
					evtStart = sp;
					break;
				}
			}
		}
		if(evtStart < (size_t)fsize) {
			parse_global_events(data, evtStart, (size_t)fsize);
		}
	}

	free(data);
	return 1;
}

/*
 */
void NCSParser::parse_camera_nad(const uint8_t *data, size_t bodyStart, size_t bodyEnd) {

	m.has_camera_nad = 0;
	memset(m.camera_nad, 0, sizeof(m.camera_nad));

	// scan the camera block for a ".nad" string (camera NAD is early in file)
	const uint8_t nadSig[4] = {'.', 'n', 'a', 'd'};
	for(size_t i = bodyStart; i + 4 <= bodyEnd; i++) {
		if(data[i] == '.' && data[i + 1] == 'n' && data[i + 2] == 'a' && data[i + 3] == 'd') {
			// found .nad — back up to find string start
			size_t start = i;
			while(start > bodyStart && data[start - 1] >= 0x20 && data[start - 1] < 0x7F) start--;
			size_t len = (i + 4) - start;
			if(len >= 5 && len < 32) {
				r_str(m.camera_nad, data + start, 32);
				m.has_camera_nad = 1;
				return;
			}
		}
	}
}

/*
 */
void NCSParser::parse_actors(const uint8_t *data, size_t searchStart, size_t file_size) {

	m.actor_count = 0;
	size_t sp	  = searchStart;

	while(sp + 16 < file_size && m.actor_count < NCS_MAX_ACTORS) {

		// find actor marker: u32(1), u32(N), u32(5) + printable at +12
		if(!(data[sp] == 1 && data[sp + 1] == 0 && data[sp + 2] == 0 && data[sp + 3] == 0 &&
			 r_u32(data + sp + 8) == 5 && sp + 12 < file_size && data[sp + 12] >= 0x20 && data[sp + 12] < 0x7F)) {
			sp++;
			continue;
		}

		size_t aoff = sp;
		if(aoff + 140 > file_size) break;

		CutsceneActor &a = m.actors[m.actor_count];
		memset(&a, 0, sizeof(a));

		a.marker2 = r_u32(data + aoff + 4);

		r_str(a.model_name, data + aoff + 12, 32);
		r_str(a.instance_name, data + aoff + 44, 32);

		// NAD path position varies:
		// u32@+92 == 9999 -> NAD at +96, tail at +128
		// u32@+92 == 0    -> tail at +96, NAD at +108
		// robust: scan actor bytes 76-140 for ".nad" suffix
		memset(a.nad_path, 0, sizeof(a.nad_path));
		for(size_t off = 76; off + 4 <= 140 && aoff + off + 4 <= file_size; off++) {
			if(data[aoff + off] == '.' && data[aoff + off + 1] == 'n' && data[aoff + off + 2] == 'a' &&
			   data[aoff + off + 3] == 'd') {
				// back up to find the string start
				size_t ns = off;
				while(ns > 76 && data[aoff + ns - 1] >= 0x20 && data[aoff + ns - 1] < 0x7F) ns--;
				r_str(a.nad_path, data + aoff + ns, 32);
				break;
			}
		}

		// parse anim-state records if marker2 == 2
		a.anim_state_count = 0;
		size_t spos		   = aoff + 140;

		if(a.marker2 == 2 && spos + 4 <= file_size) {
			uint32_t numStates = r_u32(data + spos);
			spos += 4;

			for(uint32_t s = 0; s < numStates && a.anim_state_count < NCS_MAX_ANIM_STATES; s++) {
				if(spos + 4 > file_size) break;
				uint32_t stype = r_u32(data + spos);

				if(stype == 2 && spos + 44 <= file_size) {
					AnimStateEvent &ev = a.anim_states[a.anim_state_count];
					ev.type			   = 2;
					ev.frame_time	   = r_f32(data + spos + 4);
					r_str(ev.name, data + spos + 12, 32);
					a.anim_state_count++;
					spos += 44;
				} else if(stype == 1 && spos + 48 <= file_size) {
					AnimStateEvent &ev = a.anim_states[a.anim_state_count];
					ev.type			   = 1;
					ev.frame_time	   = r_f32(data + spos + 4);
					r_str(ev.name, data + spos + 16, 32);
					a.anim_state_count++;
					spos += 48;
				} else {
					break;
				}
			}
		}

		m.actor_count++;
		sp = spos;
	}
}

/*
 */
void NCSParser::parse_global_events(const uint8_t *data, size_t evtStart, size_t file_size) {

	m.sound_cue_count	 = 0;
	m.conversation_count = 0;

	if(evtStart >= file_size) return;

	// sound cues: type-3 header (20 bytes) + N × 76-byte records
	for(size_t i = evtStart; i + 20 < file_size; i++) {
		if(r_u32(data + i) != 3) continue;

		uint32_t count = r_u32(data + i + 12);
		if(count == 0 || count > NCS_MAX_SOUND_CUES) continue;

		// validate: check if the first record after header has a plausible name
		size_t recStart = i + 20;
		if(recStart + 76 > file_size) continue;
		if(data[recStart + 8] < 0x20 || data[recStart + 8] >= 0x7F) continue;

		// parse sound cue records (76 bytes each)
		for(uint32_t c = 0; c < count && m.sound_cue_count < NCS_MAX_SOUND_CUES; c++) {
			size_t roff = recStart + (size_t)c * 76;
			if(roff + 76 > file_size) break;

			SoundCueEvent &sc = m.sound_cues[m.sound_cue_count];
			memset(&sc, 0, sizeof(sc));
			sc.frame_time = r_f32(data + roff);
			r_str(sc.name, data + roff + 8, 32);
			sc.volume = r_f32(data + roff + 48); // volume at record+48
			if(sc.volume <= 0.0f || sc.volume > 10.0f) sc.volume = 1.0f;
			m.sound_cue_count++;
		}
		break; // found and parsed sound cues
	}

	// conversations: type-2 header (u32(2), u32(count), u32(0))
	// each record: char[32] name + 16 bytes (including f32 frame_time at +44)
	for(size_t i = evtStart; i + 12 < file_size; i++) {
		if(r_u32(data + i) != 2) continue;
		uint32_t count = r_u32(data + i + 4);
		if(count == 0 || count > NCS_MAX_CONVERSATIONS) continue;

		// validate: name at +12 should start with 'c' (conversation IDs)
		size_t recStart = i + 12;
		if(recStart + 48 > file_size) continue;
		if(data[recStart] != 'c') continue;

		for(uint32_t c = 0; c < count && m.conversation_count < NCS_MAX_CONVERSATIONS; c++) {
			size_t roff = recStart + (size_t)c * 48;
			if(roff + 48 > file_size) break;

			ConversationEvent &ce = m.conversations[m.conversation_count];
			memset(&ce, 0, sizeof(ce));
			r_str(ce.name, data + roff, 32);

			// frame time at record + 44
			ce.frame_time = r_f32(data + roff + 44);
			if(ce.frame_time < 0.0f || ce.frame_time > 100000.0f) ce.frame_time = 0.0f;
			m.conversation_count++;
		}
		break;
	}
}

/*
 */
int NCSParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}
