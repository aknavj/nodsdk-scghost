/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_script.cpp
Version: 0.6.8

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

#include "nod_script.h"
#include "nod_assets.h"
#include "NOTDatabase.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <iterator>
#include <set>
#include <utility>
#include <sstream>
#include <iomanip>

/*
 */
namespace nod::script {
namespace {

/*
 */
constexpr uint32_t empty_guid = 0xfffffffe;
constexpr uint32_t max_records = 1024;

/*
 */
const char *property(const nod_scene::Entity &entity, const char *key) {
	const auto *value = nod_scene::get_property(&entity, key);
	return value ? value : "";
}

/*
 */
bool identifier(const std::string &value, bool existing = false) {
	if(value.compare(0, 4, "nsd:") == 0) {
		if(!existing) {
			return false;
		}

		uint32_t offset = 0;
		const auto text = value.substr(4);
		return nod_scene::parse_unsigned_integer(text.c_str(), UINT32_MAX, &offset) &&
			   std::to_string(offset) == text;
	}

	return !value.empty() && value.size() <= 63 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			   c == '_' || c == '-' || c == '.' || c == ':';
	});
}

struct Placement {
	struct Trigger {
		std::string target;
		float delay = 0;
	};
	std::vector<Trigger> triggers;
	size_t entity;
	std::string classname;
	std::string name;
	std::string target;
	std::string cue;
	std::string enemy;
	std::string console_template = "hackConsole_wall_activate";
	uint32_t index = 0;
	uint32_t cap = 25;
	float position[3]{};
	float rotation[3]{};
	float destination[3]{};
};

/*
 */
