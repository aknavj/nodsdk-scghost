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

#include "core/AssetDatabase.h"
#include "core/Strings.h"

#include "parsers/AssetParser.h"
#include "parsers/AssetParserFactory.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unordered_map>

/*
 */
namespace viewer::core {

/*
 */
bool AssetDatabase::mount(const char *rootPath) {
	if(!rootPath) return false;

	m_root = rootPath;
	m_entries.clear();
	m_categories.clear();
	m_totalFiles = 0;
	m_parsableFiles = 0;

	scanDirectory(rootPath, "");

	std::sort(m_entries.begin(), m_entries.end(),
	          [](const AssetEntry &a, const AssetEntry &b) {
		          return a.relativePath < b.relativePath;
	          });

	buildCategories();

	std::printf("[AssetDatabase] Mounted '%s': %d files (%d parsable)\n", rootPath, m_totalFiles, m_parsableFiles);
	return m_totalFiles > 0;
}

/*
 */
std::string AssetDatabase::fullPath(int idx) const {
	if(idx < 0 || idx >= static_cast<int>(m_entries.size())) return {};
	return m_root + "/" + m_entries[idx].relativePath;
}

/*
 */
void AssetDatabase::scanDirectory(const char *dir, const char *relBase) {
	DIR *d = ::opendir(dir);
	if(!d) return;

	struct dirent *ent;
	while((ent = ::readdir(d)) != nullptr) {
		if(ent->d_name[0] == '.') continue;

		char fullPath[2048];
		std::snprintf(fullPath, sizeof(fullPath), "%s/%s", dir, ent->d_name);

		char relPath[2048];
		if(relBase[0]) {
			std::snprintf(relPath, sizeof(relPath), "%s/%s", relBase, ent->d_name);
		}
		else {
			std::snprintf(relPath, sizeof(relPath), "%s", ent->d_name);
		}

		struct stat st;
		if(::stat(fullPath, &st) != 0) continue;

		if(S_ISDIR(st.st_mode)) {
			scanDirectory(fullPath, relPath);
		} else if(S_ISREG(st.st_mode)) {
			AssetEntry entry;
			entry.relativePath = relPath;
			entry.extension = getExtension(relPath);
			entry.category = categorize(relPath);
			entry.fileSize = static_cast<std::size_t>(st.st_size);

			++m_totalFiles;
			if(!entry.extension.empty()) {
				IAssetParser *probe = AssetParserFactory::createParser(fullPath);
				if(probe) {
					++m_parsableFiles;
					delete probe;
				}
			}
			m_entries.push_back(std::move(entry));
		}
	}
	::closedir(d);
}

/*
 */
void AssetDatabase::buildCategories() {
	m_categories.clear();
	std::unordered_map<std::string, int> catMap;

	for(int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
		const std::string &cat = m_entries[i].category;
		auto it = catMap.find(cat);
		if(it == catMap.end()) {
			catMap[cat] = static_cast<int>(m_categories.size());
			AssetCategory ac;
			ac.name = cat;
			ac.assetIndices.push_back(i);
			m_categories.push_back(std::move(ac));
		} else {
			m_categories[it->second].assetIndices.push_back(i);
		}
	}

	std::sort(m_categories.begin(), m_categories.end(),
    [](const AssetCategory &a, const AssetCategory &b) {
        return a.name < b.name;
    });
}

/*
 */
std::string AssetDatabase::categorize(const std::string &relPath) {
	const std::size_t slash = relPath.find('/');
	if(slash != std::string::npos) {
		const std::string top = relPath.substr(0, slash);
		const std::size_t slash2 = relPath.find('/', slash + 1);
		if(slash2 != std::string::npos &&
		   (top == "3D" || top == "Materials" || top == "Misc")) {
			return relPath.substr(0, slash2);
		}
		return top;
	}
	return "Root";
}

} // namespace viewer::core
