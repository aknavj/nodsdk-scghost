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
#include <string_view>
#include <unordered_map>

/*
 */
namespace viewer::scene {

/*
 */
class DdsCatalog {
    public:
        DdsCatalog() = default;

        // rebind to a new asset root; invalidates the cache.
        void setAssetDir(std::string assetDir) noexcept;

        // look up full path by lowercase basename (no .dds); lazy-builds on first call.
        [[nodiscard]] std::string pathFor(std::string_view lowercaseKey);

        [[nodiscard]] std::size_t size();

    private:
        void buildIfNeeded();

        std::string m_assetDir;
        std::unordered_map<std::string, std::string> m_lookup;
        bool m_built = false;
};

} // namespace viewer::scene