bool validate(const nod_scene::Entity &entity, Placement &out, std::string &error) {
	out.classname = property(entity, "classname");
	std::set<std::string> allowed{"classname", "origin", "angles", "targetname"};
	if(out.classname == "nod_console") {
		allowed.insert("target");
		allowed.insert("nod_console_template");
	} else if(out.classname == "nod_sound_cue") {
		allowed.insert("nod_cue");
	} else if(out.classname == "nod_trigger_relay") {
		allowed.insert("nod_relay_count");
		uint32_t count = 0;
		const auto *text = property(entity, "nod_relay_count");
		if(!nod_scene::parse_unsigned_integer(text, 12, &count) || !count || std::to_string(count) != text) {
			error = "nod_relay_count: expected a canonical integer in 1..12";
			return false;
		}

		for(uint32_t i = 0; i < count; ++i) {
			allowed.insert("nod_relay_target_" + std::to_string(i));
			allowed.insert("nod_relay_delay_" + std::to_string(i));
			Placement::Trigger trigger;
			trigger.target = property(entity, ("nod_relay_target_" + std::to_string(i)).c_str());
			if(!identifier(trigger.target, true) ||
			   !nod_scene::parse_float(property(entity, ("nod_relay_delay_" + std::to_string(i)).c_str()), &trigger.delay) ||
			   trigger.delay < 0) {
				error = "Relay action " + std::to_string(i) + ": expected a target and finite nonnegative delay";
				return false;
			}

			out.triggers.push_back(std::move(trigger));
		}
	} else {
		allowed.insert({"nod_enemy", "nod_destination", "nod_max_spawns"});
	}

	std::set<std::string> seen;
	for(size_t i = 0; i < entity.pair_count; ++i) {
		const std::string key = entity.pairs[i].key;
		if(!seen.insert(key).second) {
			error = "Duplicate key: " + key;
			return false;
		}

		if(!allowed.count(key)) {
			error = "Unsupported preset key: " + key;
			return false;
		}
	}

	if(entity.brush_count) {
		error = "Native control presets must be point entities";
		return false;
	}

	float angles[3]{};
	for(const auto *key : {"origin", "angles"}) {
		const auto *text = nod_scene::get_property(&entity, key);
		if(text && !nod_scene::float_vector(text, std::strcmp(key, "origin") == 0 ? out.position : angles)) {
			error = std::string(key) + ": expected three finite values";
			return false;
		}
	}

	nod_angles_from_editor(angles, out.rotation);
	out.name = property(entity, "targetname");
	if((seen.count("targetname") || out.classname != "nod_console") && !identifier(out.name)) {
		error = "targetname: expected a 1..63-byte ASCII identifier (letters, digits, _, -, ., :; nsd: is reserved)";
		return false;
	}

	if(out.classname == "nod_console") {
		const auto *selected = nod_scene::get_property(&entity, "nod_console_template");
		if(selected) {
			out.console_template = selected;
			if(!identifier(out.console_template) || out.console_template.size() > 31) {
				error = "nod_console_template: expected a native NOT template name";
				return false;
			}
		}

		out.target = property(entity, "target");
		if(!identifier(out.target, true)) {
			error = "target: expected an ASCII identifier or nsd:OFFSET";
			return false;
		}
	} else if(out.classname == "nod_sound_cue") {
		out.cue = property(entity, "nod_cue");
		if(!identifier(out.cue) || out.cue.size() > 31 || out.cue.find(':') != std::string::npos ||
		   out.cue.front() == '.' || out.cue.back() == '.') {
			error = "nod_cue: expected a 1..31-byte ASCII cue identifier (letters, digits, _, -, .; no edge dots)";
			return false;
		}
	} else if(out.classname == "nod_enemy_spawn") {
		out.enemy = property(entity, "nod_enemy");
		if(!identifier(out.enemy) || out.enemy.size() > 31) {
			error = "nod_enemy: expected a native actor template name of 1..31 bytes";
			return false;
		}

		if(!nod_scene::float_vector(property(entity, "nod_destination"), out.destination)) {
			error = "nod_destination: expected three finite world-space coordinates";
			return false;
		}

		const auto *cap = nod_scene::get_property(&entity, "nod_max_spawns");
		if(cap && (!nod_scene::parse_unsigned_integer(cap, 25, &out.cap) || out.cap == 0 ||
				   std::to_string(out.cap) != cap)) {
			error = "nod_max_spawns: expected a canonical decimal integer in 1..25";
			return false;
		}
	}

	return true;
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
void real(std::vector<uint8_t> &bytes, float value) {
	const auto offset = bytes.size();
	bytes.resize(offset + 4);
	put_float(bytes.data() + offset, value);
}

/*
 */
void modifier(std::vector<uint8_t> &bytes, uint32_t tag, const void *value, size_t size) {
	word(bytes, tag);
	const auto offset = bytes.size();
	bytes.resize(offset + 32, 0);
	std::memcpy(bytes.data() + offset, value, size);
}

/*
 */
struct Parameter {
	uint32_t type;
	uint32_t value;
	std::string text;
	Parameter(uint32_t type_, uint32_t value_) : type(type_), value(value_) {}
	explicit Parameter(const std::string &text_) : type(7), value(0), text(text_) {}
};

/*
 */
using Parameters = std::vector<Parameter>;

/*
 */
void frame(std::vector<uint8_t> &bytes, const float rotation[3], const float position[3]) {
	for(size_t i = 0; i < 3; ++i) {
		real(bytes, rotation[i]);
	}

	for(size_t i = 0; i < 3; ++i) {
		real(bytes, position[i]);
	}

	word(bytes, 0);
}

bool region_model(const nod_scene::Entity &entity, std::vector<uint8_t> &bytes, std::string &error) {
	float low[3], high[3];
	if(!nod_scene::float_vector(property(entity, "nod_region_min"), low) ||
	   !nod_scene::float_vector(property(entity, "nod_region_max"), high)) {
		error = "Trigger bounds require finite local minimum and maximum vectors";
		return false;
	}

	for(unsigned axis = 0; axis < 3; ++axis) {
		const float extent = high[axis] - low[axis];
		if(!std::isfinite(extent) || extent < 0.001f) {
			error = "Trigger maximum must exceed minimum by at least 0.001 on every axis";
			return false;
		}
	}

	const auto start = bytes.size();
	bytes.resize(start + 124, 0);
	put32(bytes.data() + start, 10);
	bytes[start + 4] = bytes[start + 6] = bytes[start + 7] = 1;
	put32(bytes.data() + start + 8, 2);
	for(unsigned axis = 0; axis < 3; ++axis) {
		put_float(bytes.data() + start + 12 + axis * 4, low[axis]);
		put_float(bytes.data() + start + 24 + axis * 4, high[axis]);
	}

	put32(bytes.data() + start + 40, 24);
	put32(bytes.data() + start + 68, 36);
	std::memcpy(bytes.data() + start + 92, "trigger", 7);
	const unsigned faces[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4},
								 {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
	for(unsigned face = 0; face < 6; ++face) {
		for(unsigned vertex : faces[face]) {
			for(unsigned axis = 0; axis < 3; ++axis) {
				real(bytes, vertex & (1u << axis) ? high[axis] : low[axis]);
			}

			for(unsigned axis = 0; axis < 3; ++axis) {
				real(bytes, axis == face / 2 ? (face % 2 ? 1.0f : -1.0f) : 0.0f);
			}

			real(bytes, 0);
			real(bytes, 0);
		}
	}

	for(unsigned face = 0; face < 6; ++face) {
		for(unsigned corner : {0u, 1u, 2u, 0u, 2u, 3u}) {
			bytes.push_back(static_cast<uint8_t>(face * 4 + corner));
			bytes.push_back(0);
		}
	}

	const auto group = bytes.size();
	bytes.resize(group + 56, 0);
	bytes[group + 6] = 36;
	bytes[group + 8] = 24;
	bytes[group + 28] = 24;
	bytes[group + 53] = 1;
	// Inline models include marker and auxiliary-vertex counts, then their convex hull.
	word(bytes, 0);
	word(bytes, 0);
	word(bytes, 6);
	for(unsigned face = 0; face < 6; ++face) {
		for(unsigned axis = 0; axis < 3; ++axis) {
			real(bytes, axis == face / 2 ? (face % 2 ? 1.0f : -1.0f) : 0.0f);
		}

		real(bytes, face % 2 ? high[face / 2] : -low[face / 2]);
	}

	return true;
}

/*
 */
std::vector<uint8_t> record(const Placement &placement, const char *base, const char *script,
						   const Parameters &parameters, bool frames = false) {
	std::vector<uint8_t> bytes;
	word(bytes, 5);
	modifier(bytes, 0, base, std::strlen(base));
	std::array<uint8_t, 12> rotation{};
	std::array<uint8_t, 12> position{};
	for(size_t i = 0; i < 3; ++i) {
		put_float(rotation.data() + i * 4, placement.rotation[i]);
		put_float(position.data() + i * 4, placement.position[i]);
	}

	modifier(bytes, 1, rotation.data(), rotation.size());
	modifier(bytes, 2, position.data(), position.size());
	const std::array<uint8_t, 4> sector{};
	modifier(bytes, 3, sector.data(), sector.size());
	modifier(bytes, 15, script, std::strlen(script));

	word(bytes, frames ? 2 : 0);
	if(frames) {
		frame(bytes, placement.rotation, placement.position);
		frame(bytes, placement.rotation, placement.destination);
	}

	word(bytes, static_cast<uint32_t>(parameters.size()));
	for(const auto &parameter : parameters) {
		word(bytes, parameter.type);
		if(parameter.type == 7) {
			word(bytes, static_cast<uint32_t>(parameter.text.size() + 1));
			bytes.insert(bytes.end(), parameter.text.begin(), parameter.text.end());
			bytes.push_back(0);
		} else {
			word(bytes, parameter.value);
		}
	}

	bytes.resize(bytes.size() + 12, 0);
	return bytes;
}

// Schemas/defaults verified against original Miners_Bunker NSDs; the published
// trigger_arena_runtime/source/controls.json carries the exact typed arrays.
Parameters action(uint32_t target) {
	Parameters values{{1, 0}, {2, 0}};
	for(uint32_t i = 2; i < 12; ++i) {
		values.emplace_back(1, i == 4 ? 1 : 0);
	}

	for(uint32_t i = 12; i < 16; ++i) {
		values.emplace_back(3, empty_guid);
	}

	values.emplace_back(3, target);
	values.emplace_back(2, 0);
	for(uint32_t i = 0; i < 3; ++i) {
		values.emplace_back(3, empty_guid);
		values.emplace_back(2, 0);
	}

	values.emplace_back(1, 0);
	return values;
}

/*
 */
Parameters sound(const std::string &cue) {
	Parameters values{{1, 1}, {2, 0}, {2, 0x41200000}, {2, 0x40a00000},
					  {2, 0x41200000}, {2, 0x3f800000}};
	values.emplace_back(cue);
	for(uint32_t i = 7; i < 16; ++i) {
		values.emplace_back(std::string());
	}

	values.emplace_back(1, 1);
	values.emplace_back(3, empty_guid);
	values.emplace_back(std::string());
	return values;
}

/*
 */
Parameters spawn(const Placement &placement) {
	return {{1, 0}, {1, 0}, Parameter(placement.enemy), {1, placement.cap}, {1, 0}, {1, 0},
			{1, 0}, {1, 1}, {2, 0x41200000}, {1, 0}, {1, 0}, {3, empty_guid}, {1, 0}};
}

/*
 */
Parameters relay(uint32_t target) {
	Parameters values{{3, target}, {2, 0}, Parameter("1 0 0 0"),
					  {3, target}, {2, 0x3e800000}, Parameter("0 0 0 0")};
	for(uint32_t i = 2; i < 12; ++i) {
		values.emplace_back(3, empty_guid);
		values.emplace_back(2, 0);
		values.emplace_back(std::string());
	}

	return values;
}

/*
 */
Parameters sequence(const Placement &placement, const std::map<std::string, uint32_t> &aliases) {
	Parameters values;
	for(size_t i = 0; i < 12; ++i) {
		values.emplace_back(3, i < placement.triggers.size() ? aliases.at(placement.triggers[i].target) : empty_guid);
		uint32_t delay = 0;
		if(i < placement.triggers.size()) {
			std::memcpy(&delay, &placement.triggers[i].delay, sizeof(delay));
		}

		values.emplace_back(2, delay);
		values.emplace_back(i < placement.triggers.size() ? std::string("0 0 0 0") : std::string());
	}

	return values;
}

} // namespace

/*
 */
bool template_class(const nod_scene::Entity &entity) {
	const std::string mode = property(entity, "nod_authoring");
	return mode == "not-template-v2" || mode == "map-template-v1";
}

/*
 */
bool body_key(const std::string &key) {
	return key == "targetname" || key.compare(0, 11, "nod_script_") == 0 ||
		   key.compare(0, 10, "nod_frame_") == 0 || key == "nod_region_min" || key == "nod_region_max";
}

/*
 */
const std::vector<std::string> &handlers() {
	static const std::vector<std::string> names{
		"cTriggerOnAction", "cSoundStinger", "cAIFightPath", "cTriggerRelay", "cButton", "cDoorMove", "cRegionTrigger"};
	return names;
}

namespace {
const NODModifier *resolved_property(const std::vector<NOTParser::ResolvedProperty> &properties, uint32_t id) {
	for(const auto &property : properties) {
		if(property.property->prop_id == id) {
			return property.property;
		}
	}

	return nullptr;
}

bool native_integer(const std::vector<NOTParser::ResolvedProperty> &properties, uint32_t id, uint32_t &value) {
	const auto *property = resolved_property(properties, id);
	if(!property) {
		return false;
	}

	const auto *bytes = property->value;
	value = uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
	return true;
}

bool native_string(const std::vector<NOTParser::ResolvedProperty> &properties, uint32_t id) {
	std::string text;
	const auto *property = resolved_property(properties, id);
	return property && nodModifierString(*property, text) && !text.empty();
}
}

bool spawnable(const std::vector<NOTParser::ResolvedProperty> &properties) {
	uint32_t type, movement;
	return native_integer(properties, 20, type) && type == 0 &&
		   native_integer(properties, 45, movement) && movement == 1 &&
		   native_string(properties, 4) && native_string(properties, 43);
}

bool actor_template(const NOTDatabase &templates, const std::string &name, std::string &error) {
	const auto *entry = templates.findTemplate(name.c_str());
	if(!entry || name != entry->name) {
		error = "Actor template not found with exact spelling in the active NOT archives: " + name;
		return false;
	}

	std::vector<NOTParser::ResolvedProperty> properties;
	if(templates.resolveModifiers(name.c_str(), properties, error) != NOTParser::ResolveStatus::ok) {
		return false;
	}

	if(!spawnable(properties)) {
		error = "Template is not a modeled native actor with actor movement and AI: " + name;
		return false;
	}

	return true;
}

bool compatible(const std::string &handler, const std::vector<NOTParser::ResolvedProperty> &properties) {
	uint32_t type, flags = 0, collision = 0;
	if(!native_integer(properties, 20, type)) {
		return false;
	}

	if(type == 3) {
		return handler == "cRegionTrigger";
	}

	if(type != 2) {
		return false;
	}

	native_integer(properties, 11, flags);
	const bool has_collision = native_integer(properties, 16, collision);
	const bool modeled = native_string(properties, 4);
	if(handler == "cTriggerOnAction" || handler == "cButton") {
		// cPlayerControl::Activate gates the native action path on THINGFLAGS bit 4.
		return modeled && (flags & 4) != 0;
	}

	if(handler == "cDoorMove") {
		return modeled && has_collision && collision != 0;
	}

	return !modeled && has_collision && collision == 0 &&
		   (handler == "cSoundStinger" || handler == "cAIFightPath" || handler == "cTriggerRelay");
}

Fields behavior(const std::string &handler) {
	if(handler == "cRegionTrigger") {
		return {{"input", "trigger"}, {"first", "1"}, {"count", "8"}, {"stride", "1"}};
	}

	if(handler == "cTriggerOnAction") {
		return {{"input", "none"}, {"first", "16"}, {"count", "4"}, {"stride", "2"}, {"delay", "1"},
				{"replace", "1"}};
	}

	if(handler == "cButton") {
		return {{"input", "none"}, {"first", "0"}, {"count", "8"}, {"stride", "1"}};
	}

	if(handler == "cTriggerRelay") {
		return {{"input", "trigger"}, {"first", "0"}, {"count", "12"}, {"stride", "3"}, {"delay", "1"},
				{"data", "2"}, {"start", "1 0 0 0"}, {"stop", "0 0 0 0"}, {"pulse-delay", "0.25"}};
	}

	if(handler == "cAIFightPath") {
		return {{"input", "pulse"}, {"frame-step", "0 2 0"}};
	}

	if(handler == "cDoorMove") {
		return {{"input", "trigger"}, {"frame-step", "0 0 2"}, {"frame-orientation", "owner"}};
	}

	if(handler == "cSoundStinger") {
		return {{"input", "trigger"}};
	}

	return {};
}

Fields defaults(const std::string &handler) {
	Parameters values;
	uint32_t frames = 0;
	if(handler == "cTriggerOnAction") {
		values = action(empty_guid);
	} else if(handler == "cSoundStinger") {
		values = sound("");
	} else if(handler == "cAIFightPath") {
		Placement placement{};
		values = spawn(placement);
		frames = 2;
	} else if(handler == "cTriggerRelay") {
		Placement placement{};
		values = sequence(placement, {});
	} else if(handler == "cButton") {
		for(unsigned i = 0; i < 8; ++i) {
			values.emplace_back(3, empty_guid);
		}

		for(unsigned i = 0; i < 4; ++i) {
			values.emplace_back(2, 0);
		}

		values.emplace_back(1, 0);
		values.emplace_back(1, 0);
		values.emplace_back(std::string("doorSwitch_01_switch"));
		values.emplace_back(1, 0);
		values.emplace_back(1, 1);
		values.emplace_back(2, 0x40a00000);
	} else if(handler == "cDoorMove") {
		values = {{1, 1}, {2, 0x40000000}, {1, UINT32_MAX}, {1, 0},
				  {3, empty_guid}, {3, empty_guid}, {3, empty_guid}, {3, empty_guid},
				  {1, 0}, {2, 0x3dcccccd}, {2, 0x3f800000}};
		frames = 2;
	} else if(handler == "cRegionTrigger") {
		values.emplace_back(1, 1);
		for(unsigned i = 0; i < 8; ++i) {
			values.emplace_back(3, empty_guid);
		}

		for(unsigned i = 0; i < 4; ++i) {
			values.emplace_back(2, 0);
		}

		values.insert(values.end(), {{1, 1}, {1, 0}, {1, 1}, {1, 0}, {2, 0}});
	} else {
		return {};
	}

	Fields fields{{"nod_script_authoring", "parameters-v1"}, {"nod_script_param_count", std::to_string(values.size())},
				  {"nod_frame_count", std::to_string(frames)}};
	for(size_t i = 0; i < values.size(); ++i) {
		const auto &value = values[i];
		std::string text;
		if(value.type == 7) {
			text = value.text;
		} else if(value.type == 3) {
			text = "-2";
		} else if(value.type == 2) {
			float number;
			std::memcpy(&number, &value.value, sizeof(number));
			std::ostringstream stream;
			stream << std::setprecision(9) << number;
			text = stream.str();
		} else {
			text = std::to_string(value.value);
		}

		fields["nod_script_kind_" + std::to_string(i)] = std::to_string(value.type);
		fields["nod_script_param_" + std::to_string(i)] = text;
	}

	for(uint32_t i = 0; i < frames; ++i) {
		fields["nod_frame_origin_" + std::to_string(i)] = "0 0 0";
		fields["nod_frame_rotation_" + std::to_string(i)] = "0 0 0";
		fields["nod_frame_sector_" + std::to_string(i)] = "0";
	}

	if(handler == "cRegionTrigger") {
		fields["nod_region_min"] = "-1 -1 0";
		fields["nod_region_max"] = "1 1 2.4";
	}

	return fields;
}

Fields field_info(const std::string &handler, const std::string &key) {
	const auto schema = defaults(handler);
	const auto found = schema.find(key);
	if(found == schema.end()) {
		return {};
	}

	Fields info{{"editable", "0"}, {"label", key}, {"storage", "raw"}};
	if(key != "nod_script_param_count" && key.compare(0, 17, "nod_script_param_") == 0) {
		const auto suffix = key.substr(17);
		const unsigned parameter = static_cast<unsigned>(std::stoul(suffix));
		const auto kind = schema.at("nod_script_kind_" + suffix);
		info["storage"] = kind == "1" ? "uint32 bits" : kind == "2" ? "float32" :
						  kind == "3" ? "object reference" : "string";
		info["editable"] = "1";
		info["label"] = "Parameter " + suffix;
		if(kind == "2") {
			info["min"] = "0";
		}

		const auto connection = behavior(handler);
		if(connection.count("first")) {
			const unsigned first = static_cast<unsigned>(std::stoul(connection.at("first")));
			const unsigned stride = static_cast<unsigned>(std::stoul(connection.at("stride")));
			const unsigned count = static_cast<unsigned>(std::stoul(connection.at("count")));
			if(parameter >= first && parameter < first + stride * count) {
				const unsigned offset = (parameter - first) % stride;
				info["label"] = std::string(offset == 0 ? "Linked target " : offset == 1 ? "Delay (seconds) " : "Trigger data ") +
								std::to_string((parameter - first) / stride + 1);
			}
		}

		if(handler == "cSoundStinger" && parameter == 6) {
			info["label"] = "Sound cue";
			info["resource"] = "cue";
		} else if(handler == "cAIFightPath" && parameter == 2) {
			info["label"] = "Actor template";
			info["resource"] = "actor";
		} else if(handler == "cAIFightPath" && parameter == 3) {
			info["label"] = "Lifetime spawn limit";
			info["min"] = "1";
			info["max"] = "25";
		} else if(handler == "cAIFightPath" && parameter >= 4 && parameter <= 7) {
			static const char *labels[] = {"Hostile to Terran", "Hostile to Zerg",
										  "Hostile to Protoss", "Hostile to player"};
			info["label"] = labels[parameter - 4];
			info["min"] = "0";
			info["max"] = "1";
		} else if(handler == "cRegionTrigger") {
			static const std::map<unsigned, std::string> labels{
				{0, "Actor filter (0 actors / 1 player / 2 player and AI)"},
				{13, "Fire on enter"}, {14, "Fire on exit"}, {15, "One-shot"},
				{16, "Initially disabled"}, {17, "Dispatch delay (seconds)"}};
			const auto label = labels.find(parameter);
			if(label != labels.end()) {
				info["label"] = label->second;
			}

			if(parameter == 0 || (parameter >= 13 && parameter <= 16)) {
				info["min"] = "0";
				info["max"] = parameter == 0 ? "2" : "1";
			}
		} else if(handler == "cDoorMove") {
			info["editable"] = parameter == 1 ? "1" : "0";
			if(parameter == 1) {
				info["label"] = "Mover speed (world units/second)";
				info["exclusive-min"] = "1";
			}
		} else if(handler == "cButton" && parameter >= 8) {
			info["editable"] = parameter == 12 || parameter == 13 || parameter == 14 ? "1" : "0";
			if(parameter == 12 || parameter == 13) {
				info["label"] = parameter == 12 ? "Button one-shot" : "Button initially locked";
				info["min"] = "0";
				info["max"] = "1";
			} else if(parameter == 14) {
				info["label"] = "Button shader reference";
			}
		}
	} else if(key == "nod_region_min" || key == "nod_region_max") {
		info["editable"] = "1";
		info["storage"] = "vector3";
		info["label"] = key == "nod_region_min" ? "Trigger local minimum" : "Trigger local maximum";
	} else if(key.compare(0, 17, "nod_frame_origin_") == 0 || key.compare(0, 19, "nod_frame_rotation_") == 0) {
		info["editable"] = "1";
		info["storage"] = "vector3";
		const bool position = key.compare(0, 17, "nod_frame_origin_") == 0;
		info["label"] = std::string(position ? "Frame position " : "Frame native rotation ") + key.substr(position ? 17 : 19);
		if(position && handler == "cAIFightPath" && key == "nod_frame_origin_1") {
			info["label"] = "Spawn path destination";
		} else if(position && handler == "cDoorMove") {
			info["label"] = key == "nod_frame_origin_0" ? "Mover stop 0 (closed / lower)" : "Mover stop 1 (open / upper)";
		}

	} else if(key.compare(0, 17, "nod_frame_sector_") == 0) {
		info["storage"] = "uint32 bits";
	}

	return info;
}

bool targets(const nod_scene::Map &map, uint32_t first_index,
			 std::map<std::string, uint32_t> &aliases, std::string &error) {
	uint32_t index = first_index;
	// Generic placements precede the optional preset records in both exporters.
	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		if(template_class(entity)) {
			const std::string name = property(entity, "targetname");
			if(!name.empty() && (!identifier(name) || !aliases.emplace(name, index).second)) {
				error = "Invalid or duplicate targetname: " + name;
				return false;
			}

			++index;
		}
	}

	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		if(authoring_class(entity)) {
			const bool spawn = std::strcmp(property(entity, "classname"), "nod_enemy_spawn") == 0;
			const std::string name = property(entity, "targetname");
			if(!name.empty() && (!identifier(name) || !aliases.emplace(name, index + (spawn ? 1 : 0)).second)) {
				error = "Invalid or duplicate targetname: " + name;
				return false;
			}

			index += spawn ? 2 : 1;
		}
	}

	if(index > max_records) {
		error = "More than 1024 final native records";
		return false;
	}

	std::map<uint32_t, const nod_scene::Entity *> objects;
	std::map<uint32_t, std::vector<uint32_t>> links;
	std::set<uint32_t> roots;
	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		const std::string name = property(entity, template_class(entity) || authoring_class(entity) ? "targetname" : "nod_id");
		const auto found = aliases.find(name);
		if(found != aliases.end()) {
			objects.emplace(found->second, &entity);
		}
	}

	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		if(!template_class(entity)) {
			continue;
		}

		const std::string handler = property(entity, "nod_prop_15");
		const auto outputs = behavior(handler);
		if(!outputs.count("first")) {
			continue;
		}

		const auto owner = aliases.find(property(entity, "targetname"));
		if(owner == aliases.end()) {
			error = "Native action/relay placements require a targetname";
			return false;
		}

		roots.insert(owner->second);

		const auto first = static_cast<unsigned>(std::stoul(outputs.at("first")));
		const auto count = static_cast<unsigned>(std::stoul(outputs.at("count")));
		const auto stride = static_cast<unsigned>(std::stoul(outputs.at("stride")));
		for(unsigned action = 0; action < count; ++action) {
			const unsigned slot = first + action * stride;
			const std::string target = property(entity, ("nod_script_param_" + std::to_string(slot)).c_str());
			if(target == "-1" || target == "-2") {
				continue;
			}

			const auto alias = aliases.find(target);
			const auto found = alias == aliases.end() ? objects.end() : objects.find(alias->second);
			if(found == objects.end()) {
				error = "Dangling native action reference: " + target;
				return false;
			}

			const auto &receiver = *found->second;
			const std::string receiver_class = property(receiver, "classname");
			const std::string receiver_handler = property(receiver, "nod_prop_15");
			const auto input = behavior(receiver_handler);
			const bool ai = input.count("input") && input.at("input") == "pulse";
			const bool accepts_trigger = input.count("input") && input.at("input") == "trigger";
			if(receiver_class != "nod_sound_cue" && receiver_class != "nod_trigger_relay" &&
			   receiver_class != "nod_enemy_spawn" && !accepts_trigger && !(outputs.count("data") && ai)) {
				error = "Native receivers must be verified cues, relays or model movers; AI paths require a relay pulse";
				return false;
			}

			if(ai) {
				const auto offset = static_cast<unsigned>(std::stoul(outputs.at("data")));
				const std::string payload = property(entity, ("nod_script_param_" + std::to_string(slot + offset)).c_str());
				if(payload != outputs.at("start") && payload != outputs.at("stop")) {
					error = "AI relay actions require a verified start/stop payload";
					return false;
				}
			}

			links[owner->second].push_back(alias->second);
		}
	}

	// Include preset edges so mixing the two authoring surfaces cannot hide cycles.
	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		if(std::strcmp(property(entity, "classname"), "nod_trigger_relay") != 0) {
			continue;
		}

		uint32_t count = 0;
		if(!nod_scene::parse_unsigned_integer(property(entity, "nod_relay_count"), 12, &count)) {
			continue; // The preset validator supplies the field diagnostic.
		}

		const auto owner = aliases.find(property(entity, "targetname"));
		if(owner == aliases.end()) {
			continue;
		}

		roots.insert(owner->second);
		for(uint32_t slot = 0; slot < count; ++slot) {
			const auto receiver = aliases.find(property(entity, ("nod_relay_target_" + std::to_string(slot)).c_str()));
			if(receiver != aliases.end()) {
				links[owner->second].push_back(receiver->second);
			}
		}
	}

	// Follow existing imported relays only when reached by newly authored actions.
	// Unchanged original graphs are not subjected to new authoring restrictions.
	for(const auto &object : objects) {
		const auto &entity = *object.second;
		if(template_class(entity) || authoring_class(entity) ||
		   std::strcmp(property(entity, "nod_prop_15"), "cTriggerRelay") != 0 ||
		   std::strcmp(property(entity, "nod_script_param_count"), "36") != 0) {
			continue;
		}

		for(unsigned slot = 0; slot < 36; slot += 3) {
			const auto found = aliases.find(property(entity, ("nod_script_param_" + std::to_string(slot)).c_str()));
			if(found != aliases.end()) {
				links[object.first].push_back(found->second);
			}
		}
	}

	std::map<uint32_t, unsigned> visiting;
	std::map<uint32_t, size_t> events;
	std::function<bool(uint32_t)> visit = [&](uint32_t object) {
		if(visiting[object] == 1) {
			error = "Native action graph contains a cycle";
			return false;
		}
		if(visiting[object] == 2) {
			return true;
		}

		visiting[object] = 1;
		for(const auto receiver : links[object]) {
			if(!visit(receiver)) {
				return false;
			}

			events[object] += 1 + events[receiver];
			// Leave half the native 128-timer pool available for spawned actors and other controls.
			if(events[object] > 64) {
				error = "Native action graph exceeds 64 transitive actions (relay limit remains 12)";
				return false;
			}
		}

		visiting[object] = 2;
		return true;
	};
	for(const auto root : roots) {
		if(!visit(root)) {
			return false;
		}
	}

	return true;
}

