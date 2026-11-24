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

// NODSurvey nsa <build> <out.json>: the material archives of a build, and
// how every material name of the models in 3D/Models is found
// (NODMaterialLibrary).

#include "NODMaterialLibrary.h"
#include "NODParser.h"
#include "report.h"

#include <cstring>
#include <set>

namespace survey {

	namespace {

		struct StepCount {
			uint32_t names = 0, slots = 0;
		};

	} // namespace

	bool surveyNsa(const fs::path &buildRoot, std::string &json) {
		NODMaterialLibrary library;
		std::string error;
		if(!library.load(buildRoot.string(), error)) {
			json = "{\"format\":\"nsa\",\"error\":" + quote(error) + "}\n";
			return false;
		}

		uint32_t blocks = 0, comments = 0, opens = 0, closes = 0, dropped = 0, droppedDrawn = 0, listed = 0, nested = 0, maxProps = 0;
		std::vector<std::string> archives, unclosed, stray, droppedAt;
		std::map<std::string, uint32_t> keys, shaders, nestedShaders;
		std::set<std::string> textureNames;
		for(const auto &archive : library.archives()) {
			const auto &md = archive.parser->getMaterialsData();
			archives.push_back(archive.file);
			if(archive.listed) ++listed;
			blocks += md.material_count;
			comments += md.comment_lines;
			opens += md.open_braces;
			closes += md.close_braces;
			if(md.unclosed_line)
				unclosed.push_back("{\"file\":" + quote(archive.file) + ",\"line\":" + std::to_string(md.unclosed_line) + "}");
			if(md.stray_close_line)
				stray.push_back("{\"file\":" + quote(archive.file) + ",\"line\":" + std::to_string(md.stray_close_line) + "}");
			for(uint32_t m = 0; m < md.material_count; ++m) {
				const auto &material = md.materials[m];
				dropped += material.dropped_props;
				maxProps = std::max(maxProps, material.prop_count + material.dropped_props);
				std::string shader = "(none)";
				for(uint32_t p = 0; p < material.prop_count; ++p) {
					const std::string key = lower(material.props[p].key);
					++keys[key];
					if(key == "shader") {
						shader = lower(material.props[p].value);
						++shaders[shader];
					}
					if(key == "texture") {
						std::string name = material.props[p].value;
						const auto blank = name.find_first_of(" \t");
						if(blank != std::string::npos) name = name.substr(0, blank);
						if(!name.empty()) textureNames.insert(lower(name));
					}
				}
				if(material.nested_blocks) {
					++nested;
					++nestedShaders[shader];
				}
				// a dynamic material keeps its passes as properties; it is not drawn
				if(material.dropped_props) {
					droppedAt.push_back("{\"file\":" + quote(archive.file) + ",\"line\":" + std::to_string(material.line) +
										",\"shader\":" + quote(shader) + ",\"dropped\":" + std::to_string(material.dropped_props) + "}");
					if(shader != "dynamic") droppedDrawn += material.dropped_props;
				}
			}
		}
		uint32_t redefined = 0;
		for(const auto &[name, defs] : library.definitions())
			if(defs.size() > 1) ++redefined;

		std::vector<std::string> missingTextures;
		for(const auto &name : textureNames)
			if(library.findTexture(name).empty()) missingTextures.push_back(name);

		// every material slot of every model, and the step that finds its name
		std::map<std::string, uint32_t> slotsOf; // lower name -> slots
		uint32_t models = 0, cut = 0;
		const fs::path modelsDir = buildRoot / "3D" / "Models";
		if(fs::is_directory(modelsDir))
			for(const auto &path : filesOf(modelsDir, ".nod")) {
				NODParser parser;
				if(!parser.parse(path.string().c_str())) continue;
				++models;
				const auto &m = parser.getModelData();
				for(uint32_t s = 0; s < m.shader_count; ++s) {
					if(std::strlen(m.shader_names[s]) == 32) ++cut;
					++slotsOf[lower(m.shader_names[s])];
				}
			}
		StepCount steps[5];
		std::vector<std::string> notFound;
		for(const auto &[name, slots] : slotsOf) {
			const auto found = library.resolve(name);
			steps[found.step].names += 1;
			steps[found.step].slots += slots;
			if(found.step == NODMaterialLibrary::STEP_NONE) notFound.push_back(name);
		}
		uint32_t slotTotal = 0;
		for(const auto &[name, slots] : slotsOf) slotTotal += slots;
		auto stepJson = [&](int step) {
			return "{\"names\":" + std::to_string(steps[step].names) + ",\"slots\":" + std::to_string(steps[step].slots) + "}";
		};

		std::vector<std::string> duplicateTextures;
		for(const auto &[name, places] : library.duplicateTextures()) duplicateTextures.push_back(name);

		json = "{\"format\":\"nsa\",\"archives\":" + std::to_string(archives.size()) + ",\"listed\":" + std::to_string(listed) +
			   ",\"archiveOrder\":" + quotedList(archives) + ",\"missingArchives\":" + quotedList(library.missingArchives()) +
			   ",\"blocks\":" + std::to_string(blocks) + ",\"names\":" + std::to_string(library.definitions().size()) +
			   ",\"redefined\":" + std::to_string(redefined) + ",\"commentLines\":" + std::to_string(comments) +
			   ",\"openBraces\":" + std::to_string(opens) + ",\"closeBraces\":" + std::to_string(closes) +
			   ",\"unclosed\":" + list(unclosed) + ",\"strayClose\":" + list(stray) +
			   ",\"droppedProperties\":" + std::to_string(dropped) + ",\"droppedDrawn\":" + std::to_string(droppedDrawn) +
			   ",\"droppedAt\":" + list(droppedAt) +
			   ",\"mostProperties\":" + std::to_string(maxProps) + ",\"withNestedBlocks\":" + std::to_string(nested) +
			   ",\"nestedShaders\":" + histogram(nestedShaders) + ",\"keys\":" + histogram(keys) +
			   ",\"shaders\":" + histogram(shaders) + ",\"textureFiles\":" + std::to_string(library.textureCount()) +
			   ",\"duplicateTextures\":" + quotedList(duplicateTextures) +
			   ",\"missingFolders\":" + quotedList(library.missingFolders()) +
			   ",\"texturesNamed\":" + std::to_string(textureNames.size()) + ",\"texturesMissing\":" + quotedList(missingTextures) +
			   ",\"models\":" + std::to_string(models) + ",\"materialNames\":" + std::to_string(slotsOf.size()) +
			   ",\"materialSlots\":" + std::to_string(slotTotal) + ",\"cutNames\":" + std::to_string(cut) +
			   ",\"lookup\":{\"archive\":" + stepJson(1) + ",\"nas\":" + stepJson(2) + ",\"texture\":" + stepJson(3) +
			   ",\"none\":" + stepJson(4) + "},\"notFound\":" + quotedList(notFound) + "}\n";
		return unclosed.empty() && stray.empty() && droppedDrawn == 0;
	}

} // namespace survey
