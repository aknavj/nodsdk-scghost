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

#include "NCEParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

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
int NCEParser::parse(const char *filePath) {

	FILE *fp = fopen(filePath, "rb");
	if(!fp) return 0;

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(fsize < 9) {
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

	memcpy(&m.version, data, 4);
	if(m.version != 1) {
		free(data);
		return 0;
	}

	uint32_t mapNameLen = r_u32(data + 5);
	if(9 + mapNameLen > (size_t)fsize) {
		free(data);
		return 0;
	}

	size_t copyLen = mapNameLen < sizeof(m.map_name) - 1 ? mapNameLen : sizeof(m.map_name) - 1;
	memcpy(m.map_name, data + 9, copyLen);
	m.map_name[copyLen] = '\0';

	// TIMING BLOCK (40 bytes)
	size_t tpos = 9 + mapNameLen;
	if(tpos + 40 > (size_t)fsize) {
		m.data_size = 0;
		free(data);
		return 1;
	}

	m.num_frames  = r_u32(data + tpos);
	m.time_scale  = r_f32(data + tpos + 4);
	m.shot_count  = r_u16(data + tpos + 26);
	m.event_count = r_u16(data + tpos + 34);
	m.data_size	  = (uint32_t)((size_t)fsize - (tpos + 40));

	size_t camStart = tpos + 40;

	parse_camera(data, camStart, (size_t)fsize);

	size_t actorsSearchStart = camStart + (m.camera.has_camera_nad ? 143 : 111);
	parse_actors(data, actorsSearchStart, (size_t)fsize);

	// GLOBAL EVENTS (after last actor)
	if(m.actor_count > 0) {
		// compute end of last actor (base + anim states)
		// we stored actor offsets during parse_actors — recompute end
		// search for the last actor and walk past its anim states
		size_t lastEnd = 0;
		{
			const uint8_t marker[8] = {1, 0, 0, 0, 1, 0, 0, 0};
			size_t sp				= actorsSearchStart;
			size_t actorStarts[NCE_MAX_ACTORS];
			uint32_t nFound = 0;
			while(sp + 8 < (size_t)fsize && nFound < NCE_MAX_ACTORS) {
				const uint8_t *found = std::search(data + sp, data + fsize, marker, marker + sizeof(marker));
				if(found == data + fsize) break;
				size_t idx = (size_t)(found - data);
				if(idx + 40 < (size_t)fsize && data[idx + 8] >= 0x20 && data[idx + 8] < 0x7F) {
					actorStarts[nFound++] = idx;
				}
				sp = idx + 1;
			}
			if(nFound > 0) {
				size_t la = actorStarts[nFound - 1];
				lastEnd	  = la + 140;
				// walk past anim states
				uint8_t ind	 = data[la + 75];
				int numExtra = (ind > 1) ? (int)(ind - 1) : 0;
				for(int s = 0; s < numExtra; s++) {
					if(lastEnd + 4 > (size_t)fsize) break;
					uint32_t st = r_u32(data + lastEnd);
					if(st == 2 && lastEnd + 44 <= (size_t)fsize)
						lastEnd += 44;
					else if(st == 1 && lastEnd + 48 <= (size_t)fsize)
						lastEnd += 48;
					else
						break;
				}
			}
		}
		if(lastEnd > 0 && lastEnd < (size_t)fsize) {
			parse_global_events(data, lastEnd, (size_t)fsize);
		}
	}

	free(data);
	return 1;
}

/*
 */
void NCEParser::parse_camera(const uint8_t *data, size_t camStart, size_t file_size) {

	memset(&m.camera, 0, sizeof(m.camera));

	if(camStart + 111 > file_size) return;

	const uint8_t *cam = data + camStart;

	// position (vec3 at +67), FOV (float at +83), Target (vec3 at +91)
	m.camera.pos_x	  = r_f32(cam + 67);
	m.camera.pos_y	  = r_f32(cam + 71);
	m.camera.pos_z	  = r_f32(cam + 75);
	m.camera.fov	  = r_f32(cam + 83);
	m.camera.target_x = r_f32(cam + 91);
	m.camera.target_y = r_f32(cam + 95);
	m.camera.target_z = r_f32(cam + 99);

	// check for camera NAD at +111 (32-byte field)
	if(camStart + 143 <= file_size) {
		char tmp[32];
		r_str(tmp, cam + 111, 32);
		// verify it ends with .nad and has a reasonable length (>8 rules out
		// partial matches like "ed.nad" from actor NAD bleed-through)
		size_t len = strlen(tmp);
		if(len > 8 && strcmp(tmp + len - 4, ".nad") == 0) {
			memcpy(m.camera.camera_nad, tmp, 32);
			m.camera.has_camera_nad = 1;
		}
	}
}

/*
 */
void NCEParser::parse_actors(const uint8_t *data, size_t searchStart, size_t file_size) {

	m.actor_count = 0;

	const uint8_t marker[8] = {1, 0, 0, 0, 1, 0, 0, 0};
	size_t sp				= searchStart;

	while(sp + 8 < file_size && m.actor_count < NCE_MAX_ACTORS) {

		// find next actor marker
		const uint8_t *found = std::search(data + sp, data + file_size, marker, marker + sizeof(marker));
		if(found == data + file_size) break;

		size_t aoff = (size_t)(found - data);

		// validate: must have room for 140-byte base and byte +8 must be printable
		if(aoff + 140 > file_size) break;
		if(data[aoff + 8] < 0x20 || data[aoff + 8] >= 0x7F) {
			sp = aoff + 1;
			continue;
		}

		CutsceneActor &a = m.actors[m.actor_count];
		memset(&a, 0, sizeof(a));

		r_str(a.model_name, data + aoff + 8, 32);
		r_str(a.instance_name, data + aoff + 40, 32);
		a.visibility	 = data[aoff + 74];
		a.anim_indicator = data[aoff + 75];
		r_str(a.nad_path, data + aoff + 95, 32);

		// parse anim-state records (count = anim_indicator - 1)
		a.anim_state_count = 0;
		int numExtra	   = (a.anim_indicator > 1) ? (int)(a.anim_indicator - 1) : 0;
		size_t spos		   = aoff + 140;

		for(int s = 0; s < numExtra && a.anim_state_count < NCE_MAX_ANIM_STATES; s++) {
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
				break; // unknown type — stop
			}
		}

		m.actor_count++;
		sp = spos; // continue search AFTER this actor's data
	}
}

