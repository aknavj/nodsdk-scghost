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

#ifndef __NADPARSER_H__
#define __NADPARSER_H__

#include "AssetParser.h"
#include <glm/glm.hpp>

/*
 */
class NADParser : public IAssetParser {

	public:
		static const uint32_t MAX_BONE_TRACKS  = 256;
		static const uint32_t MAX_SHAPE_TRACKS = 64;

		// track types (from decompiled tBoneTrack.type)
		enum TrackType : uint32_t {
			TRACK_ROTATION	  = 0, // euler angles (degrees)
			TRACK_TRANSLATION = 1 // world-space position
		};

		// 52 bytes per key — cubic polynomial coefficients
		struct BoneKey {
			float frame; // +0   time in frames
			glm::vec3 value; // +4   base value (position or euler rotation)
			glm::vec3 b_coeff; // +16  linear coefficient (velocity)
			glm::vec3 c_coeff; // +28  quadratic coefficient
			glm::vec3 d_coeff; // +40  cubic coefficient
		};

		struct BoneTrack {
			uint32_t bone_number; // bone index into model skeleton
			uint32_t num_keys;
			uint32_t type; // TrackType: 0=rotation, 1=translation
			BoneKey *keys; // malloc'd array
		};

		// 8 bytes per tag
		struct KeyTag {
			uint32_t tag_type; // eRenderKeyTag enum value
			float frame_num; // frame at which event fires
		};

		// 8 bytes per shape key
		struct ShapeKey {
			float frame;
			float weight;
		};

		struct ShapeTrack {
			uint8_t group_num;
			uint8_t shape_num;
			uint16_t num_keys;
			ShapeKey *keys; // malloc'd array
		};

		struct AnimationClip {
			uint32_t version;
			uint32_t num_bone_tracks;
			uint32_t flags;
			float duration; // in frames

			BoneTrack *bone_tracks; // malloc'd array

			uint32_t num_key_tags;
			KeyTag *key_tags; // malloc'd array

			uint32_t num_shape_tracks;
			ShapeTrack *shape_tracks; // malloc'd array

			size_t file_size;
		};

		NADParser() { memset(&clip, 0, sizeof(clip)); }
		~NADParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NAD"; }
		const AnimationClip &getAnimationClip() const { return clip; }

	private:
		AnimationClip clip;
};

#endif // __NADPARSER_H__
