/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_imported.cpp
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

#include "nod_imported.h"
#include "path_graph.h"
#include "NPDParser.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

/*
 */
namespace nod::imported {
namespace {

/*
 */
std::string scalar(float value) {
	char text[64]{};
	std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
	return text;
}

/*
 */
std::string vector_text(const uint8_t *bytes) {
	return scalar(f32(bytes)) + " " + scalar(f32(bytes + 4)) + " " + scalar(f32(bytes + 8));
}

/*
 */
bool text(const uint8_t *bytes, size_t size, std::string &out) {
	const auto *end = static_cast<const uint8_t *>(std::memchr(bytes, 0, size));
	if(!end || size_t(end - bytes) >= nod_scene::map_value_capacity) {
		return false;
	}

	out.assign(reinterpret_cast<const char *>(bytes), size_t(end - bytes));
	return std::all_of(out.begin(), out.end(), [](unsigned char c) { return c >= 32 && c <= 126 && c != '"'; });
}

/*
 */
struct Slot {
	uint32_t kind;
	size_t offset;
	size_t size;
};

/*
 */
struct Body {
	size_t frame_offset = 0;
	uint32_t frames = 0;
	size_t end = 0;
	std::vector<Slot> parameters;
};

/*
 */
bool decode(const NSDParser &parser, const NSDParser::SceneEntity &entity, Body &body) {
	std::vector<NODModifier> modifiers;
	if(!parser.getModifiers(entity, modifiers)) {
		return false;
	}

	const auto &scene = parser.getSceneData();
	const auto *bytes = scene.raw_data;
	const auto limit = entity.block_offset + entity.block_size;
	size_t cursor = entity.block_offset - 4 + modifiers.size() * 36;
	if(cursor > limit || limit - cursor < 4) {
		return false;
	}

	body.frames = u32(bytes + cursor);
	cursor += 4;
	body.frame_offset = cursor;
	if(body.frames > (limit - cursor) / 28) {
		return false;
	}

	cursor += body.frames * 28;
	if(limit - cursor < 4) {
		return false;
	}

	const auto count = u32(bytes + cursor);
	cursor += 4;
	if(count > 128 || count > (limit - cursor) / 8) {
		return false;
	}

	for(uint32_t i = 0; i < count; ++i) {
		const auto kind = u32(bytes + cursor);
		const auto value = u32(bytes + cursor + 4);
		cursor += 4;
		size_t size = 4;
		if(kind == 7) {
			if(!value || value > limit - cursor - 4 || bytes[cursor + 4 + value - 1] != 0) {
				return false;
			}

			size += value;
		} else if(kind != 1 && kind != 2 && kind != 3) {
			return false;
		}

		body.parameters.push_back({kind, cursor, size});
		cursor += size;
		if(i + 1 < count && limit - cursor < 8) {
			return false;
		}
	}

	body.end = cursor;
	return true;
}

/*
 */
const char *property(const nod_scene::Entity &entity, const char *key) {
	const auto *value = nod_scene::get_property(&entity, key);
	return value ? value : "";
}

/*
 */
bool canonical(const char *value, uint32_t limit, uint32_t &out) {
	return nod_scene::parse_unsigned_integer(value, limit, &out) && std::to_string(out) == value;
}

/*
 */
bool safe_reference(const std::string &value, const std::string &extension) {
	if(value.empty()) {
		return true;
	}

	if(value.size() > 31 || value.front() == '.' || value.back() == '.' ||
	   !std::all_of(value.begin(), value.end(), [](unsigned char c) {
		   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
				  c == '_' || c == '-' || c == '.';
	   })) {
		return false;
	}

	std::string lower = value;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
		return c >= 'A' && c <= 'Z' ? char(c + 'a' - 'A') : char(c);
	});
	return extension.empty() || (value.size() > extension.size() &&
								lower.substr(value.size() - extension.size()) == extension);
}

/*
 */
bool changed(const nod_scene::Entity &actual, const Fields &baseline, const std::string &key) {
	const auto found = baseline.find(key);
	return found == baseline.end() ? *property(actual, key.c_str()) != 0
								  : found->second != property(actual, key.c_str());
}

/*
 */
bool environment_layout(const NSDParser &parser) {
	size_t offset = 0;
	return record_insertion_offset(parser, offset);
}

} // namespace