/*
 */
void NCEParser::parse_global_events(const uint8_t *data, size_t evtStart, size_t file_size) {

	m.sound_cue_count	 = 0;
	m.conversation_count = 0;

	if(evtStart >= file_size) return;
	size_t remain = file_size - evtStart;

	// scan for printable strings that look like sound cues or conversations.
	// sound cues: typically "SF...", "env_...", "terran_...", "@music_..."
	// conversations: typically "cM...", or dialogue IDs
	// since the exact record structure varies, we do a string-scan approach
	// to reliably extract the names + look for preceding frame-time floats.
	const uint8_t *ev = data + evtStart;

	// first: check for the type-3 sound event header (u32=3, u32=count)
	if(remain >= 8) {
		uint32_t type  = r_u32(ev);
		uint32_t count = r_u32(ev + 4);
		if(type == 3 && count > 0 && count <= NCE_MAX_SOUND_CUES) {
			// we have a structured sound cue block
			// each sound cue record is ~106 bytes (name at +20 from record start)
			// but layout varies — fall through to string scan
		}
	}

	// string-scan: find all printable C-strings >= 4 chars in the event data
	size_t pos = 0;
	while(pos < remain) {
		if(ev[pos] >= 0x20 && ev[pos] < 0x7F) {
			size_t end = pos;
			while(end < remain && ev[end] != 0) end++;
			size_t slen = end - pos;

			if(slen >= 4 && slen < 64) {
				char tmp[64];
				memcpy(tmp, ev + pos, slen);
				tmp[slen] = '\0';

				int isSoundCue	   = 0;
				int isConversation = 0;

				// sound cues: SF..., env_..., terran_..., @music_..., *.wav
				if(tmp[0] == 'S' && tmp[1] == 'F')
					isSoundCue = 1;
				else if(strncmp(tmp, "env_", 4) == 0)
					isSoundCue = 1;
				else if(strncmp(tmp, "terran_", 7) == 0)
					isSoundCue = 1;
				else if(tmp[0] == '@')
					isSoundCue = 1;
				else if(slen > 4 && strcmp(tmp + slen - 4, ".wav") == 0)
					isSoundCue = 1;
				// dialogue / conversation triggers
				else if(tmp[0] == 'c' && tmp[1] == 'M')
					isConversation = 1;
				// general dialogue cue patterns: "X_Y_Z_Name##"
				else if(strchr(tmp, '_') && slen > 8) {
					// check for NAD refs — skip those
					if(slen > 4 && strcmp(tmp + slen - 4, ".nad") == 0) {
						// this is an animation reference, not a dialogue cue
					} else {
						// could be a dialogue cue (e.g. "3_1_Pylons_Hauler01")
						isSoundCue = 1; // categorise as sound/dialogue event
					}
				}

				if(isSoundCue && m.sound_cue_count < NCE_MAX_SOUND_CUES) {
					SoundCueEvent &sc = m.sound_cues[m.sound_cue_count];
					memset(&sc, 0, sizeof(sc));
					memcpy(sc.name, tmp, slen < 31 ? slen : 31);
					// try to read frame time from 4 bytes before the string
					if(pos >= 4) {
						float ft = r_f32(ev + pos - 4);
						if(ft > 0.0f && ft < 10000.0f) sc.frame_time = ft;
					}
					sc.volume = 1.0f;
					m.sound_cue_count++;
				} else if(isConversation && m.conversation_count < NCE_MAX_CONVERSATIONS) {
					ConversationEvent &ce = m.conversations[m.conversation_count];
					memset(&ce, 0, sizeof(ce));
					memcpy(ce.name, tmp, slen < 31 ? slen : 31);
					if(pos >= 4) {
						float ft = r_f32(ev + pos - 4);
						if(ft > 0.0f && ft < 10000.0f) ce.frame_time = ft;
					}
					m.conversation_count++;
				}
			}
			pos = end + 1;
		} else {
			pos++;
		}
	}
}

/*
 */
int NCEParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}
