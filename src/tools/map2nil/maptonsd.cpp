/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: maptonsd.cpp
Version: 0.6.7

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

#include "scene_bridge.h"
#include "tool_cli.h"
#include "nod_assets.h"
#include "path_graph.h"
#include "map_lighting.h"
#include "NSDParser.h"
#include "../editors/document.h"
#include "../editors/workspace.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>

/*
 */
namespace {
namespace fs = std::filesystem;

/*
 */
int failure(const std::string &message) {
	std::cerr << "MAPtoNSD: " << message << '\n';
	return 1;
}

/*
 */
bool number(const std::string &text, uint32_t limit, uint32_t &value) {
	return nod_scene::parse_unsigned_integer(text.c_str(), limit, &value) && std::to_string(value) == text;
}

/*
 */
const char *property(const nod_scene::Entity *entity, const char *key) {
	const auto *value = nod_scene::get_property(entity, key);
	return value ? value : "";
}

/*
 */
void word(std::vector<uint8_t> &bytes, uint32_t value) {
	const auto offset = bytes.size();
	bytes.resize(offset + 4);
	put32(bytes.data() + offset, value);
}

/*
 */
void modifier(std::vector<uint8_t> &bytes, const NODModifier &value) {
	word(bytes, value.prop_id);
	bytes.insert(bytes.end(), std::begin(value.value), std::end(value.value));
}

/*
 */
NODModifier vector_modifier(uint32_t id, const float value[3]) {
	NODModifier result{};
	result.prop_id	  = id;
	result.value_size = 32;
	for(size_t i = 0; i < 3; ++i) {
		put_float(result.value + i * 4, value[i]);
	}

	return result;
}

/*
 */
const NODModifier *find(const std::vector<NOTParser::ResolvedProperty> &values, uint32_t id) {
	for(const auto &value : values) {
		if(value.property->prop_id == id) {
			return value.property;
		}
	}

	return nullptr;
}

/*
 */
bool archive_context(const std::string &text, std::vector<std::string> &archives, std::string &error) {
	if(text.empty()) {
		return true;
	}

	size_t start = 0;
	std::set<std::string> seen;
	do {
		const auto end	 = text.find(';', start);
		const auto entry = text.substr(start, end == std::string::npos ? end : end - start);
		std::string canonical;
		if(!NOTDatabase::archiveName(entry, canonical, error)) {
			return false;
		}

		if(entry != canonical || entry.size() > 31 || entry == "global.not" || !seen.insert(entry).second ||
		   archives.size() == 8) {
			error = "Use at most eight canonical, unique scene archives; Global is implicit";
			return false;
		}

		archives.push_back(entry);
		if(end == std::string::npos) {
			return true;
		}

		start = end + 1;
	} while(start <= text.size());
	return false;
}

/*
 */
bool imported(const nod_scene::Entity &entity, std::vector<NODModifier> &values, std::string &error) {
	const std::string count_text = property(&entity, "nod_modifier_count");
	if(count_text.empty()) {
		return true;
	}

	uint32_t count;
	if(!number(count_text, NOTParser::MAX_PROPERTIES, count) || count < 3) {
		error = "Invalid imported modifier count";
		return false;
	}

	const std::string digits = "0123456789abcdef";
	for(uint32_t i = 0; i < count; ++i) {
		const auto key		   = "nod_modifier_" + std::to_string(i);
		const std::string text = property(&entity, key.c_str());
		std::array<uint8_t, 36> bytes{};
		if(text.size() != bytes.size() * 2 || text.find_first_not_of(digits) != std::string::npos) {
			error = "Invalid " + key;
			return false;
		}

		for(size_t j = 0; j < bytes.size(); ++j) {
			bytes[j] = uint8_t(digits.find(text[j * 2]) * 16 + digits.find(text[j * 2 + 1]));
		}

		NODModifier value{};
		value.prop_id	 = u32(bytes.data());
		value.value_size = 32;
		std::copy(bytes.begin() + 4, bytes.end(), value.value);
		const auto *info = nodPropertyInfo(value.prop_id);
		if(!info) {
			error = "Unknown imported property";
			return false;
		}

		if(info->native_type == 0) {
			const auto end = std::find(std::begin(value.value), std::end(value.value), uint8_t(0));
			if(end == std::end(value.value) ||
			   std::any_of(std::begin(value.value), end, [](uint8_t c) { return c < 32 || c > 126; })) {
				error = "Invalid imported string";
				return false;
			}
		} else if(info->native_type == 2 || info->native_type == 4) {
			for(size_t j = 0; j < (info->native_type == 2 ? 1u : 3u); ++j) {
				if(!std::isfinite(f32(value.value + j * 4))) {
					error = "Invalid imported float";
					return false;
				}
			}
		} else if(info->native_type != 3) {
			error = "Unsupported imported property storage";
			return false;
		}

		values.push_back(value);
	}

	if(values[0].prop_id != 0 || values[1].prop_id != 1 || values[2].prop_id != 2) {
		error = "Imported placement requires basedon, rotation and position first";
		return false;
	}

	return true;
}

/*
 */
bool compile_entity(const nod_scene::Entity &entity, const NOTDatabase &templates, std::vector<uint8_t> &bytes,
					std::string &error) {
	if(entity.brush_count) {
		error = "Brush entities are unsupported";
		return false;
	}

	const std::string name		= property(&entity, "nod_template");
	const std::string classname = property(&entity, "classname");
	const std::string authoring = property(&entity, "nod_authoring");
	if(!((authoring == "not-template-v2" && classname == name) ||
		 (authoring == "map-template-v1" && classname == "nod_template_instance"))) {
		error = "Expected a native template placement";
		return false;
	}

	const auto *object = templates.findTemplate(name.c_str());
	const auto *source = object ? templates.source(object) : nullptr;
	if(!source || name != object->name || name.empty() || name.size() > 31 ||
	   source->name != property(&entity, "nod_template_archive")) {
		error = "Unresolved template or stale archive provenance";
		return false;
	}

	uint32_t expected_type;
	if(!number(property(&entity, "nod_template_type"), UINT32_MAX, expected_type)) {
		error = "Invalid native type metadata";
		return false;
	}

	float position[3];
	float angles[3];
	float rotation[3];
	if(!nod_scene::float_vector(property(&entity, "origin"), position) ||
	   !nod_scene::float_vector(property(&entity, "angles"), angles)) {
		error = "Expected finite origin and angles vectors";
		return false;
	}

	nod_angles_from_editor(angles, rotation);
	std::vector<NODModifier> values;
	values.reserve(NOTParser::MAX_PROPERTIES);
	if(!imported(entity, values, error)) {
		return false;
	}

	NODModifier reference{};
	reference.value_size = 32;
	std::copy(name.begin(), name.end(), reference.value);
	if(values.empty()) {
		values = {reference, vector_modifier(1, rotation), vector_modifier(2, position)};
	} else {
		std::string base;
		if(!nodModifierString(values[0], base) || base != name) {
			error = "Imported base template differs from placement metadata";
			return false;
		}

		for(size_t i = 0; i < 3; ++i) {
			put_float(values[1].value + i * 4, rotation[i]);
			put_float(values[2].value + i * 4, position[i]);
		}
	}

	NODModifier sector{};
	sector.prop_id	  = 3;
	sector.value_size = 32;
	if(values.size() > 3 && values[3].prop_id == 3) {
		put32(values[3].value, 0);
	} else {
		values.insert(values.begin() + 3, sector);
	}

	if(values.size() > NOTParser::MAX_PROPERTIES) {
		error = "Too many placement modifiers";
		return false;
	}

	std::vector<NOTParser::ResolvedProperty> resolved;
	if(templates.resolveModifiers(values.data(), values.size(), resolved, error) != NOTParser::ResolveStatus::ok) {
		return false;
	}

	const std::set<std::string> metadata{
		"classname",		 "nod_authoring", "nod_template", "nod_template_archive", "nod_template_archives",
		"nod_template_type", "origin",		  "angles",		  "nod_modifier_count"};
	std::set<std::string> keys;
	uint32_t count				 = 0;
	const std::string count_text = property(&entity, "nod_modifier_count");
	if(!count_text.empty()) {
		number(count_text, NOTParser::MAX_PROPERTIES, count);
	}

	bool bounds_changed = std::any_of(values.begin(), values.end(), [](const NODModifier &value) {
		return value.prop_id == 6 || value.prop_id == 7;
	});
	for(size_t i = 0; i < entity.pair_count; ++i) {
		const std::string key  = entity.pairs[i].key;
		const std::string text = entity.pairs[i].value;
		if(!keys.insert(key).second) {
			error = "Duplicate key: " + key;
			return false;
		}

		if(metadata.count(key)) {
			continue;
		}

		uint32_t id = 0;
		if(key.compare(0, 13, "nod_modifier_") == 0) {
			if(!number(key.substr(13), NOTParser::MAX_PROPERTIES - 1, id) || id >= count) {
				error = "Unexpected modifier key: " + key;
				return false;
			}

			continue;
		}

		if(key.compare(0, 9, "nod_prop_") != 0 || !number(key.substr(9), 176, id) ||
		   !nodPropertyMapEditable(id, false) || !find(resolved, id)) {
			error = "Unsupported or absent template property: " + key;
			return false;
		}

		NODModifier value{};
		if(!nod::edit::encode_modifier(id, text, value, error)) {
			error = key + ": " + error;
			return false;
		}

		if(values.size() == NOTParser::MAX_PROPERTIES) {
			error = "Too many placement modifiers";
			return false;
		}

		values.push_back(value);
		bounds_changed = bounds_changed || id == 6 || id == 7;
	}

	if(values.size() > NOTParser::MAX_PROPERTIES) {
		error = "Too many placement modifiers";
		return false;
	}

	if(templates.resolveModifiers(values.data(), values.size(), resolved, error) != NOTParser::ResolveStatus::ok) {
		return false;
	}

	const auto *type = find(resolved, 20);
	if(!type || u32(type->value) != expected_type) {
		error = "Stale or missing native type";
		return false;
	}

	if(expected_type != 0 && expected_type != 2 && expected_type != 7 && expected_type != 8) {
		error = "Template '" + name + "' has unsupported native type " + std::to_string(expected_type) +
				"; supported point types are 0, 2, 7 and 8";
		return false;
	}

	for(size_t i = 1; i <= 3; ++i) {
		const auto *effective = find(resolved, uint32_t(i));
		if(!effective || std::memcmp(effective->value, values[i].value, i == 3 ? 4 : 12)) {
			error = "Later modifiers override the placement transform or sector";
			return false;
		}
	}

	if(bounds_changed) {
		const auto *low	 = find(resolved, 6);
		const auto *high = find(resolved, 7);
		if(!low || !high) {
			error = "Editor bounds require MIN and MAX";
			return false;
		}

		for(size_t i = 0; i < 3; ++i) {
			const float a = f32(low->value + i * 4);
			const float b = f32(high->value + i * 4);
			if(!std::isfinite(a) || !std::isfinite(b) || a >= b) {
				error = "Editor MIN must be below MAX on every axis";
				return false;
			}
		}
	}

	word(bytes, static_cast<uint32_t>(values.size()));
	for(const auto &value : values) {
		modifier(bytes, value);
	}

	// Empty frames, script parameters, inline model, sector links, camera region and lights.
	bytes.resize(bytes.size() + 20, 0);
	return true;
}

/*
 */
bool compile(const nod_scene::Map &map, const fs::path &workspace, const fs::path &assets, std::vector<uint8_t> &bytes,
			 std::string &error) {
	if(!map.count || std::string(property(&map.entities[0], "classname")) != "worldspawn") {
		error = "Expected leading worldspawn and at most 1024 placements";
		return false;
	}

	nod::path::Graph graph;
	if(!nod::path::compile(map, graph, error)) {
		return false;
	}

	nod::lighting::Settings lighting;
	if(!nod::lighting::parse(map, lighting, error)) {
		return false;
	}

	std::vector<size_t> placements;
	for(size_t i = 1; i < map.count; ++i) {
		if(!nod::path::authoring_class(map.entities[i]) && !nod::lighting::authoring_class(map.entities[i])) {
			placements.push_back(i);
		}
	}

	if(placements.size() > NSDParser::MAX_ENTITIES) {
		error = "More than 1024 placements";
		return false;
	}

	const auto &world = map.entities[0];
	for(size_t i = 0; i < world.pair_count; ++i) {
		if(std::strcmp(world.pairs[i].key, "classname") && std::strcmp(world.pairs[i].key, "nod_light_spacing")) {
			error = "Worldspawn scene settings are unsupported";
			return false;
		}
	}

	std::vector<std::string> archives;
	const std::string context =
		placements.empty() ? "" : property(&map.entities[placements[0]], "nod_template_archives");
	if(!archive_context(context, archives, error)) {
		return false;
	}

	for(const auto i : placements) {
		if(context != property(&map.entities[i], "nod_template_archives")) {
			error = "Placements must share one ordered archive context";
			return false;
		}
	}

	NOTDatabase templates;
	if(!templates.load(workspace, archives, error, assets)) {
		return false;
	}

	for(const auto &warning : templates.warnings()) {
		std::cerr << "MAPtoNSD: " << warning << '\n';
	}

	bytes.assign(72, 0);
	put32(bytes.data(), 0x1044534e);
	put32(bytes.data() + 4, 11);
	word(bytes, 0);
	word(bytes, static_cast<uint32_t>(archives.size()));
	for(const auto &archive : archives) {
		const auto offset = bytes.size();
		bytes.resize(offset + 32, 0);
		std::copy(archive.begin(), archive.end(), bytes.begin() + offset);
	}

	word(bytes, static_cast<uint32_t>(placements.size()));
	for(const auto i : placements) {
		if(!compile_entity(map.entities[i], templates, bytes, error)) {
			error = "Entity " + std::to_string(i) + ": " + error;
			return false;
		}
	}

	// No sounds or auxiliary world arrays; empty environment and sky fields, flags zero.
	bytes.resize(bytes.size() + 192, 0);
	NSDParser check;
	if(!check.parseBuffer(bytes.data(), bytes.size()) || check.getSceneData().entity_array_count != placements.size()) {
		error = "Compiled scene failed parser cross-check";
		return false;
	}

	return true;
}

} // namespace

