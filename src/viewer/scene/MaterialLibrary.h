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
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "parsers/NODParser.h"

/*
 */
namespace viewer::scene {

/*
 */
class DdsCatalog;

/*
 */
class MaterialLibrary {
    public:
        MaterialLibrary() = default;
        ~MaterialLibrary();

        MaterialLibrary(const MaterialLibrary &) = delete;
        MaterialLibrary &operator=(const MaterialLibrary &) = delete;

        void setAssetDir(std::string assetDir) noexcept;

        // scan NSA materials; idempotent.
        void buildMaterialDB();

        // find or upload diffuse texture for given shader name; returns 0 if unmatched.
        [[nodiscard]] GLuint findAndUploadDDS(std::string_view name, DdsCatalog &ddsCatalog);

        // full lookup: direct DDS, then NSA material DB redirect; returns 0 if unmatched.
        [[nodiscard]] GLuint resolveTexture(std::string_view shaderName, DdsCatalog &ddsCatalog);

        // resolve per-group diffuse textures; outTextures sized to groupCount.
        void loadMaterialsForModel(const char shaderNames[][33], uint32_t shaderCount, const NODParser::MeshGroup *groups, uint32_t groupCount, DdsCatalog &ddsCatalog, std::vector<GLuint> &outTextures);

        // free all owned textures; called by destructor.
        void clear() noexcept;

    private:
        std::string m_assetDir;
        std::unordered_map<std::string, std::string> m_materialDb;   // matName(lc) -> texture name
        std::unordered_map<std::string, GLuint>      m_texCache;     // texKey(lc)  -> GL name
        bool m_built = false;
};

} // namespace viewer::scene