bool body(const nod_scene::Entity &entity, const std::map<std::string, uint32_t> &aliases,
		  std::vector<uint8_t> &bytes, std::string &error, const NOTDatabase &templates) {
	const std::string handler = property(entity, "nod_prop_15");
	const auto schema = defaults(handler);
	if(schema.empty() || property(entity, "nod_script_authoring") != schema.at("nod_script_authoring") ||
	   property(entity, "nod_script_param_count") != schema.at("nod_script_param_count") ||
	   property(entity, "nod_frame_count") != schema.at("nod_frame_count")) {
		error = "Unsupported native handler/NOT template combination or incomplete parameter/frame schema";
		return false;
	}

	for(size_t i = 0; i < entity.pair_count; ++i) {
		const std::string key = entity.pairs[i].key;
		if(body_key(key) && key != "targetname" && !schema.count(key)) {
			error = "Unexpected native body field: " + key;
			return false;
		}
	}

	std::vector<uint8_t> pending;
	const auto frames = static_cast<uint32_t>(std::stoul(schema.at("nod_frame_count")));
	word(pending, frames);
	for(uint32_t i = 0; i < frames; ++i) {
		float position[3], rotation[3];
		const auto suffix = std::to_string(i);
		if(!nod_scene::float_vector(property(entity, ("nod_frame_origin_" + suffix).c_str()), position) ||
		   !nod_scene::float_vector(property(entity, ("nod_frame_rotation_" + suffix).c_str()), rotation) ||
		   std::strcmp(property(entity, ("nod_frame_sector_" + suffix).c_str()), "0") != 0) {
			error = "Frame " + suffix + ": expected finite position/rotation and sector 0";
			return false;
		}

		frame(pending, rotation, position);
	}

	const auto count = static_cast<uint32_t>(std::stoul(schema.at("nod_script_param_count")));
	word(pending, count);
	for(uint32_t i = 0; i < count; ++i) {
		const auto suffix = std::to_string(i);
		const auto key = "nod_script_param_" + suffix;
		const auto *text = nod_scene::get_property(&entity, key.c_str());
		const auto kind = schema.at("nod_script_kind_" + suffix);
		if(!text || property(entity, ("nod_script_kind_" + suffix).c_str()) != kind) {
			error = key + ": missing value or mismatched native type";
			return false;
		}

		const std::string value = text;
		const auto info = field_info(handler, key);
		if(info.at("editable") == "0" && value != schema.at(key)) {
			error = key + ": this native setting is fixed for the verified model/button workflow";
			return false;
		}

		word(pending, static_cast<uint32_t>(std::stoul(kind)));
		uint32_t number = 0;
		if(kind == "7") {
			if(value.size() > 255 || std::any_of(value.begin(), value.end(), [](unsigned char c) {
				return c < 32 || c > 126 || c == '"' || c == '\\';
			})) {
				error = key + ": expected at most 255 MAP-safe printable ASCII bytes";
				return false;
			}

			if(info.count("resource") && info.at("resource") == "cue" &&
			   (value.empty() || value.size() > 31 || value.front() == '.' || value.back() == '.' ||
				value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.") != std::string::npos)) {
				error = key + ": expected a deployed sound cue name";
				return false;
			}
			if(info.count("resource") && info.at("resource") == "actor" && !actor_template(templates, value, error)) {
				error = key + ": " + error;
				return false;
			}

			word(pending, static_cast<uint32_t>(value.size() + 1));
			pending.insert(pending.end(), value.begin(), value.end());
			pending.push_back(0);
			continue;
		} else if(kind == "3") {
			if(value == "-2" || value == "-1") {
				number = value == "-2" ? empty_guid : UINT32_MAX;
			} else {
				const auto found = aliases.find(value);
				if(found == aliases.end()) {
					error = key + ": dangling object reference: " + value;
					return false;
				}

				number = found->second;
			}
		} else if(kind == "2") {
			float real;
			if(!nod_scene::parse_float(text, &real) ||
			   (info.count("min") && (info.count("exclusive-min") ? real <= std::stof(info.at("min")) :
											real < std::stof(info.at("min")))) ||
			   (info.count("max") && real > std::stof(info.at("max")))) {
				error = key + (handler == "cDoorMove" ? ": expected a finite positive mover speed" :
							  ": expected a finite nonnegative float");
				return false;
			}

			std::memcpy(&number, &real, sizeof(number));
		} else if(!nod_scene::parse_unsigned_integer(text, UINT32_MAX, &number) || std::to_string(number) != value ||
				  (info.count("min") && number < std::stoul(info.at("min"))) ||
				  (info.count("max") && number > std::stoul(info.at("max")))) {
			const bool boolean = info.count("min") && info.at("min") == "0" &&
								 info.count("max") && info.at("max") == "1";
			error = key + (handler == "cButton" || boolean ? ": expected 0 or 1" :
						  ": expected a canonical uint32 (spawn limit: 1..25)");
			return false;
		}

		word(pending, number);
	}

	if(handler == "cRegionTrigger") {
		word(pending, 1);
		if(!region_model(entity, pending, error)) {
			return false;
		}

		word(pending, 1);
		word(pending, 0);
		word(pending, 0);
	} else {
		pending.resize(pending.size() + 12, 0);
	}
	bytes.insert(bytes.end(), pending.begin(), pending.end());
	return true;
}

bool authoring_class(const nod_scene::Entity &entity) {
	const std::string name = property(entity, "classname");
	return name == "nod_console" || name == "nod_sound_cue" || name == "nod_enemy_spawn" || name == "nod_trigger_relay";
}

bool compile(const nod_scene::Map &map, uint32_t first_index,
			 const std::map<std::string, uint32_t> &existing_targets,
			 std::vector<std::vector<uint8_t>> &records, std::string &error, const NOTDatabase *templates) {
	if(first_index > max_records) {
		error = "More than 1024 final native records";
		return false;
	}

	auto aliases = existing_targets;
	for(const auto &target : aliases) {
		if(!identifier(target.first, true) || target.second >= first_index) {
			error = "Invalid existing target identifier or serialized index: " + target.first;
			return false;
		}
	}

	std::vector<Placement> placements;
	uint32_t next = first_index;
	for(size_t i = 0; i < map.count; ++i) {
		if(!authoring_class(map.entities[i])) {
			continue;
		}

		Placement placement{};
		placement.entity = i;
		placement.index = next;
		if(!validate(map.entities[i], placement, error)) {
			error = "Entity " + std::to_string(i) + ": " + error;
			return false;
		}

		if(templates && placement.classname == "nod_enemy_spawn" && !actor_template(*templates, placement.enemy, error)) {
			error = "Entity " + std::to_string(i) + ": " + error;
			return false;
		}

		if(templates && placement.classname == "nod_console") {
			std::vector<NOTParser::ResolvedProperty> properties;
			const auto *entry = templates->findTemplate(placement.console_template.c_str());
			if(!entry || placement.console_template != entry->name ||
			   templates->resolveModifiers(placement.console_template.c_str(), properties, error) != NOTParser::ResolveStatus::ok ||
			   !compatible("cTriggerOnAction", properties)) {
				error = "nod_console_template: expected an interactive modeled NOT prop: " + placement.console_template;
				return false;
			}
		}

		const bool spawner = placement.classname == "nod_enemy_spawn";
		next += spawner ? 2 : 1;
		if(next > max_records) {
			error = "More than 1024 final native records (including spawn relays)";
			return false;
		}

		if(!placement.name.empty() && !aliases.emplace(placement.name, placement.index + (spawner ? 1 : 0)).second) {
			error = "Entity " + std::to_string(i) + ": Duplicate targetname: " + placement.name;
			return false;
		}

		placements.push_back(std::move(placement));
	}

	for(const auto &placement : placements) {
		if(placement.classname == "nod_console" && !aliases.count(placement.target)) {
			error = "Entity " + std::to_string(placement.entity) + ": Dangling target: " + placement.target;
			return false;
		}

		for(const auto &trigger : placement.triggers) {
			if(!aliases.count(trigger.target)) {
				error = "Entity " + std::to_string(placement.entity) + ": Dangling target: " + trigger.target;
				return false;
			}

			const auto receiver = std::find_if(placements.begin(), placements.end(), [&](const Placement &value) {
				return value.name == trigger.target;
			});
			if(receiver != placements.end() && receiver->classname == "nod_console") {
				error = "Entity " + std::to_string(placement.entity) + ": A console is not a trigger receiver";
				return false;
			}
			if(receiver == placements.end()) {
				const nod_scene::Entity *imported = nullptr;
				for(size_t i = 0; i < map.count; ++i) {
					if(property(map.entities[i], "nod_id") == trigger.target ||
					   property(map.entities[i], "targetname") == trigger.target) {
						imported = &map.entities[i];
						break;
					}
				}

				const std::string script = imported ? property(*imported, "nod_prop_15") : "";
				if(script != "cSoundStinger" && script != "cTriggerRelay" && script != "cDoorMove") {
					error = "Entity " + std::to_string(placement.entity) +
							": Imported sequence receivers must be verified sound cues or pulse relays";
					return false;
				}
			}
		}
	}

	std::map<std::string, const Placement *> relays;
	for(const auto &placement : placements) {
		if(placement.classname == "nod_trigger_relay") {
			relays.emplace(placement.name, &placement);
		}
	}

	std::map<std::string, unsigned> visiting;
	std::map<std::string, size_t> events;
	std::function<bool(const std::string &)> visit = [&](const std::string &name) {
		if(visiting[name] == 1) {
			error = "Entity " + std::to_string(relays.at(name)->entity) + ": Sequence cycle: " + name;
			return false;
		}

		if(visiting[name] == 2) {
			return true;
		}

		visiting[name] = 1;
		for(const auto &trigger : relays.at(name)->triggers) {
			if(relays.count(trigger.target) && !visit(trigger.target)) {
				return false;
			}
			events[name] += 1 + (relays.count(trigger.target) ? events[trigger.target] : 0);
			if(events[name] > 12) {
				error = "Entity " + std::to_string(relays.at(name)->entity) +
						": Sequence " + name + ": more than 12 transitive trigger actions";
				return false;
			}
		}

		visiting[name] = 2;
		return true;
	};
	for(const auto &entry : relays) {
		if(!visit(entry.first)) {
			return false;
		}
	}

	std::vector<std::vector<uint8_t>> pending;
	for(const auto &placement : placements) {
		if(placement.classname == "nod_console") {
			pending.push_back(record(placement, placement.console_template.c_str(), "cTriggerOnAction",
									 action(aliases.at(placement.target))));
		} else if(placement.classname == "nod_sound_cue") {
			pending.push_back(record(placement, "script_object", "cSoundStinger", sound(placement.cue)));
		} else if(placement.classname == "nod_trigger_relay") {
			pending.push_back(record(placement, "script_target", "cTriggerRelay", sequence(placement, aliases)));
		} else {
			pending.push_back(record(placement, "script_object", "cAIFightPath", spawn(placement), true));
			pending.push_back(record(placement, "script_target", "cTriggerRelay", relay(placement.index)));
		}
	}

	records.insert(records.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
	error.clear();
	return true;
}

} // namespace nod::script
