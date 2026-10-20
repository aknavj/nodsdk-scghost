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

#include "NOTDatabase.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <cmath>

/*
 */
namespace {

/*
 */
std::string lower(std::string value) {
	for(auto &c : value) if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return value;
}

} // namespace

/*
 */
bool NOTDatabase::archiveName(std::string value, std::string &name, std::string &error) {
	error.clear();
	std::replace(value.begin(), value.end(), '\\', '/');
	if(value.empty() || value.front() == '/' || value.find(':') != std::string::npos ||
	   std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c > 126; })) {
		error = "Invalid template archive reference: " + value;
		return false;
	}
	size_t start = 0;
	while(start <= value.size()) {
		const auto end = value.find('/', start);
		const auto part = value.substr(start, end - start);
		if(part.empty() || part == "." || part == "..") {
			error = "Unsupported template archive path: " + value;
			return false;
		}
		if(end == std::string::npos) break;
		start = end + 1;
	}
	const auto filename = value.substr(value.find_last_of('/') == std::string::npos ? 0 : value.find_last_of('/') + 1);
	if(filename.find('.') == std::string::npos) value += ".not";
	if(value.size() >= 128) { error = "Template archive path exceeds engine filename capacity"; return false; }
	name = lower(value);
	return true;
}

/*
 */
namespace {

/*
 */
bool read_archive(const std::filesystem::path &root, const std::string &name,
				  std::vector<uint8_t> &bytes, std::string &error, bool &missing, std::filesystem::path *resolved_path) {
	missing = false;
	std::filesystem::path path = root;
	const auto relative = std::filesystem::path("Misc") / "Templates" / std::filesystem::u8path(name);
	for(const auto &component : relative) {
		std::error_code ec;
		std::filesystem::directory_iterator entries(path, ec);
		if(ec) {
			missing = ec == std::errc::no_such_file_or_directory;
			error = "Cannot inspect template directory " + path.u8string() + ": " + ec.message();
			return false;
		}
		std::filesystem::path match;
		for(auto it = entries; it != std::filesystem::directory_iterator(); it.increment(ec)) {
			if(ec) break;
			if(lower(it->path().filename().u8string()) != lower(component.u8string())) continue;
			if(!match.empty()) { error = "Ambiguous template archive path: " + name; return false; }
			match = it->path();
		}
		if(ec) { error = "Cannot enumerate template directory: " + ec.message(); return false; }
		if(match.empty()) { missing = true; error = "Missing template archive: " + name; return false; }
		path = std::move(match);
	}
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if(!file) { error = "Cannot open template archive: " + path.u8string(); return false; }
	const auto size = file.tellg();
	if(size < 0 || uintmax_t(size) > std::numeric_limits<size_t>::max() ||
	   uintmax_t(size) > uintmax_t(std::numeric_limits<std::streamsize>::max())) {
		error = "Cannot determine template archive size: " + name;
		return false;
	}
	bytes.resize(size_t(size));
	file.seekg(0);
	if(!bytes.empty() && !file.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()))) {
		error = "Cannot read template archive: " + name;
		return false;
	}
	if(resolved_path) *resolved_path = path;
	return true;
}

} // namespace

/*
 */
bool NOTDatabase::readArchive(const std::filesystem::path &root, const std::string &name, Snapshot &out,
							  std::string &error, const std::filesystem::path &fallback_root,
							  std::filesystem::path *resolved_path) {
	error.clear();
	Snapshot next;
	if(!archiveName(name, next.name, error)) return false;
	bool missing;
	std::filesystem::path path;
	if(!read_archive(root, next.name, next.bytes, error, missing, &path)) {
		if(!missing || fallback_root.empty()) return false;
		error.clear();
		if(!read_archive(fallback_root, next.name, next.bytes, error, missing, &path)) return false;
	}
	out = std::move(next);
	if(resolved_path) *resolved_path = std::move(path);
	return true;
}

/*
 */
bool NOTDatabase::load(const std::filesystem::path &game_root, const std::vector<std::string> &scene_archives,
					   std::string &error, const std::filesystem::path &fallback_root) {
	error.clear();
	std::vector<std::string> requests{"Global.not"};
	requests.insert(requests.end(), scene_archives.begin(), scene_archives.end());
	std::vector<Snapshot> snapshots;
	std::set<std::string> loaded;
	for(const auto &request : requests) {
		std::string name;
		if(!archiveName(request, name, error)) return false;
		if(!loaded.insert(name).second) continue;
		if(snapshots.size() == 64) { error = "More than 64 open template archives"; return false; }
		Snapshot snapshot;
		if(!readArchive(game_root, name, snapshot, error, fallback_root)) return false;
		snapshots.push_back(std::move(snapshot));
	}
	return loadBuffers(snapshots, error);
}

