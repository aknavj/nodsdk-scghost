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

#ifndef __NCEPARSER_H__
#define __NCEPARSER_H__

#include "AssetParser.h"

#define NCE_MAX_ACTORS 32
#define NCE_MAX_ANIM_STATES 16
#define NCE_MAX_SOUND_CUES 16
#define NCE_MAX_CONVERSATIONS 4

/*
 */
class NCEParser : public IAssetParser {

	public:
		// per-actor animation state event (type-1 or type-2)
		struct AnimStateEvent {
			uint32_t type; // 1 = named anim, 2 = state change
			float frame_time; // frame at which this event fires
			char name[32]; // animation or state name
		};

		// actor record extracted from the NCE body
		struct CutsceneActor {
			char model_name[32]; // template model name  (e.g. "nova_cut")
			char instance_name[32]; // instance name (e.g. "Nova_121a_tankRush")
			char nad_path[32]; // per-actor animation NAD path
			uint8_t visibility; // flags[2]: 0x00/0x01/0xFF/etc.
			uint8_t anim_indicator; // flags[3]: (value-1) = extra state count

			uint32_t anim_state_count;
			AnimStateEvent anim_states[NCE_MAX_ANIM_STATES];
		};

		// initial camera shot data (from 111-byte base block)
		struct CameraShot {
			float pos_x, pos_y, pos_z;
			float target_x, target_y, target_z;
			float fov; // field of view in degrees
			char camera_nad[32]; // external camera NAD (empty if inline)
			int has_camera_nad; // non-zero if camera_nad is populated
		};

		// sound cue event (global, after actors)
		struct SoundCueEvent {
			char name[32]; // sound file / cue name
			float frame_time; // trigger frame
			float volume; // volume hint (1.0 default)
		};

		// conversation trigger (global, after actors)
		struct ConversationEvent {
			char name[32]; // conversation ID (e.g. "cM121_EscortSCV")
			float frame_time; // trigger frame
		};

		/*
		 */
		struct CutsceneEventData {
			uint32_t version;
			char map_name[128];
			uint32_t num_frames;
			float time_scale;
			uint16_t shot_count;
			uint16_t event_count;
			uint32_t data_size;
			size_t file_size;

			CameraShot camera;

			uint32_t actor_count;
			CutsceneActor actors[NCE_MAX_ACTORS];

			uint32_t sound_cue_count;
			SoundCueEvent sound_cues[NCE_MAX_SOUND_CUES];

			uint32_t conversation_count;
			ConversationEvent conversations[NCE_MAX_CONVERSATIONS];
		};

		NCEParser() { memset(&m, 0, sizeof(m)); }
		~NCEParser() override = default;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NCE"; }
		const CutsceneEventData &getCutsceneEventData() const { return m; }

	private:
		void parse_camera(const uint8_t *data, size_t camStart, size_t file_size);
		void parse_actors(const uint8_t *data, size_t searchStart, size_t file_size);
		void parse_global_events(const uint8_t *data, size_t evtStart, size_t file_size);

		CutsceneEventData m;
};

#endif // __NCEPARSER_H__
