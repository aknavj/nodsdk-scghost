/*
===========================================================================
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

// NODExport materials <build> <names.txt> <out.json>: the material of each
// model material name in names.txt (one per line), found by
// NODMaterialLibrary. Per name: the lookup step (archive, nas, texture,
// none), its source file, its properties in file order, and the texture
// file of each texture property, relative to the build (null when missing).

#include "NODMaterialLibrary.h"
#include "export.h"

#include <fstream>

namespace nodexport {

	namespace {

		const char *stepName(NODMaterialLibrary::Step step) {
			switch(step) {
			case NODMaterialLibrary::STEP_ARCHIVE: return "archive";
			case NODMaterialLibrary::STEP_NAS: return "nas";
			case NODMaterialLibrary::STEP_TEXTURE: return "texture";
			case NODMaterialLibrary::STEP_NONE: return "none";
			}
			return "none";
		}

		bool isTextureKey(const std::string &key) {
			return key == "texture" || key == "texture2" || key == "glow" || key == "envmap" || key == "detail";
		}

		std::string relativeOrNull(const fs::path &root, const std::string &file) {
			if(file.empty()) return "null";
			const fs::path path = file;
			return survey::quote((path.is_absolute() ? path.lexically_relative(root) : path).generic_string());
		}

	} // namespace

	bool exportMaterials(const char *build, const char *names, std::string &json) {
		NODMaterialLibrary library;
		std::string error;
		if(!library.load(build, error)) {
			json = refusal("materials", error);
			return false;
		}
		std::ifstream list(names);
		if(!list) {
			json = refusal("materials", std::string("cannot read ") + names);
			return false;
		}
		const fs::path root = build;
		std::vector<std::string> out;
		std::string name;
		while(std::getline(list, name)) {
			while(!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
			if(name.empty()) continue;
			const auto found = library.resolve(name);
			std::vector<std::string> props, textures;
			if(found.material) {
				for(uint32_t p = 0; p < found.material->prop_count; ++p) {
					const auto &prop	  = found.material->props[p];
					const std::string key = NODMaterialLibrary::lower(prop.key);
					props.push_back("[" + survey::quote(prop.key) + "," + survey::quote(prop.value) + "]");
					if(isTextureKey(key))
						textures.push_back(survey::quote(key) + ":" + relativeOrNull(root, library.findTexture(prop.value)));
				}
			}
			if(found.step == NODMaterialLibrary::STEP_TEXTURE)
				textures.push_back("\"texture\":" + relativeOrNull(root, found.source));
			// an archive is named by its file under Materials; the other steps give a path
			const std::string source =
				found.step == NODMaterialLibrary::STEP_ARCHIVE ? (root / "Materials" / found.source).string() : found.source;
			std::string textureJson = "{";
			for(size_t i = 0; i < textures.size(); ++i) textureJson += (i ? "," : "") + textures[i];
			out.push_back("{\"name\":" + survey::quote(name) + ",\"step\":\"" + stepName(found.step) + "\",\"source\":" +
						  relativeOrNull(root, source) + ",\"line\":" + std::to_string(found.line) +
						  ",\"properties\":" + survey::list(props) + ",\"textures\":" + textureJson + "}}");
		}
		json = "{\"format\":\"materials\",\"materials\":" + survey::list(out) + "}\n";
		return true;
	}

} // namespace nodexport
