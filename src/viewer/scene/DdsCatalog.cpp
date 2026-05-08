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

#include "scene/DdsCatalog.h"
#include "core/Strings.h"

#include <cstdio>
#include <dirent.h>

namespace viewer::scene {

namespace {

// same 13 directories the engine searches, in order.
constexpr const char *kSearchDirs[] = {
	"Materials/Common",  "Materials/Metal",   "Materials/Stone",   "Materials/Wood",   "Materials/Organic",
	"Materials/Glass",   "Materials/Effects", "Materials/Terrain", "Materials/Colors", "Materials/Lights",
	"Materials/Shaders", "Materials",         "3D/Materials",
};

} // namespace

/*
 */
void DdsCatalog::setAssetDir(std::string assetDir) noexcept {
	m_assetDir = std::move(assetDir);
	m_lookup.clear();
	m_built = false;
}

/*
 */
void DdsCatalog::buildIfNeeded() {
	if(m_built) return;
	m_built = true;
	if(m_assetDir.empty()) return;

	for(const char *sub : kSearchDirs) {
		const std::string dirPath = m_assetDir + "/" + sub;
		DIR *dir = opendir(dirPath.c_str());
		if(!dir) continue;

		while(auto *ent = readdir(dir)) {
			const std::string fn = ent->d_name;
			if(fn.size() < 5) continue;
			const std::string lower = viewer::core::toLower(fn);
			if(lower.size() <= 4 || lower.substr(lower.size() - 4) != ".dds") continue;
			const std::string key = lower.substr(0, lower.size() - 4);
			// first directory wins (matches engine search order).
			if(m_lookup.count(key) == 0) m_lookup[key] = dirPath + "/" + fn;
		}
		closedir(dir);
	}
	std::printf("[DdsCatalog] %zu textures across 13 dirs\n", m_lookup.size());
}

/*
 */
std::string DdsCatalog::pathFor(std::string_view lowercaseKey) {
	buildIfNeeded();
	const auto it = m_lookup.find(std::string(lowercaseKey));
	return it == m_lookup.end() ? std::string() : it->second;
}

/*
 */
std::size_t DdsCatalog::size() {
	buildIfNeeded();
	return m_lookup.size();
}

} // namespace viewer::scene