/*
 */
int main(int argc, char **argv) {
	std::string error;
	NodTools::CommandLine command_line;
	if(!command_line.initialize(argc, argv, error)) {
		return failure(error);
	}

	if(argc == 2 && std::string(argv[1]) == "--help") {
		std::cout << "MAPtoNSD --workspace DIR --asset-root DIR input.map output.nsd\n"
					 "Native template point placements, types 0/2/7/8, sector 0. No world geometry.\n";
		return 0;
	}

	fs::path workspace;
	fs::path assets;
	fs::path input;
	fs::path output;
	bool options = true;
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg.empty()) {
			return failure("Empty argument");
		}

		if(options && arg == "--") {
			options = false;
			continue;
		}

		if(options && (arg == "--workspace" || arg == "--asset-root")) {
			if(i + 1 == argc || !*argv[i + 1]) {
				return failure("Missing value for " + arg);
			}

			auto &path = arg == "--workspace" ? workspace : assets;
			if(!path.empty()) {
				return failure("Duplicate " + arg);
			}

			path = fs::u8path(argv[++i]);
		} else if(options && arg[0] == '-') {
			return failure("Unknown option: " + arg);
		} else if(input.empty()) {
			input = fs::u8path(arg);
		} else if(output.empty()) {
			output = fs::u8path(arg);
		} else {
			return failure("Expected one MAP and one NSD path");
		}
	}

	if(!NodTools::is_directory(workspace) || !NodTools::is_directory(assets) || input.empty() || output.empty()) {
		return failure("Specify workspace, assets and paths; use --help");
	}

	if(NodTools::lower_ascii(input.extension().u8string()) != ".map" ||
	   NodTools::lower_ascii(output.extension().u8string()) != ".nsd") {
		return failure("Expected input.map and output.nsd");
	}

	if(!NodTools::validate_output(workspace, assets, output, error) ||
	   !NodTools::distinct_input_output(input, output, error)) {
		return failure(error);
	}

	nod_scene::AuthoringMap source;
	if(!source.loadFile(input, error)) {
		return failure(error);
	}

	std::vector<uint8_t> bytes;
	if(!compile(source.getMap(), workspace, assets, bytes, error)) {
		return failure(error);
	}

	if(!nod::edit::atomic_write(output, bytes, true, error)) {
		return failure(error);
	}

	std::cout << "MAPtoNSD: wrote " << bytes.size() << " bytes, "
			  << u32(bytes.data() + 80 + u32(bytes.data() + 76) * 32) << " placements\n";
	return 0;
}