/*
 */
bool record_insertion_offset(const NSDParser &parser, size_t &offset) {
	const auto &scene = parser.getSceneData();
	if(scene.entity_count != scene.entity_array_count) {
		return false;
	}

	size_t cursor = scene.object_data_offset;
	if(scene.entity_array_count) {
		Body last;
		if(!decode(parser, scene.entities[scene.entity_array_count - 1], last) ||
		   last.end > scene.raw_data_size || scene.raw_data_size - last.end < 12 ||
		   !std::all_of(scene.raw_data + last.end, scene.raw_data + last.end + 12,
						[](uint8_t value) { return value == 0; })) {
			return false;
		}

		cursor = last.end + 12;
	}

	offset = cursor;
	// Original reader 0x52DD40 reads the sound array; nonempty sound records
	// have variable payloads and remain unsupported. 0x52E2C4/44B/541 read
	// the three opaque world arrays, with verified strides 60/96/96.
	for(const auto stride : {size_t(0), size_t(60), size_t(96), size_t(96)}) {
		if(cursor > scene.raw_data_size || scene.raw_data_size - cursor < 4) {
			return false;
		}

		const auto count = u32(scene.raw_data + cursor);
		cursor += 4;
		if((!stride && count) || (stride && count > (scene.raw_data_size - cursor) / stride)) {
			return false;
		}

		cursor += count * stride;
	}

	return scene.raw_data_size - cursor == 176;
}

/*
 */
bool script_fields(const NSDParser &parser, const NSDParser::SceneEntity &entity, Fields &fields) {
	if(parser.getSceneData().entity_count != parser.getSceneData().entity_array_count) {
		return false;
	}

	Body body;
	std::vector<NODModifier> modifiers;
	if(!decode(parser, entity, body) || !parser.getModifiers(entity, modifiers) || modifiers.size() < 5 ||
	   !std::any_of(modifiers.begin() + 4, modifiers.end(), [](const NODModifier &value) { return value.prop_id == 15; })) {
		return false;
	}

	const auto &scene = parser.getSceneData();
	Fields result;
	for(size_t i = 0; i < body.parameters.size(); ++i) {
		const auto &slot = body.parameters[i];
		const auto value = u32(scene.raw_data + slot.offset);
		std::string rendered;
		if(slot.kind == 7) {
			if(!text(scene.raw_data + slot.offset + 4, slot.size - 4, rendered) ||
			   rendered.size() + 1 != slot.size - 4) {
				return false;
			}
		} else if(slot.kind == 2) {
			const auto number = f32(scene.raw_data + slot.offset);
			if(!std::isfinite(number)) {
				return false;
			}

			rendered = scalar(number);
		} else if(slot.kind == 3) {
			if(value == 0xfffffffe || value == 0xffffffff) {
				rendered = value == 0xfffffffe ? "-2" : "-1";
			} else if(value < scene.entity_array_count) {
				rendered = "nsd:" + std::to_string(scene.entities[value].block_offset);
			} else {
				return false;
			}
		} else {
			rendered = std::to_string(value);
		}

		result["nod_script_param_" + std::to_string(i)] = rendered;
		result["nod_script_kind_" + std::to_string(i)] = std::to_string(slot.kind);
	}

	for(uint32_t i = 0; i < body.frames; ++i) {
		const auto *frame = scene.raw_data + body.frame_offset + i * 28;
		for(size_t axis = 0; axis < 6; ++axis) {
			if(!std::isfinite(f32(frame + axis * 4))) {
				return false;
			}
		}

		result["nod_frame_rotation_" + std::to_string(i)] = vector_text(frame);
		result["nod_frame_origin_" + std::to_string(i)] = vector_text(frame + 12);
		result["nod_frame_sector_" + std::to_string(i)] = std::to_string(u32(frame + 24));
	}

	result["nod_script_authoring"] = "parameters-v1";
	result["nod_script_param_count"] = std::to_string(body.parameters.size());
	result["nod_frame_count"] = std::to_string(body.frames);
	fields.insert(result.begin(), result.end());
	return true;
}

/*
 */
