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

#ifndef __NCSPARSER_H__
#define __NCSPARSER_H__

#include "AssetParser.h"

#define NCS_MAX_ACTORS 32
#define NCS_MAX_ANIM_STATES 16
#define NCS_MAX_SOUND_CUES 16
#define NCS_MAX_CONVERSATIONS 4

/*
 */
class NCSParser : public IAssetParser {

	public:
		// per-actor animation state event
		struct AnimStateEvent {
			uint32_t type; // 1 or 2
			float frame_time; // frame at which this event fires
			char name[32]; // animation or state name
		};

		/*
		 */
		// actor record extracted from the NCS body
		struct CutsceneActor {
			char model_name[32]; // template model name  (e.g. "nova_cut")
			char instance_name[32]; // instance name
			char nad_path[32]; // per-actor animation NAD path
			uint32_t marker2; // second marker u32: 1=base, 2=has anims

			uint32_t anim_state_count;
			AnimStateEvent anim_states[NCS_MAX_ANIM_STATES];
		};

		/*
		 */
		// sound cue event (from type-3 block, 76-byte records)
		struct SoundCueEvent {
			char name[32]; // sound file / cue name
			float frame_time; // trigger frame
			float volume; // playback volume (typically 1.0)
		};

		/*
		 */
		struct ConversationEvent {
			char name[32]; // conversation ID (e.g. "cM121_EscortSCV")
			float frame_time; // trigger frame
		};

		/*
		 */
		struct CutsceneScriptData {
			uint32_t version;
			uint16_t variant1; // u16 at +4 in header
			uint16_t variant2; // u16 at +6 in header
			uint16_t variant3; // u16 at +8 in header
			uint16_t shot_count; // u16 at +10
			uint16_t event_count; // u16 at +18
			uint32_t data_size; // size of body after header
			size_t file_size;

			char camera_nad[32]; // camera animation path (empty if none)
			int has_camera_nad;

			uint32_t actor_count;
			CutsceneActor actors[NCS_MAX_ACTORS];

			uint32_t sound_cue_count;
			SoundCueEvent sound_cues[NCS_MAX_SOUND_CUES];

			uint32_t conversation_count;
			ConversationEvent conversations[NCS_MAX_CONVERSATIONS];
		};

		NCSParser() { memset(&m, 0, sizeof(m)); }
		~NCSParser() override = default;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NCS"; }
		const CutsceneScriptData &getCutsceneScriptData() const { return m; }

	private:
		void parse_camera_nad(const uint8_t *data, size_t bodyStart, size_t bodyEnd);
		void parse_actors(const uint8_t *data, size_t searchStart, size_t file_size);
		void parse_global_events(const uint8_t *data, size_t evtStart, size_t file_size);

		CutsceneScriptData m;
};

#endif // __NCSPARSER_H__
