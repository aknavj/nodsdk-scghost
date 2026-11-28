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

// NODSurvey nms <build> <out.json>: every motion set of 3D/Motions, its
// flags, and whether each clip it names is in 3D/Anims: by exact name, only
// without case, or only with ".nad" appended to a name that has no extension.
// The survey records these classes; it does not claim what the engine does.

#include "NMSParser.h"
#include "report.h"

#include <set>

namespace survey {

	bool surveyNms(const fs::path &buildRoot, std::string &json) {
		const fs::path motions = buildRoot / "3D" / "Motions";
		const fs::path anims   = buildRoot / "3D" / "Anims";
		const fs::path models  = buildRoot / "3D" / "Models";
		if(!fs::is_directory(motions) || !fs::is_directory(anims)) {
			json = "{\"format\":\"nms\",\"error\":\"no 3D/Motions or 3D/Anims\"}\n";
			return false;
		}
		std::set<std::string> clipsExact;
		std::map<std::string, std::vector<std::string>> clipsLower; // lower name -> names on disk
		for(const auto &path : filesOf(anims, ".nad")) {
			const std::string name = path.filename().string();
			clipsExact.insert(name);
			clipsLower[lower(name)].push_back(name);
		}
		std::vector<std::string> caseTwins; // clip names that differ only by case
		for(const auto &[key, names] : clipsLower)
			if(names.size() > 1) caseTwins.push_back(key);
		std::set<std::string> modelStems;
		if(fs::is_directory(models))
			for(const auto &path : filesOf(models, ".nod")) modelStems.insert(lower(path.stem().string()));

		uint32_t files = 0, read = 0, entries = 0, shortLines = 0, droppedFlags = 0, withModel = 0;
		uint32_t exact = 0, byCase = 0, byAddedExt = 0, missing = 0, missingNoExt = 0;
		std::map<std::string, uint32_t> flags;
		std::set<std::string> named, missingClips;
		std::vector<std::string> refused;
		for(const auto &path : filesOf(motions, ".nms")) {
			++files;
			NMSParser parser;
			if(!parser.parse(path.string().c_str())) {
				refused.push_back(quote(path.filename().string()));
				continue;
			}
			++read;
			if(modelStems.count(lower(path.stem().string()))) ++withModel;
			const auto &ms = parser.getMotionSetData();
			entries += ms.entry_count;
			shortLines += ms.short_lines;
			droppedFlags += ms.dropped_flags;
			for(uint32_t e = 0; e < ms.entry_count; ++e) {
				const auto &entry = ms.entries[e];
				for(uint32_t f = 0; f < entry.flag_count; ++f) ++flags[lower(entry.flags[f])];
				const std::string clip = entry.animation_file;
				named.insert(lower(clip));
				const bool hasExt = lower(fs::path(clip).extension().string()) == ".nad";
				if(clipsExact.count(clip))
					++exact;
				else if(clipsLower.count(lower(clip)))
					++byCase;
				else if(!hasExt && clipsLower.count(lower(clip + ".nad")))
					++byAddedExt;
				else {
					++missing;
					if(!hasExt) ++missingNoExt;
					missingClips.insert(clip);
				}
			}
		}
		json = "{\"format\":\"nms\",\"files\":" + std::to_string(files) + ",\"read\":" + std::to_string(read) +
			   ",\"withModelOfSameName\":" + std::to_string(withModel) + ",\"entries\":" + std::to_string(entries) +
			   ",\"shortLines\":" + std::to_string(shortLines) + ",\"droppedFlags\":" + std::to_string(droppedFlags) +
			   ",\"flags\":" + histogram(flags) + ",\"clipsNamed\":" + std::to_string(named.size()) +
			   ",\"entriesExact\":" + std::to_string(exact) + ",\"entriesByCase\":" + std::to_string(byCase) +
			   ",\"entriesByAddedExt\":" + std::to_string(byAddedExt) +
			   ",\"entriesMissing\":" + std::to_string(missing) +
			   ",\"entriesMissingNoExt\":" + std::to_string(missingNoExt) +
			   ",\"missingClipCount\":" + std::to_string(missingClips.size()) +
			   ",\"missingClips\":" + quotedList(std::vector<std::string>(missingClips.begin(), missingClips.end())) +
			   ",\"clipCaseTwins\":" + quotedList(caseTwins) + ",\"refused\":" + list(refused) + "}\n";
		return refused.empty() && shortLines == 0 && droppedFlags == 0 && caseTwins.empty();
	}

} // namespace survey
