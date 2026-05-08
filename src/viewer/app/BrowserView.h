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

#include <string>
#include <unordered_map>

#include "core/AssetDatabase.h"
#include "render/PreviewRenderer.h"
#include "scene/DdsCatalog.h"
#include "scene/MaterialLibrary.h"
#include "animation/AnimationController.h"
#include "AudioPlayer.h"
#include "ui/Inspectors.h" // TexturePreviewEntry

#include "parsers/AssetParser.h"

/*
 */
namespace viewer::app {

/*
 */
class BrowserView final {
        
    public:
        BrowserView();
        ~BrowserView();

        BrowserView(const BrowserView &) = delete;
        BrowserView &operator=(const BrowserView &) = delete;

        bool mount(const char *rootPath);
        void draw();

    private:
        void parseSelected();

        viewer::core::AssetDatabase        m_db;
        viewer::render::PreviewRenderer    m_renderer;
        viewer::scene::DdsCatalog          m_ddsCatalog;
        viewer::scene::MaterialLibrary     m_materials;
        viewer::anim::AnimationController  m_animation;
        AudioPlayer                        m_audio;

        IAssetParser *m_activeParser = nullptr;
        int           m_selectedAsset = -1;
        int           m_previewModelIdx = 0;
        int           m_playingEntry = -1;

        char        m_filterBuf[128] = {};
        std::string m_filterExt;
        std::unordered_map<int, viewer::ui::TexturePreviewEntry> m_texCache;
    };

} // namespace viewer::app
