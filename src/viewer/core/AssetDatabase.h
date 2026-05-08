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
#include <vector>

/*
 */
namespace viewer::core {

    /*
     */
    struct AssetEntry {
        std::string relativePath; // path relative to mount root
        std::string extension;    // lowercase, no dot
        std::string category;     // "3D/Models", "Levels", "UI", ...
        std::size_t fileSize = 0;
        bool        isParsed = false;
        bool        isFailed = false;
    };

    /*
     */
    struct AssetCategory {
        std::string      name;
        std::vector<int> assetIndices; // indices into AssetDatabase::entries
    };

    /*
     */
    class AssetDatabase {
        public:
            // mount a directory; returns true if any files were found.
            bool mount(const char *rootPath);

            [[nodiscard]] const std::string &rootPath()   const noexcept { return m_root; }
            [[nodiscard]] const std::vector<AssetEntry>    &entries()    const noexcept { return m_entries; }
            [[nodiscard]] const std::vector<AssetCategory> &categories() const noexcept { return m_categories; }
            [[nodiscard]] std::vector<AssetEntry>    &entriesMut()    noexcept { return m_entries; }

            [[nodiscard]] int totalFiles()    const noexcept { return m_totalFiles; }
            [[nodiscard]] int parsableFiles() const noexcept { return m_parsableFiles; }

            // Build an absolute path for an entry index.
            [[nodiscard]] std::string fullPath(int idx) const;

        private:
            void scanDirectory(const char *dir, const char *relBase);
            void buildCategories();
            [[nodiscard]] static std::string categorize(const std::string &relPath);

            std::string                m_root;
            std::vector<AssetEntry>    m_entries;
            std::vector<AssetCategory> m_categories;
            int m_totalFiles = 0;
            int m_parsableFiles = 0;
    };

} // namespace viewer::core
