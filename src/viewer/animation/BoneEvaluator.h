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

#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "parsers/NADParser.h"
#include "parsers/NODParser.h"

/*
 */
namespace viewer::anim {

    // sample a single track at fractional frame using cubic B-spline keys.
    [[nodiscard]] glm::vec3 evalTrack(const NADParser::BoneTrack &track, float frame) noexcept;

    // build euler rotation matching D3D MatrixPYR (Rx*Ry*Rz), column-major.
    [[nodiscard]] glm::mat4 buildEulerRotation(const glm::vec3 &eulerDeg) noexcept;

    // FK chain -> skin matrices; out[i] = global_pose[i] * T(bone[i].offset).
    void computeBoneMatrices(const NADParser::AnimationClip &clip, const NODParser::Bone *bones, uint32_t boneCount, float frame, std::vector<glm::mat4> &out);

} // namespace viewer::anim
