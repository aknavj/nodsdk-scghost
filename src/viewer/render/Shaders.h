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

#include <GL/glew.h>

/*
 */
namespace viewer::render {

    extern const char *kPreviewVertexShader;
    extern const char *kPreviewFragmentShader;

    [[nodiscard]] GLuint compileShader(GLenum type, const char *src) noexcept;
    [[nodiscard]] GLuint linkProgram(GLuint vs, GLuint fs) noexcept;

    // compile + link + delete shaders; returns 0 on failure.
    [[nodiscard]] GLuint buildProgram(const char *vsSrc, const char *fsSrc) noexcept;

} // namespace viewer::render
