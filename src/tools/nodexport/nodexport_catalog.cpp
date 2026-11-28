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

// NODExport catalog <build> <out.json>: what a browser of the build lists.
//
// templates: every template of Misc/Templates/Global.not, resolved through
// its basedon chain: name, model (id 4), motion set (id 13), script (id 15),
// type (id 20) and model scale (id 143), as the last value of each counts.
// motionSets: every motion set of 3D/Motions with its entries.
// models: every model of 3D/Models with its header counts and vertex types.
// Names are as the files give them; matching them to files is left to the
// reader. A template, motion set or model that is refused is listed with
// the reason, and the rest are still exported.

#include "NMSParser.h"
#include "NODCheck.h"
#include "NODParser.h"
#include "NOTDatabase.h"
#include "export.h"

#include <cstring>

namespace nodexport {

	namespace {

		uint32_t u32(const uint8_t *p) {
			uint32_t v;
			std::memcpy(&v, p, 4);
			return v;
		}

		std::string refused(const std::string &name, const std::string &error) {
			return "{\"name\":" + survey::quote(name) + ",\"error\":" + survey::quote(error) + "}";
		}

		std::string text(const std::string &value) { return value.empty() ? "null" : survey::quote(value); }

		std::string templates(const fs::path &build, std::vector<std::string> &bad) {
			NOTDatabase db;
			std::string error;
			if(!db.load(build, {}, error)) {
				bad.push_back(refused("Global.not", error));
				return "[]";
			}
			std::vector<std::string> out;
			for(const auto &[key, entry] : db.templates()) {
				std::vector<NOTParser::ResolvedProperty> resolved;
				if(db.resolveModifiers(entry->name, resolved, error) != NOTParser::ResolveStatus::ok) {
					bad.push_back(refused(entry->name, error));
					continue;
				}
				std::string model, motionSet, script, scale = "null", type = "null";
				error.clear();
				for(const auto &r : resolved) {
					const NODModifier &p = *r.property;
					std::string value;
					if(p.prop_id == 4 || p.prop_id == 13 || p.prop_id == 15) {
						if(!nodModifierString(p, value)) {
							error = "property " + std::to_string(p.prop_id) + " is not text";
							break;
						}
						(p.prop_id == 4 ? model : p.prop_id == 13 ? motionSet : script) = value;
					} else if(p.prop_id == 20) {
						type = std::to_string(u32(p.value));
					} else if(p.prop_id == 143) {
						float v;
						std::memcpy(&v, p.value, 4);
						scale = exact(v);
					}
				}
				if(!error.empty()) {
					bad.push_back(refused(entry->name, error));
					continue;
				}
				out.push_back("{\"name\":" + survey::quote(entry->name) + ",\"model\":" + text(model) +
							  ",\"motionSet\":" + text(motionSet) + ",\"script\":" + text(script) + ",\"type\":" + type +
							  ",\"scale\":" + scale + "}");
			}
			return survey::list(out);
		}

		std::string motionSets(const fs::path &dir, std::vector<std::string> &bad) {
			std::vector<std::string> out;
			if(!fs::is_directory(dir)) return "[]";
			for(const auto &path : survey::filesOf(dir, ".nms")) {
				NMSParser parser;
				if(!parser.parse(path.string().c_str())) {
					bad.push_back(refused(path.filename().string(), "not a motion set"));
					continue;
				}
				const auto &ms = parser.getMotionSetData();
				std::vector<std::string> entries;
				for(uint32_t e = 0; e < ms.entry_count; ++e) {
					const auto &entry = ms.entries[e];
					std::vector<std::string> flags;
					for(uint32_t f = 0; f < entry.flag_count; ++f) flags.push_back(entry.flags[f]);
					entries.push_back("{\"name\":" + survey::quote(entry.motion_name) +
									  ",\"clip\":" + survey::quote(entry.animation_file) +
									  ",\"flags\":" + survey::quotedList(flags) + "}");
				}
				out.push_back("{\"file\":" + survey::quote(path.filename().string()) +
							  ",\"entries\":" + survey::list(entries) + "}");
			}
			return survey::list(out);
		}

		// The checks of the nod mode, without reading the vertices.
		bool modelFault(const NODParser &parser, std::string &error) {
			const auto &m = parser.getModelData();
			if(m.header.flags != 10) error = "version " + std::to_string(m.header.flags) + ", not 10";
			else if(m.bytes_read != m.file_size)
				error = std::to_string(m.file_size - m.bytes_read) + " bytes the layout does not explain";
			else {
				const auto faults = nodFaults(m);
				if(!faults.empty())
					error = (faults.front().group >= 0 ? "group " + std::to_string(faults.front().group) + ": " : "") +
							faults.front().what;
			}
			return !error.empty();
		}

		std::string models(const fs::path &dir, std::vector<std::string> &bad) {
			std::vector<std::string> out;
			if(!fs::is_directory(dir)) return "[]";
			for(const auto &path : survey::filesOf(dir, ".nod")) {
				NODParser parser;
				std::string error;
				if(!parser.parse(path.string().c_str())) error = parser.getLastError();
				if(error.empty()) modelFault(parser, error);
				if(!error.empty()) {
					bad.push_back(refused(path.filename().string(), error));
					continue;
				}
				const auto &m = parser.getModelData();
				std::string types = "[";
				for(uint32_t s = 0; s < m.vertex_set_count; ++s) types += (s ? "," : "") + std::to_string(m.vertex_sets[s].type);
				out.push_back("{\"file\":" + survey::quote(path.filename().string()) +
							  ",\"materials\":" + std::to_string(m.shader_count) + ",\"bones\":" + std::to_string(m.bone_count) +
							  ",\"groups\":" + std::to_string(m.mesh_group_count) + ",\"vertexTypes\":" + types + "]}");
			}
			return survey::list(out);
		}

	} // namespace

	bool exportCatalog(const char *build, std::string &json) {
		const fs::path root(build);
		if(!fs::is_directory(root / "3D" / "Models")) {
			json = refusal("catalog", "no 3D/Models in " + root.string());
			return false;
		}
		std::vector<std::string> badTemplates, badSets, badModels;
		const std::string t = templates(root, badTemplates);
		const std::string s = motionSets(root / "3D" / "Motions", badSets);
		const std::string m = models(root / "3D" / "Models", badModels);
		json = "{\"format\":\"catalog\",\"templates\":" + t + ",\"refusedTemplates\":" + survey::list(badTemplates) +
			   ",\"motionSets\":" + s + ",\"refusedMotionSets\":" + survey::list(badSets) + ",\"models\":" + m +
			   ",\"refusedModels\":" + survey::list(badModels) + "}\n";
		return true;
	}

} // namespace nodexport