bool environment_fields(const NSDParser &parser, Fields &fields) {
	if(!environment_layout(parser)) {
		return false;
	}

	const auto &scene = parser.getSceneData();
	const auto *trailer = scene.raw_data + scene.raw_data_size - 192;
	Fields result;
	for(const auto &entry : std::map<std::string, size_t>{{"nod_music", 16}, {"nod_ambient_loop", 48},
													   {"nod_sky_model", 128}, {"nod_sky_animation", 160}}) {
		std::string value;
		if(!text(trailer + entry.second, 32, value)) {
			return false;
		}

		const std::string extension = entry.first == "nod_sky_model" ? ".nod" : entry.first == "nod_sky_animation" ? ".nad" : "";
		// Original extracted references can contain basename NUL extension NUL.
		// Expose a normal filename but retain its exact original bytes on no-edit.
		if(!extension.empty() && !value.empty() && value.size() + 5 <= 32 &&
		   std::memcmp(trailer + entry.second + value.size() + 1, extension.data() + 1, 3) == 0 &&
		   trailer[entry.second + value.size() + 4] == 0) {
			value += extension;
		}

		if(!safe_reference(value, extension)) {
			return false;
		}

		result[entry.first] = value;
	}

	const auto mode = u32(trailer + 96);
	if(mode > 1 || u32(trailer + 92) > 25 || !std::isfinite(f32(trailer + 120)) || f32(trailer + 120) < 0 ||
	   scene.object_data_offset < 12 || scene.raw_data_size < 80) {
		return false;
	}

	result["nod_fog_mode"] = mode ? "linear" : "none";
	if(mode) {
		for(size_t i = 0; i < 3; ++i) {
			if(!std::isfinite(f32(trailer + 100 + i * 4)) || f32(trailer + 100 + i * 4) < 0 ||
			   f32(trailer + 100 + i * 4) > 255) {
				return false;
			}
		}

		if(!std::isfinite(f32(trailer + 112)) || f32(trailer + 112) < 0 ||
		   !std::isfinite(f32(trailer + 116)) || f32(trailer + 116) <= f32(trailer + 112)) {
			return false;
		}

		result["nod_fog_color"] = vector_text(trailer + 100);
		result["nod_fog_start"] = scalar(f32(trailer + 112));
		result["nod_fog_end"] = scalar(f32(trailer + 116));
	}

	result["nod_far_clip"] = scalar(f32(trailer + 120));
	result["nod_reverb"] = std::to_string(u32(trailer + 92));
	result["nod_sky_unmasked"] = u32(scene.raw_data + 72) & 2 ? "1" : "0";
	result["nod_environment_authoring"] = "nsd-environment-v1";
	fields.insert(result.begin(), result.end());
	return true;
}

/*
 */
bool optional_key(const std::string &key) {
	return key.compare(0, 11, "nod_script_") == 0 || key.compare(0, 10, "nod_frame_") == 0 ||
		   key == "nod_environment_authoring" || key == "nod_music" || key == "nod_ambient_loop" ||
		   key == "nod_sky_model" || key == "nod_sky_animation" || key == "nod_sky_unmasked" ||
		   key == "nod_fog_mode" || key == "nod_fog_color" || key == "nod_fog_start" ||
		   key == "nod_fog_end" || key == "nod_far_clip" || key == "nod_reverb";
}

/*
 */
