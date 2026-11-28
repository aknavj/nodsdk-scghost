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

// NODSurvey not <build> <out.json>: every template of Misc/Templates/Global.not,
// resolved through its basedon chain. For each: whether it resolves, its
// model (id 4) and motion set (id 13), and the match class of each in
// 3D/Models and 3D/Motions: exact, only without case, only with the
// extension appended to a name without one, or missing. The survey records
// the classes; it does not claim what the engine does.

#include "NOTDatabase.h"
#include "report.h"

#include <set>

namespace survey {

	namespace {

		struct Index {
			std::set<std::string> exact, lower;
		};

		Index indexOf(const fs::path &dir, const std::string &ext) {
			Index index;
			if(!fs::is_directory(dir)) return index;
			for(const auto &path : filesOf(dir, ext)) {
				index.exact.insert(path.filename().string());
				index.lower.insert(lower(path.filename().string()));
			}
			return index;
		}

		struct Matches {
			std::map<std::string, uint32_t> classes; // exact, case, addedExt, missing
			std::set<std::string> names, missing;
		};

		void match(const Index &index, const std::string &ext, const std::string &name, Matches &m) {
			m.names.insert(lower(name));
			const bool hasExt = lower(fs::path(name).extension().string()) == ext;
			if(index.exact.count(name))
				++m.classes["exact"];
			else if(index.lower.count(lower(name)))
				++m.classes["case"];
			else if(!hasExt && index.lower.count(lower(name + ext)))
				++m.classes["addedExt"];
			else {
				++m.classes["missing"];
				m.missing.insert(name);
			}
		}

		std::string matchJson(const Matches &m) {
			return "{\"templates\":" + histogram(m.classes) + ",\"names\":" + std::to_string(m.names.size()) +
				   ",\"missing\":" + quotedList(std::vector<std::string>(m.missing.begin(), m.missing.end())) + "}";
		}

		const char *statusName(NOTParser::ResolveStatus status) {
			switch(status) {
			case NOTParser::ResolveStatus::ok: return "ok";
			case NOTParser::ResolveStatus::missing_template: return "missing_template";
			case NOTParser::ResolveStatus::cycle: return "cycle";
			case NOTParser::ResolveStatus::too_many: return "too_many";
			case NOTParser::ResolveStatus::invalid_modifiers: return "invalid_modifiers";
			}
			return "unknown";
		}

	} // namespace

	bool surveyNot(const fs::path &buildRoot, std::string &json) {
		NOTDatabase db;
		std::string error;
		if(!db.load(buildRoot, {}, error)) {
			json = "{\"format\":\"not\",\"error\":" + quote(error) + "}\n";
			return false;
		}
		const Index models  = indexOf(buildRoot / "3D" / "Models", ".nod");
		const Index motions = indexOf(buildRoot / "3D" / "Motions", ".nms");

		uint32_t templates = 0, incomplete = 0, withExtra = 0, withTrailing = 0, withModel = 0, withMotion = 0;
		uint32_t motionWithoutModel = 0, unknownIds = 0;
		std::map<std::string, uint32_t> status, ownIds;
		std::vector<std::string> refused, badStrings;
		Matches modelMatch, motionMatch;
		for(const auto &[key, entry] : db.templates()) {
			++templates;
			if(!entry->properties_complete) ++incomplete;
			if(entry->extra_size) ++withExtra;
			if(entry->trailing_size) ++withTrailing;
			for(uint32_t p = 0; p < entry->property_count; ++p) {
				const uint32_t id = entry->properties[p].prop_id;
				if(!nodPropertyInfo(id)) ++unknownIds;
				++ownIds[std::to_string(id)];
			}
			std::vector<NOTParser::ResolvedProperty> resolved;
			const auto result = db.resolveModifiers(entry->name, resolved, error);
			++status[statusName(result)];
			if(result != NOTParser::ResolveStatus::ok) {
				refused.push_back(quote(std::string(entry->name) + ": " + error));
				continue;
			}
			std::string model, motion;
			bool stringsOk = true;
			for(const auto &r : resolved) {
				std::string value;
				if(r.property->prop_id != 4 && r.property->prop_id != 13) continue;
				if(!nodModifierString(*r.property, value)) {
					stringsOk = false;
					continue;
				}
				(r.property->prop_id == 4 ? model : motion) = value; // the last one counts
			}
			if(!stringsOk) badStrings.push_back(quote(entry->name));
			if(!model.empty()) {
				++withModel;
				match(models, ".nod", model, modelMatch);
			}
			if(!motion.empty()) {
				++withMotion;
				if(model.empty()) ++motionWithoutModel;
				match(motions, ".nms", motion, motionMatch);
			}
		}
		json = "{\"format\":\"not\",\"archives\":" + std::to_string(db.snapshots().size()) +
			   ",\"templates\":" + std::to_string(templates) + ",\"incomplete\":" + std::to_string(incomplete) +
			   ",\"withExtra\":" + std::to_string(withExtra) + ",\"withTrailing\":" + std::to_string(withTrailing) +
			   ",\"shadowed\":" + std::to_string(db.warnings().size()) + ",\"unknownIds\":" + std::to_string(unknownIds) +
			   ",\"ownIds\":" + histogram(ownIds) + ",\"resolve\":" + histogram(status) +
			   ",\"withModel\":" + std::to_string(withModel) + ",\"withMotionSet\":" + std::to_string(withMotion) +
			   ",\"motionSetWithoutModel\":" + std::to_string(motionWithoutModel) +
			   ",\"models\":" + matchJson(modelMatch) + ",\"motionSets\":" + matchJson(motionMatch) +
			   ",\"badStrings\":" + list(badStrings) + ",\"refused\":" + list(refused) + "}\n";
		return refused.empty() && badStrings.empty() && incomplete == 0 && unknownIds == 0;
	}

} // namespace survey
