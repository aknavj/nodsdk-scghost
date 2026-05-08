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

#include "parsers/AssetParser.h"
#include "parsers/NADParser.h"
#include "parsers/NODParser.h"

#include <string>

namespace viewer::core   { class AssetDatabase; }
namespace viewer::render { class PreviewRenderer; }
namespace viewer::anim {

/*
*/
class AnimationController {
        
    public:
        AnimationController() = default;
        ~AnimationController() { unload(); }

        AnimationController(const AnimationController &) = delete;
        AnimationController &operator=(const AnimationController &) = delete;

        // load a NAD file; replaces current animation.
        bool loadAnimation(int assetIdx, const viewer::core::AssetDatabase &db);
        void unload();

        // advance frame counter by wall-clock delta.
        void tick(double nowSeconds);

        // sample bones for current clip and push to renderer; modelIndex only relevant for NMB.
        void apply(IAssetParser *modelParser, int modelIndex, viewer::render::PreviewRenderer &renderer);

        // state accessors
        [[nodiscard]] bool        isLoaded()    const noexcept { return m_active != nullptr; }
        [[nodiscard]] int         assetIndex()  const noexcept { return m_assetIdx; }
        [[nodiscard]] NADParser  *clip()        const noexcept { return m_active; }
        [[nodiscard]] float      &frame()             noexcept { return m_frame; }
        [[nodiscard]] bool       &playing()           noexcept { return m_playing; }
        [[nodiscard]] float      &fps()               noexcept { return m_fps; }

    private:
        NADParser *m_active = nullptr;
        int        m_assetIdx = -1;
        float      m_frame = 0.0f;
        bool       m_playing = false;
        float      m_fps = 30.0f;
        double     m_lastTime = 0.0;
    };

} // namespace viewer::anim