bool patch_script(const NSDParser &parser, const NSDParser::SceneEntity &entity,
				  const nod_scene::Entity &actual, const Fields &baseline,
				  const std::map<std::string, uint32_t> &targets, std::vector<uint8_t> &bytes,
				  std::vector<Splice> &splices, std::string &error) {
	const auto *marker = nod_scene::get_property(&actual, "nod_script_authoring");
	if(!marker) {
		for(size_t i = 0; i < actual.pair_count; ++i) {
			const std::string key = actual.pairs[i].key;
			if(key.compare(0, 11, "nod_script_") == 0 || key.compare(0, 10, "nod_frame_") == 0) {
				error = "Incomplete script authoring group";
				return false;
			}
		}

		return true;
	}

	Body body;
	if(std::strcmp(marker, "parameters-v1") || baseline.empty() || !decode(parser, entity, body)) {
		error = "Unsupported or unverified script authoring group";
		return false;
	}

	for(const auto &field : baseline) {
		const auto *value = nod_scene::get_property(&actual, field.first.c_str());
		if(!value || ((field.first == "nod_script_authoring" || field.first == "nod_script_param_count" ||
					  field.first == "nod_frame_count" || field.first.compare(0, 16, "nod_script_kind_") == 0) &&
					 field.second != value)) {
			error = "Missing or read-only script schema key: " + field.first;
			return false;
		}
	}

	for(size_t i = 0; i < body.parameters.size(); ++i) {
		const auto key = "nod_script_param_" + std::to_string(i);
		const std::string value = property(actual, key.c_str());
		if(!changed(actual, baseline, key)) {
			continue;
		}

		const auto &slot = body.parameters[i];
		if(slot.kind == 7) {
			if(value.size() >= nod_scene::map_value_capacity ||
			   !std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c <= 126 && c != '"'; })) {
				error = key + ": expected printable ASCII";
				return false;
			}

			Splice splice{slot.offset, slot.size, std::vector<uint8_t>(4)};
			put32(splice.bytes.data(), static_cast<uint32_t>(value.size() + 1));
			splice.bytes.insert(splice.bytes.end(), value.begin(), value.end());
			splice.bytes.push_back(0);
			splices.push_back(std::move(splice));
		} else if(slot.kind == 2) {
			float number = 0;
			if(!nod_scene::parse_float(value.c_str(), &number)) {
				error = key + ": expected a finite float";
				return false;
			}

			put_float(bytes.data() + slot.offset, number);
		} else {
			uint32_t number = 0;
			if(slot.kind == 3) {
				const auto found = targets.find(value);
				if(value == "-1" || value == "-2") {
					number = value == "-1" ? 0xffffffff : 0xfffffffe;
				} else if(found != targets.end()) {
					number = found->second;
				} else {
					error = key + ": dangling target (use a symbolic target or -1/-2)";
					return false;
				}
			} else if(!canonical(value.c_str(), UINT32_MAX, number)) {
				error = key + ": expected a canonical unsigned integer";
				return false;
			}

			put32(bytes.data() + slot.offset, number);
		}
	}

	for(uint32_t i = 0; i < body.frames; ++i) {
		for(const auto &entry : std::map<std::string, size_t>{{"nod_frame_rotation_", 0}, {"nod_frame_origin_", 12},
														   {"nod_frame_sector_", 24}}) {
			const auto key = entry.first + std::to_string(i);
			if(!changed(actual, baseline, key)) {
				continue;
			}

			const auto offset = body.frame_offset + i * 28 + entry.second;
			if(entry.second == 24) {
				uint32_t sector = 0;
				if(!canonical(property(actual, key.c_str()), 0, sector)) {
					error = key + ": only sector 0 is supported for authored frames";
					return false;
				}

				put32(bytes.data() + offset, sector);
			} else {
				float vector[3]{};
				if(!nod_scene::float_vector(property(actual, key.c_str()), vector)) {
					error = key + ": expected three finite native values";
					return false;
				}

				for(size_t axis = 0; axis < 3; ++axis) {
					put_float(bytes.data() + offset + axis * 4, vector[axis]);
				}
			}
		}
	}

	return true;
}

/*
 */
