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

#include "NODMaterialLibrary.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

	std::string trim(const std::string &s) {
		const auto first = s.find_first_not_of(" \t\r\n");
		if(first == std::string::npos) return "";
		const auto last = s.find_last_not_of(" \t\r\n");
		return s.substr(first, last - first + 1);
	}

	// The lines of a text file that are not blank and not a // or ; comment.
	std::vector<std::string> listLines(const fs::path &path) {
		std::vector<std::string> out;
		std::ifstream in(path);
		std::string line;
		while(std::getline(in, line)) {
			const std::string t = trim(line);
			if(t.empty() || t[0] == ';' || t[0] == '/') continue;
			out.push_back(t);
		}
		return out;
	}

	// `relative` under `root`, each part matched without case ('\' or '/'
	// between parts). Empty when a part does not exist.
	fs::path findPath(const fs::path &root, const std::string &relative) {
		fs::path at = root;
		std::string part;
		auto step = [&](const std::string &name) {
			if(name.empty() || at.empty()) return;
			fs::path match;
			std::error_code ec;
			for(const auto &entry : fs::directory_iterator(at, ec))
				if(NODMaterialLibrary::lower(entry.path().filename().string()) == NODMaterialLibrary::lower(name)) {
					match = entry.path();
					break;
				}
			at = match;
		};
		for(char c : relative) {
			if(c == '\\' || c == '/') {
				step(part);
				part.clear();
			} else {
				part += c;
			}
		}
		step(part);
		return at;
	}

} // namespace

std::string NODMaterialLibrary::lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	return s;
}

void NODMaterialLibrary::indexTextures(const std::string &folder) {
	std::vector<fs::path> files;
	std::error_code ec;
	for(const auto &entry : fs::recursive_directory_iterator(folder, ec))
		if(entry.is_regular_file() && lower(entry.path().extension().string()) == ".dds") files.push_back(entry.path());
	std::sort(files.begin(), files.end());
	for(const auto &file : files) {
		const std::string key = lower(file.stem().string());
		const auto it		  = textures_.find(key);
		if(it == textures_.end()) {
			textures_[key] = file.string();
			continue;
		}
		auto &places = duplicateTextures_[key];
		if(places.empty()) places.push_back(it->second);
		places.push_back(file.string());
	}
}

bool NODMaterialLibrary::load(const std::string &buildRoot, std::string &error) {
	root_			  = buildRoot;
	const fs::path materials = findPath(buildRoot, "Materials");
	if(materials.empty()) {
		error = "no Materials folder in " + buildRoot;
		return false;
	}

	// archives: the listed ones in list order, then the rest by name
	std::map<std::string, std::string> onDisk; // lower stem -> file name
	for(const auto &entry : fs::directory_iterator(materials))
		if(entry.is_regular_file() && lower(entry.path().extension().string()) == ".nsa")
			onDisk[lower(entry.path().stem().string())] = entry.path().filename().string();
	std::vector<std::pair<std::string, bool>> order;
	const fs::path list = findPath(materials, "shaderlist.txt");
	if(!list.empty())
		for(const std::string &name : listLines(list)) {
			const auto it = onDisk.find(lower(name));
			if(it == onDisk.end()) {
				missingArchives_.push_back(name);
				continue;
			}
			order.emplace_back(it->second, true);
			onDisk.erase(it);
		}
	std::vector<std::string> rest;
	for(const auto &[key, file] : onDisk) rest.push_back(file);
	std::sort(rest.begin(), rest.end(), [](const std::string &a, const std::string &b) { return lower(a) < lower(b); });
	for(const auto &file : rest) order.emplace_back(file, false);

	for(const auto &[file, listed] : order) {
		Archive archive{file, listed, std::make_unique<NSAParser>()};
		if(!archive.parser->parse((materials / file).string().c_str())) {
			error = "cannot read " + (materials / file).string();
			return false;
		}
		const auto &md	   = archive.parser->getMaterialsData();
		const uint32_t idx = uint32_t(archives_.size());
		for(uint32_t m = 0; m < md.material_count; ++m) definitions_[lower(md.materials[m].name)].push_back({idx, m});
		archives_.push_back(std::move(archive));
	}

	// .nas: one material each, found by file name
	std::error_code ec;
	for(const auto &entry : fs::recursive_directory_iterator(materials, ec)) {
		if(!entry.is_regular_file() || lower(entry.path().extension().string()) != ".nas") continue;
		auto parser = std::make_unique<NSAParser>();
		if(!parser->parse(entry.path().string().c_str())) continue;
		const auto &md = parser->getMaterialsData();
		if(md.material_count > 0) nasByName_[lower(entry.path().stem().string())] = {&md.materials[0], entry.path().string()};
		nas_.push_back(std::move(parser));
	}

	// textures: 3D/Materials, then MaterialDirs.txt in order
	const fs::path models = findPath(buildRoot, "3D/Materials");
	if(!models.empty()) indexTextures(models.string());
	const fs::path dirs = findPath(buildRoot, "MaterialDirs.txt");
	if(!dirs.empty())
		for(const std::string &folder : listLines(dirs)) {
			const fs::path at = findPath(buildRoot, folder);
			if(at.empty()) {
				missingFolders_.push_back(folder);
				continue;
			}
			indexTextures(at.string());
		}
	return true;
}

NODMaterialLibrary::Found NODMaterialLibrary::resolve(const std::string &materialName) const {
	Found found;
	const std::string key = lower(materialName);
	const auto def		  = definitions_.find(key);
	if(def != definitions_.end() && !def->second.empty()) {
		const Definition &d	   = def->second.front();
		const Archive &archive = archives_[d.archive];
		found.step			   = STEP_ARCHIVE;
		found.material		   = &archive.parser->getMaterialsData().materials[d.material];
		found.source		   = archive.file;
		found.line			   = found.material->line;
		return found;
	}
	const auto nas = nasByName_.find(key);
	if(nas != nasByName_.end()) {
		found.step	   = STEP_NAS;
		found.material = nas->second.first;
		found.source   = nas->second.second;
		found.line	   = found.material->line;
		return found;
	}
	const std::string texture = findTexture(materialName);
	if(!texture.empty()) {
		found.step	 = STEP_TEXTURE;
		found.source = texture;
	}
	return found;
}

std::string NODMaterialLibrary::findTexture(const std::string &textureName) const {
	std::string name = trim(textureName);
	const auto blank = name.find_first_of(" \t");
	if(blank != std::string::npos) name = name.substr(0, blank);
	const auto slash = name.find_last_of("/\\");
	if(slash != std::string::npos) name = name.substr(slash + 1);
	std::string key = lower(name);
	if(key.size() > 4 && (key.compare(key.size() - 4, 4, ".dds") == 0 || key.compare(key.size() - 4, 4, ".tga") == 0))
		key.resize(key.size() - 4);
	const auto it = textures_.find(key);
	return it == textures_.end() ? std::string() : it->second;
}
