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
class DDSParser;
class TGAParser;

/*
 */
namespace viewer::render {

    [[nodiscard]] GLuint uploadDDS(const DDSParser *dds) noexcept;
    [[nodiscard]] GLuint uploadTGA(const TGAParser *tga) noexcept;

    inline void freeTexture(GLuint &id) noexcept {
        if(id) { glDeleteTextures(1, &id); id = 0; }
    }

    // aspect-fitted ImGui::Image with up-to-2x upscale.
    void drawTexturePreview(GLuint texId, int texW, int texH);

} // namespace viewer::render
