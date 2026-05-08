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

#include "scene/MaterialLibrary.h"
#include "scene/DdsCatalog.h"
#include "core/Strings.h"
#include "render/TextureUpload.h"

#include "parsers/DDSParser.h"
#include "parsers/NSAParser.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>

/*
 */
namespace viewer::scene {

/*
 */
MaterialLibrary::~MaterialLibrary() { clear(); }

/*
 */
void MaterialLibrary::setAssetDir(std::string assetDir) noexcept {
	m_assetDir = std::move(assetDir);
	m_materialDb.clear();
	m_built = false;
	clear();
}

/*
 */
void MaterialLibrary::clear() noexcept {
	for(auto &kv : m_texCache)
		viewer::render::freeTexture(kv.second);
	m_texCache.clear();
}

/*
 */
void MaterialLibrary::buildMaterialDB() {
	if(m_built) return;
	m_built = true;
	if(m_assetDir.empty()) return;

	const std::string dirs[3] = {
		m_assetDir + "/Materials",
		m_assetDir + "/3D/Materials",
		m_assetDir + "/Materials/Effects",
	};

	for(const auto &dirPath : dirs) {
		DIR *dir = opendir(dirPath.c_str());
		if(!dir) continue;

		while(auto *ent = readdir(dir)) {
			const std::string fname = ent->d_name;
			if(fname.size() < 5) continue;
			const std::string ext = viewer::core::toLower(fname.substr(fname.size() - 4));
			if(ext != ".nsa" && ext != ".nas") continue;

			const std::string nsaPath = dirPath + "/" + fname;
			NSAParser nsa;
			if(nsa.parse(nsaPath.c_str()) == 0) continue;

			const auto &md = nsa.getMaterialsData();
			for(uint32_t m = 0; m < md.material_count; ++m) {
				const auto &mat = md.materials[m];
				const std::string matName = mat.name;
				if(matName.empty()) continue;

				const std::string key = viewer::core::toLower(matName);
				if(m_materialDb.count(key)) continue;

				const char *texVal = nullptr;
				const char *frameVal = nullptr;
				for(uint32_t p = 0; p < mat.prop_count; ++p) {
					if(std::strcmp(mat.props[p].key, "texture") == 0) {
						texVal = mat.props[p].value;
					}
					else if(std::strcmp(mat.props[p].key, "frame") == 0 && !frameVal) {
						frameVal = mat.props[p].value;
					}
				}

				const char *effective = texVal;
				if((!effective || effective[0] == '\0') && frameVal && frameVal[0] != '\0') {
					effective = frameVal;
				}
				if(!effective || effective[0] == '\0') continue;
				if(std::strcmp(effective, "dflt") == 0) continue;

				const std::string cleaned = viewer::core::cleanTexName(effective);
				if(!cleaned.empty()) m_materialDb[key] = cleaned;
			}
		}
		closedir(dir);
	}

	std::printf("[MaterialLibrary] DB: %zu entries\n", m_materialDb.size());
}

/*
 */
GLuint MaterialLibrary::findAndUploadDDS(std::string_view name, DdsCatalog &ddsCatalog) {
	const std::string key = viewer::core::toLower(viewer::core::cleanTexName(std::string(name).c_str()));
	if(key.empty()) return 0;

	const auto cit = m_texCache.find(key);
	if(cit != m_texCache.end()) return cit->second;

	const std::string path = ddsCatalog.pathFor(key);
	if(path.empty()) return 0;

	DDSParser dds;
	if(dds.parse(path.c_str()) == 0) return 0;

	const GLuint tid = viewer::render::uploadDDS(&dds);
	if(!tid) return 0;

	m_texCache[key] = tid;
	return tid;
}

/*
 */
GLuint MaterialLibrary::resolveTexture(std::string_view shaderName, DdsCatalog &ddsCatalog) {
	const std::string name(shaderName);
	if(name.empty()) return 0;

	GLuint tid = findAndUploadDDS(name, ddsCatalog);
	if(tid) return tid;

	const std::string matKey = viewer::core::toLower(name);
	const auto mit = m_materialDb.find(matKey);
	if(mit == m_materialDb.end() || mit->second.empty()) return 0;

	return findAndUploadDDS(mit->second, ddsCatalog);
}

/*
 */
void MaterialLibrary::loadMaterialsForModel(const char shaderNames[][33], uint32_t shaderCount, const NODParser::MeshGroup *groups, uint32_t groupCount, DdsCatalog &ddsCatalog, std::vector<GLuint> &outTextures) {
	outTextures.assign(groupCount, 0);
	buildMaterialDB();

	for(uint32_t g = 0; g < groupCount; ++g) {
		const uint32_t si = groups[g].shader_index;
		if(si >= shaderCount) continue;

		const std::string shaderName = shaderNames[si];
		if(shaderName.empty()) continue;

		outTextures[g] = resolveTexture(shaderName, ddsCatalog);
	}
}

} // namespace viewer::scene
