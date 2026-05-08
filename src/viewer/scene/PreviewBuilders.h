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

#include <cstdint>
#include <string>

/*
 */
class NODParser;
class NMBParser;
class NILParser;
class NOCParser;

/*
 */
namespace viewer::render { class PreviewRenderer; }

/*
 */
namespace viewer::scene {

    /*
    */
    class MaterialLibrary;
    class DdsCatalog;

    void buildNodPreview(NODParser *nod, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog);
    void buildNmbPreview(NMBParser *nmb, int modelIndex, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog);
    void buildNilPreview(NILParser *nil, const std::string &nilFullPath, const std::string &assetDir, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog);
    void buildNocPreview(NOCParser *noc, viewer::render::PreviewRenderer &renderer);

} // namespace viewer::scene
