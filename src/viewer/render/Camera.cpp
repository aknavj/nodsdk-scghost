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

#include "render/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

/*
 */
namespace viewer::render {

/*
 */
void Camera::reset() noexcept {
	target = (boundsMin + boundsMax) * 0.5f;
	const float extent = glm::length(boundsMax - boundsMin);
	dist = extent > 0.001f ? extent * 1.5f : 5.0f;
	yaw = 135.0f;
	pitch = 20.0f;
}

/*
 */
void Camera::fitTo(const glm::vec3 &bMin, const glm::vec3 &bMax) noexcept {
	boundsMin = bMin;
	boundsMax = bMax;
	reset();
}

/*
 */
void Camera::switchToFps() noexcept {
	const float yr = glm::radians(yaw);
	const float pr = glm::radians(pitch);
	fpsPos.x = target.x + dist * std::cos(pr) * std::sin(yr);
	fpsPos.y = target.y + dist * std::sin(pr);
	fpsPos.z = target.z + dist * std::cos(pr) * std::cos(yr);
	const glm::vec3 dir = glm::normalize(target - fpsPos);
	fpsPitch = glm::degrees(std::asin(dir.y));
	fpsYaw = glm::degrees(std::atan2(dir.x, dir.z));
}

/*
 */
Camera::Frame Camera::frame() const noexcept {
	Frame f;
	if(mode == CameraMode::Fps) {
		const float yr = glm::radians(fpsYaw);
		const float pr = glm::radians(fpsPitch);
		const glm::vec3 fwd(std::sin(yr) * std::cos(pr), std::sin(pr), std::cos(yr) * std::cos(pr));
		f.pos = fpsPos;
		f.view = glm::lookAt(f.pos, f.pos + fwd, glm::vec3(0, 1, 0));
		f.refDist = std::max(glm::length(boundsMax - boundsMin), 1.0f);
	} else {
		const float yr = glm::radians(yaw);
		const float pr = glm::radians(pitch);
		f.pos.x = target.x + dist * std::cos(pr) * std::sin(yr);
		f.pos.y = target.y + dist * std::sin(pr);
		f.pos.z = target.z + dist * std::cos(pr) * std::cos(yr);
		f.view = glm::lookAt(f.pos, target, glm::vec3(0, 1, 0));
		f.refDist = dist;
	}
	return f;
}

} // namespace viewer::render
