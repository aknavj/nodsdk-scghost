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

/*
 */
namespace viewer::render {

    /*
     */
    enum class CameraMode { 
        Orbital, 
        Fps 
    };

    /*
     */
    struct Camera {

        CameraMode mode = CameraMode::Orbital;

        // orbital
        float     yaw = 135.0f;
        float     pitch = 20.0f;
        float     dist = 5.0f;
        glm::vec3 target = glm::vec3(0.0f);

        // FPS
        float     fpsYaw = -45.0f;
        float     fpsPitch = -10.0f;
        glm::vec3 fpsPos = glm::vec3(0.0f);

        // scene bounds
        glm::vec3 boundsMin = glm::vec3(0.0f);
        glm::vec3 boundsMax = glm::vec3(0.0f);

        // reset to a sensible default orbiting the current bounds.
        void reset() noexcept;

        // switch to FPS while preserving current viewing direction.
        void switchToFps() noexcept;

        // compute world-space camera position and view matrix for current state.
        struct Frame {
            glm::vec3 pos;
            glm::mat4 view;
            float     refDist; // for near/far clip & grid scale
        };
        [[nodiscard]] Frame frame() const noexcept;

        // rebuild target/dist/angles from a new bounds.
        void fitTo(const glm::vec3 &bMin, const glm::vec3 &bMax) noexcept;
    };

} // namespace viewer::render