bool patch_environment(const NSDParser &parser, const nod_scene::Entity &actual, const Fields &baseline,
					   std::vector<uint8_t> &bytes, std::string &error) {
	const auto *marker = nod_scene::get_property(&actual, "nod_environment_authoring");
	if(!marker) {
		for(size_t i = 0; i < actual.pair_count; ++i) {
			if(optional_key(actual.pairs[i].key)) {
				error = "Environment fields require nod_environment_authoring";
				return false;
			}
		}

		return true;
	}

	if(std::strcmp(marker, "nsd-environment-v1") || baseline.empty() || !environment_layout(parser)) {
		error = "Unsupported or unverified environment authoring group";
		return false;
	}

	const auto trailer_offset = bytes.size() - 192;
	auto *trailer = bytes.data() + trailer_offset;
	for(const auto &entry : std::map<std::string, size_t>{{"nod_music", 16}, {"nod_ambient_loop", 48},
													   {"nod_sky_model", 128}, {"nod_sky_animation", 160}}) {
		if(!changed(actual, baseline, entry.first)) {
			continue;
		}

		const std::string value = property(actual, entry.first.c_str());
		if(!safe_reference(value, entry.first == "nod_sky_model" ? ".nod" : entry.first == "nod_sky_animation" ? ".nad" : "")) {
			error = entry.first + ": invalid native filename reference";
			return false;
		}

		std::memset(trailer + entry.second, 0, 32);
		std::memcpy(trailer + entry.second, value.data(), value.size());
	}

	for(const auto &entry : std::map<std::string, size_t>{{"nod_reverb", 92}, {"nod_sky_unmasked", 72}}) {
		if(!changed(actual, baseline, entry.first)) {
			continue;
		}

		const auto *value = nod_scene::get_property(&actual, entry.first.c_str());
		uint32_t number = 0;
		if(value && !canonical(value, entry.first == "nod_reverb" ? 25 : 1, number)) {
			error = entry.first + ": invalid canonical preset";
			return false;
		}

		if(entry.first == "nod_sky_unmasked") {
			put32(bytes.data() + 72, (u32(bytes.data() + 72) & ~2u) | (number ? 2u : 0u));
		} else {
			put32(trailer + entry.second, number);
		}
	}

	if(changed(actual, baseline, "nod_far_clip")) {
		const auto *value = nod_scene::get_property(&actual, "nod_far_clip");
		float distance = 0;
		if(value && (!nod_scene::parse_float(value, &distance) || distance < 0)) {
			error = "nod_far_clip: expected a finite nonnegative distance";
			return false;
		}

		put_float(trailer + 120, distance);
	}

	bool fog_changed = false;
	for(const auto *key : {"nod_fog_mode", "nod_fog_color", "nod_fog_start", "nod_fog_end"}) {
		fog_changed = fog_changed || changed(actual, baseline, key);
	}

	if(fog_changed) {
		const std::string mode = property(actual, "nod_fog_mode");
		if(mode.empty() || mode == "none") {
			for(const auto *key : {"nod_fog_color", "nod_fog_start", "nod_fog_end"}) {
				if(nod_scene::get_property(&actual, key)) {
					error = std::string(key) + ": requires linear fog";
					return false;
				}
			}

			std::memset(trailer + 96, 0, 24);
		} else if(mode == "linear") {
			float color[3]{};
			float start = 0;
			float end = 0;
			if(!nod_scene::float_vector(property(actual, "nod_fog_color"), color) ||
			   std::any_of(std::begin(color), std::end(color), [](float value) { return value < 0 || value > 255; }) ||
			   !nod_scene::parse_float(property(actual, "nod_fog_start"), &start) || start < 0 ||
			   !nod_scene::parse_float(property(actual, "nod_fog_end"), &end) || end <= start) {
				error = "Invalid linear fog color/start/end";
				return false;
			}

			put32(trailer + 96, 1);
			for(size_t i = 0; i < 3; ++i) {
				put_float(trailer + 100 + i * 4, color[i]);
			}

			put_float(trailer + 112, start);
			put_float(trailer + 116, end);
		} else {
			error = "nod_fog_mode: expected none or linear";
			return false;
		}
	}

	return true;
}

/*
 */
