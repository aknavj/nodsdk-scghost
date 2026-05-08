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
#include <unordered_map>
#include <string>

#include "parsers/AssetParser.h"

/*
 */
namespace viewer::core   { class AssetDatabase; }
namespace viewer::render { class PreviewRenderer; }
namespace viewer::scene  { class MaterialLibrary; class DdsCatalog; }
namespace viewer::anim   { class AnimationController; }
class AudioPlayer;

/*
 */
namespace viewer::ui {

/*
 */
struct TexturePreviewEntry {
	GLuint gl_texture = 0;
	int    width = 0;
	int    height = 0;
};

/*
 */
struct InspectorContext {
	viewer::render::PreviewRenderer       *renderer = nullptr;
	AudioPlayer                           *audio = nullptr;
	viewer::core::AssetDatabase           *db = nullptr;
	viewer::scene::MaterialLibrary        *materials = nullptr;
	viewer::scene::DdsCatalog             *ddsCatalog = nullptr;
	viewer::anim::AnimationController     *animation = nullptr;
	std::unordered_map<int, TexturePreviewEntry> *texCache = nullptr;
	int  *playingEntry = nullptr;
	int  *previewModelIndex = nullptr; // for NMB sub-model selector
	int   selectedAssetIdx = -1;
};

// top-level dispatcher (called from BrowserView::draw).
void drawInspector(IAssetParser *parser, InspectorContext &ctx);

} // namespace viewer::ui