/*
 */
bool NOTDatabase::loadBuffers(const std::vector<Snapshot> &archives, std::string &error,
							 const std::vector<std::string> &required) {
	error.clear();
	NOTDatabase next;
	std::set<std::string> loaded;
	for(const auto &archive : archives) {
		std::string name;
		if(!archiveName(archive.name, name, error)) return false;
		if(!loaded.insert(name).second) continue;
		if(next.snapshots_.size() == 64) { error = "More than 64 open template archives"; return false; }
		auto parser = std::make_unique<NOTParser>();
		if(!parser->parseBuffer(archive.bytes.data(), archive.bytes.size())) {
			error = name + ": " + parser->getLastError();
			return false;
		}
		const size_t index = next.snapshots_.size();
		next.snapshots_.push_back({name, archive.bytes});
		const auto &db = parser->getTemplateDB();
		for(uint32_t i = 0; i < db.num_templates; ++i) {
			const auto *entry = &db.templates[i];
			next.sources_.emplace(entry, index);
			const auto inserted = next.templates_.emplace(lower(entry->name), entry);
			if(!inserted.second)
				next.warnings_.push_back(name + ": template " + entry->name + " is shadowed by first-loaded " +
					next.source(inserted.first->second)->name);
		}
		next.parsers_.push_back(std::move(parser));
	}
	for(const auto &request : required) {
		std::string name;
		if(!archiveName(request, name, error)) return false;
		if(!loaded.count(name)) { error = "Missing required template archive snapshot: " + name; return false; }
	}
	*this = std::move(next);
	return true;
}

/*
 */
const NOTParser::Template *NOTDatabase::findTemplate(const char *name) const {
	if(!name) return nullptr;
	const auto found = templates_.find(lower(name));
	return found == templates_.end() ? nullptr : found->second;
}

/*
 */
const NOTDatabase::Snapshot *NOTDatabase::source(const NOTParser::Template *owner) const {
	const auto found = sources_.find(owner);
	return found == sources_.end() ? nullptr : &snapshots_[found->second];
}

/*
 */
NOTParser::ResolveStatus NOTDatabase::resolveModifiers(const char *name,
		std::vector<NOTParser::ResolvedProperty> &out, std::string &error) const {
	NOTParser resolver;
	return resolver.resolveModifiers(name, out, error, [this](const char *reference) { return findTemplate(reference); });
}

/*
 */
NOTParser::ResolveStatus NOTDatabase::resolveModifiers(const NODModifier *properties, size_t count,
		std::vector<NOTParser::ResolvedProperty> &out, std::string &error) const {
	NOTParser resolver;
	return resolver.resolveModifiers(properties, count, out, error,
		[this](const char *reference) { return findTemplate(reference); });
}

/*
 */
bool NOTDatabase::modelPreview(const std::vector<NOTParser::ResolvedProperty> &properties,
							  ModelPreview &out, std::string &error) {
	out = {};
	error.clear();
	ModelPreview result;
	uint32_t render_flags = 0;
	const NODModifier *offset = nullptr;
	for(const auto &resolved : properties) {
		if(!resolved.property || resolved.property->value_size != 32) {
			error = "Invalid resolved model modifier";
			return false;
		}
		const auto &property = *resolved.property;
		switch(property.prop_id) {
		case 4:
			if(!nodModifierString(property, result.model)) { error = "invalid model reference"; return false; }
			result.owner = resolved.owner;
			result.scale = 1;
			break;
		case 12:
			render_flags = FileUtils::readUInt32(property.value, 0);
			break;
		case 143:
			// The object handler ignores scale before a model instance exists.
			if(!result.model.empty()) {
				result.scale = FileUtils::readFloat(property.value, 0);
				if(!std::isfinite(result.scale)) { error = "Non-finite MODELSCALE"; return false; }
			}
			break;
		case 175:
			offset = &property;
			break;
		}
	}
	if(!result.model.empty() && (render_flags & 0x2000)) {
		if(!offset) { error = "Model offset enabled by renderflags without MODELOFFSET"; return false; }
		for(size_t axis = 0; axis < 3; ++axis) {
			result.offset[axis] = FileUtils::readFloat(offset->value, axis * 4);
			if(!std::isfinite(result.offset[axis])) { error = "Non-finite active MODELOFFSET"; return false; }
		}
	}
	out = std::move(result);
	return true;
}
