/*
===========================================================================
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

// NODExport nad <in.nad> <out.json>: an animation clip as JSON.
//
// length in frames; tracks with bone number, kind (0 turn as three angles
// in degrees, 1 position) and keys. A key is 13 numbers: frame, value[3],
// b[3], c[3], d[3]; the value at t frames after the key is
// value + t*b + t^2*c + t^3*d. Not-a-number is written as null (the last
// key of a track holds it). Tags: type and frame.

#include "NADParser.h"
#include "export.h"

namespace nodexport {

	bool exportNad(const char *in, std::string &json) {
		NADParser parser;
		if(!parser.parse(in)) {
			json = refusal("nad", "not a readable NAD file");
			return false;
		}
		const auto &clip = parser.getAnimationClip();
		if(clip.version != 4) {
			json = refusal("nad", "version " + std::to_string(clip.version) + ", not 4");
			return false;
		}
		if(clip.bytes_read != clip.file_size) {
			json = refusal("nad", std::to_string(clip.file_size - clip.bytes_read) + " bytes the layout does not explain");
			return false;
		}
		std::vector<std::string> tracks, tags;
		for(uint32_t t = 0; t < clip.num_bone_tracks; ++t) {
			const auto &track = clip.bone_tracks[t];
			if(track.type > 1) {
				json = refusal("nad", "track " + std::to_string(t) + ": kind " + std::to_string(track.type) + ", not 0 or 1");
				return false;
			}
			std::vector<std::string> keys;
			for(uint32_t k = 0; k < track.num_keys; ++k) {
				const auto &key = track.keys[k];
				if(!std::isfinite(key.frame)) {
					json = refusal("nad", "track " + std::to_string(t) + " key " + std::to_string(k) + ": frame not finite");
					return false;
				}
				const float v[13] = {key.frame,		key.value.x,   key.value.y,	  key.value.z,	 key.b_coeff.x,
									 key.b_coeff.y, key.b_coeff.z, key.c_coeff.x, key.c_coeff.y, key.c_coeff.z,
									 key.d_coeff.x, key.d_coeff.y, key.d_coeff.z};
				keys.push_back(floats(v, 13));
			}
			tracks.push_back("{\"bone\":" + std::to_string(track.bone_number) + ",\"kind\":" + std::to_string(track.type) +
							 ",\"keys\":" + survey::list(keys) + "}");
		}
		for(uint32_t i = 0; i < clip.num_key_tags; ++i)
			tags.push_back("{\"type\":" + std::to_string(clip.key_tags[i].tag_type) +
						   ",\"frame\":" + exact(clip.key_tags[i].frame_num) + "}");
		json = "{\"format\":\"nad\",\"version\":4,\"length\":" + exact(clip.duration) + ",\"framesPerSecond\":30" +
			   ",\"tracks\":" + survey::list(tracks) + ",\"tags\":" + survey::list(tags) +
			   ",\"shapeTracks\":" + std::to_string(clip.num_shape_tracks) + "}\n";
		return true;
	}

} // namespace nodexport