bool rebuild_navigation(const nod_scene::Map &actual, const nod_scene::Map &expected,
						const Blob &source, std::vector<uint8_t> &bytes, std::string &error) {
	NPDParser parser;
	if(!parser.parseBuffer(source.data, source.size)) {
		error = parser.getLastError();
		return false;
	}

	const auto &native = parser.getPlacementData();
	std::map<std::string, const nod_scene::Entity *> old;
	for(size_t i = 0; i < expected.count; ++i) {
		if(nod::path::authoring_class(expected.entities[i])) {
			old.emplace(nod_scene::entity_id(&expected.entities[i]), &expected.entities[i]);
		}
	}

	bool topology = false;
	size_t retained = 0;
	for(size_t i = 0; i < actual.count; ++i) {
		const auto &entity = actual.entities[i];
		if(!nod::path::authoring_class(entity)) {
			continue;
		}

		const auto *id = nod_scene::get_property(&entity, "nod_id");
		if(!id) {
			topology = true;
			continue;
		}

		const auto found = old.find(id);
		if(found == old.end()) {
			error = "Unknown source-bound NPD identity";
			return false;
		}

		++retained;
		if(std::string(property(entity, "classname")) == "nod_npd_link" &&
		   (std::strcmp(property(entity, "nod_from"), property(*found->second, "nod_from")) ||
			std::strcmp(property(entity, "nod_to"), property(*found->second, "nod_to")))) {
			topology = true;
		}
	}

	topology = topology || retained != old.size();
	if(!topology) {
		return true;
	}

	// Rebuild only normal graph topology. Associations, sectors, special flags,
	// and opaque link modes require native semantics not exposed by this editor.
	if(u16(source.data + 4) != native.link_count) {
		error = "Unsupported NPD topology: header link count disagrees with source adjacency";
		return false;
	}

	for(uint32_t i = 0; i < native.record_count; ++i) {
		const auto &node = native.records[i];
		if(node.flags || node.associationA() != 0xffff || node.associationB() != 0xffff || node.sector() != 0) {
			error = "Unsupported NPD topology: non-normal flags, associations or sectors";
			return false;
		}
	}

	for(size_t i = 0; i < native.link_count; ++i) {
		if(native.links[i].flags()) {
			error = "Unsupported NPD topology: non-normal link flags";
			return false;
		}
	}

	std::vector<std::vector<nod_scene::Pair>> pairs(1);
	std::vector<nod_scene::Entity> entities(1);
	const auto add = [&](const Fields &fields) {
		std::vector<nod_scene::Pair> values;
		for(const auto &field : fields) {
			nod_scene::Pair pair{};
			if(field.first.size() >= sizeof(pair.key) || field.second.size() >= sizeof(pair.value)) {
				return false;
			}

			std::strcpy(pair.key, field.first.c_str());
			std::strcpy(pair.value, field.second.c_str());
			values.push_back(pair);
		}

		pairs.push_back(std::move(values));
		entities.push_back({pairs.back().data(), pairs.back().size(), nullptr, 0});
		return true;
	};

	pairs[0].push_back(nod_scene::Pair{"classname", "worldspawn"});
	entities[0] = {pairs[0].data(), pairs[0].size(), nullptr, 0};
	for(size_t i = 0; i < actual.count; ++i) {
		const auto &entity = actual.entities[i];
		if(!nod::path::authoring_class(entity)) {
			continue;
		}

		const auto *id = nod_scene::get_property(&entity, "nod_id");
		if(!id) {
			Fields fields;
			for(size_t j = 0; j < entity.pair_count; ++j) {
				fields.emplace(entity.pairs[j].key, entity.pairs[j].value);
			}

			if(entity.brush_count || !add(fields)) {
				error = "Invalid added NPD point entity";
				return false;
			}

			continue;
		}

		const bool node = std::string(property(entity, "classname")) == "nod_npd_waypoint";
		Fields fields{{"classname", property(entity, "classname")}, {"nod_authoring", "npd-world-v1"},
					  {"nod_space", "world"}, {"origin", property(entity, "origin")}};
		if(node) {
			if(std::strcmp(property(entity, "nod_flags"), "0") ||
			   std::strcmp(property(entity, "nod_color"), property(*old.at(id), "nod_color")) ||
			   std::strcmp(property(entity, "nod_extra"), property(*old.at(id), "nod_extra"))) {
				error = "Unsupported NPD topology: source opaque node fields changed";
				return false;
			}

			fields["nod_waypoint_id"] = property(entity, "nod_waypoint_id");
			fields["nod_flags"] = "0";
		} else {
			for(const auto *key : {"nod_from", "nod_to"}) {
				uint32_t index = 0;
				if(!canonical(property(entity, key), native.record_count ? native.record_count - 1 : 0, index) ||
				   index >= native.record_count) {
					error = "Imported link endpoint must be an original source record index";
					return false;
				}

				fields[key] = std::to_string(native.records[index].entity_id);
			}

			if(std::strcmp(property(entity, "nod_cost"), "0")) {
				error = "Unsupported NPD topology: source link flags changed";
				return false;
			}

			fields["nod_link_flags"] = "0";
		}

		if(!add(fields)) {
			error = "Invalid source NPD point entity";
			return false;
		}
	}

	nod::path::Graph graph;
	nod_scene::Map map{entities.data(), entities.size()};
	if(!nod::path::compile(map, graph, error)) {
		return false;
	}

	put16(graph.bytes.data() + 6, std::max(u16(graph.bytes.data() + 6), u16(source.data + 6)));
	bytes = std::move(graph.bytes);
	return true;
}

} // namespace nod::imported
