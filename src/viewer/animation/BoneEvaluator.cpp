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

#include "animation/BoneEvaluator.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace viewer::anim {

/*
 */
glm::vec3 evalTrack(const NADParser::BoneTrack &track, float frame) noexcept {
	if(!track.keys || track.num_keys == 0) return glm::vec3(0.0f);
	if(track.num_keys == 1) return track.keys[0].value;
	if(frame <= track.keys[0].frame) return track.keys[0].value;
	if(frame >= track.keys[track.num_keys - 1].frame) return track.keys[track.num_keys - 1].value;
	for(uint32_t i = 0; i + 1 < track.num_keys; ++i) {
		if(frame <= track.keys[i + 1].frame) {
			const float t = frame - track.keys[i].frame;
			const auto &k = track.keys[i];
			return k.value + k.b_coeff * t + k.c_coeff * (t * t) + k.d_coeff * (t * t * t);
		}
	}
	return track.keys[track.num_keys - 1].value;
}

/*
 */
glm::mat4 buildEulerRotation(const glm::vec3 &euler) noexcept {
	const float sP = std::sin(glm::radians(euler.x)), cP = std::cos(glm::radians(euler.x));
	const float sY = std::sin(glm::radians(euler.y)), cY = std::cos(glm::radians(euler.y));
	const float sR = std::sin(glm::radians(euler.z)), cR = std::cos(glm::radians(euler.z));

	glm::mat4 m(1.0f);
	m[0][0] = cY*cR;             m[0][1] = cY*sR;             m[0][2] = -sY;
	m[1][0] = sY*sP*cR - cP*sR;  m[1][1] = sY*sP*sR + cP*cR;  m[1][2] = cY*sP;
	m[2][0] = sY*cP*cR + sP*sR;  m[2][1] = sY*cP*sR - sP*cR;  m[2][2] = cP*cY;
	return m;
}

/*
 */
void computeBoneMatrices(const NADParser::AnimationClip &clip, const NODParser::Bone *bones, uint32_t boneCount, float frame, std::vector<glm::mat4> &out) {
	std::vector<glm::vec3> rot(boneCount, glm::vec3(0.0f));
	std::vector<glm::vec3> trans(boneCount, glm::vec3(0.0f));

	for(uint32_t t = 0; t < clip.num_bone_tracks; ++t) {
		const auto &track = clip.bone_tracks[t];
		if(!track.keys || track.num_keys == 0) continue;
		if(track.bone_number >= boneCount) continue;
		const glm::vec3 v = evalTrack(track, frame);
		if(track.type == NADParser::TRACK_ROTATION) rot[track.bone_number] = v;
		else { trans[track.bone_number] = v; }
	}

	out.assign(boneCount, glm::mat4(1.0f));
	std::vector<glm::mat4> globalPose(boneCount, glm::mat4(1.0f));

	for(uint32_t i = 0; i < boneCount; ++i) {
		const auto &bone = bones[i];
		const glm::mat4 R = buildEulerRotation(rot[i]);
		glm::vec3 pos(bone.position[0], bone.position[1], bone.position[2]);
		pos += trans[i];
		const glm::mat4 localMat = glm::translate(glm::mat4(1.0f), pos) * R;

		const uint8_t par = bone.parent;
		globalPose[i] = (par != 0xFF && par < boneCount)
		                  ? globalPose[par] * localMat
		                  : localMat;

		const glm::vec3 off(bone.offset[0], bone.offset[1], bone.offset[2]);
		out[i] = globalPose[i] * glm::translate(glm::mat4(1.0f), off);
	}
}

} // namespace viewer::anim
