/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodscene_compile.cpp
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

#include "niltomap.h"
#include "nod_assets.h"
#include "tool_common.h"

#include "NPDParser.h"
#include "NSDParser.h"
#include "NOTDatabase.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene_bridge.h"
#include "nod_script.h"
#include "nod_imported.h"
#include "path_graph.h"
#include <algorithm>
#include <map>
#include <set>

/*
 */
namespace nod_scene {

/*
 */
int is_editable(const Entity *entity, const char *key_name) {
	const char *classname = get_property(entity, "classname");
	const char *identity  = get_property(entity, "nod_id");
	if(identity && !strcmp(identity, "nil:world")) {
		return 0;
	}

	if(identity && !strncmp(identity, "nsd:", 4) && !strncmp(key_name, "nod_prop_", 9)) {
		uint32_t id = 0;
		return !get_property(entity, "nod_geometry") && nsd_property_id(key_name, &id) &&
			   nsd_property_editable(id, get_property(entity, "nod_reference") != nullptr);
	}

	if(!strcmp(classname, "misc_model")) {
		return !strcmp(key_name, "origin") || !strcmp(key_name, "angles") || !strcmp(key_name, "nod_sector");
	}

	if(!strncmp(classname, "nod_nsd_type_", 13)) {
		if(get_property(entity, "nod_geometry")) {
			return 0;
		}

		const bool model = get_property(entity, "model") && *get_property(entity, "model");
		return !strcmp(key_name, "origin") || !strcmp(key_name, model ? "angles" : "nod_rotation") ||
			   !strcmp(key_name, "nod_sector");
	}

	if(!strcmp(classname, "nod_nsd_entity")) {
		return !strcmp(key_name, "origin") || !strcmp(key_name, "nod_rotation") || !strcmp(key_name, "nod_sector");
	}

	if(!strcmp(classname, "nod_npd_waypoint")) {
		return !strcmp(key_name, "origin") || !strcmp(key_name, "nod_flags") || !strcmp(key_name, "nod_color") ||
			   !strcmp(key_name, "nod_extra");
	}

	if(!strcmp(classname, "nod_npd_link")) {
		return !strcmp(key_name, "nod_from") || !strcmp(key_name, "nod_to") || !strcmp(key_name, "nod_cost");
	}

	return 0;
}

/*
 */
int patch_nsd_property(Blob sources[SOURCE_COUNT], const Entity *entity, uint32_t index, const char *key_name,
					   const char *value) {
	uint32_t field_value  = 0;
	uint32_t record_count = 0;
	if(index > sources[SOURCE_NSD].size || sources[SOURCE_NSD].size - index < nsd_minimum_entity_size) {
		return scene_error("NSD patch outside source");
	}

	unsigned char *record_bytes = sources[SOURCE_NSD].data + index;
	if(!strncmp(key_name, "nod_prop_", 9)) {
		uint32_t id = 0;
		NSDParser parser;
		std::vector<NODModifier> properties;
		if(!nsd_property_id(key_name, &id) || !parser.parseBuffer(sources[SOURCE_NSD].data, sources[SOURCE_NSD].size)) {
			return scene_error("invalid native property patch");
		}

		const NODModifier *selected = nullptr;
		for(size_t i = 0; i < parser.getSceneData().entity_array_count; ++i) {
			const auto &native = parser.getSceneData().entities[i];
			if(native.block_offset != index) {
				continue;
			}

			if(!parser.getModifiers(native, properties)) {
				return scene_error("unverified native property array");
			}

			for(const auto &property : properties) {
				if(property.prop_id == id) {
					selected = &property;
				}
			}

			break;
		}

		if(!selected || !nsd_property_editable(id, get_property(entity, "nod_reference") != nullptr)) {
			return scene_error("native property is not editable");
		}

		unsigned char *payload = sources[SOURCE_NSD].data + selected->value_offset;
		const auto *info	   = nodPropertyInfo(id);
		if(info->native_type == 0) {
			const size_t length = strlen(value);
			if(length > 31) {
				return scene_error("native property strings require at most 31 printable ASCII bytes");
			}

			for(size_t i = 0; i < length; ++i) {
				if(static_cast<unsigned char>(value[i]) < 32 || static_cast<unsigned char>(value[i]) > 126) {
					return scene_error("native property strings require printable ASCII");
				}
			}

			memset(payload, 0, 32);
			memcpy(payload, value, length);
		} else if(info->native_type == 3) {
			uint32_t bits = 0;
			if(!parse_unsigned_integer(value, UINT32_MAX, &bits)) {
				return 0;
			}

			put32(payload, bits);
		} else if(info->native_type == 2) {
			float scalar = 0;
			if(!parse_float(value, &scalar)) {
				return 0;
			}

			put_float(payload, scalar);
		} else {
			float vector[3] = {};
			if(!float_vector(value, vector)) {
				return 0;
			}

			for(size_t i = 0; i < 3; ++i) {
				put_float(payload + i * 4, vector[i]);
			}
		}

		return 1;
	}

	if(!strcmp(key_name, "origin") || !strcmp(key_name, "nod_rotation") || !strcmp(key_name, "angles")) {
		float coordinates[vector_components] = {};
		const size_t offset					 = !strcmp(key_name, "origin") ? nsd_position_offset : nsd_rotation_offset;
		if(!float_vector(value, coordinates)) {
			return 0;
		}

		if(!strcmp(key_name, "angles")) {
			float expected_angles[vector_components] = {};
			float native_angles[vector_components]	 = {};
			if(!float_vector(get_property(entity, "angles"), expected_angles)) {
				return 0;
			}

			if(coordinates[0] == expected_angles[0] && coordinates[1] == expected_angles[1] &&
			   coordinates[2] == expected_angles[2]) {
				return 1;
			}

			nod_angles_from_editor(coordinates, native_angles);
			memcpy(coordinates, native_angles, sizeof(coordinates));
		}

		for(size_t i = 0; i < vector_components; ++i) {
			if(coordinates[i] != f32(record_bytes + offset + i * native_word_size)) {
				put_float(record_bytes + offset + i * native_word_size, coordinates[i]);
			}
		}

		return 1;
	}

	/* NIL envmap names precede the sector count. */
	{
		size_t offset = nil_shader_table_offset +
						static_cast<size_t>(u32(sources[SOURCE_NIL].data + nil_shader_count_offset)) * native_name_size;
		offset += native_halfword_size + static_cast<size_t>(u16(sources[SOURCE_NIL].data + offset)) * native_name_size;
		record_count = u32(sources[SOURCE_NIL].data + offset);
	}

	if(!parse_unsigned_integer(value, UINT32_MAX, &field_value) || field_value >= record_count ||
	   sources[SOURCE_NSD].size - index < nsd_sector_block_size || u32(record_bytes + nsd_sector_tag_offset) != 3) {
		return scene_error("invalid NSD sector assignment");
	}

	put32(record_bytes + nsd_sector_offset, field_value);
	return 1;
}

/*
 */
int patch_npd_waypoint(Blob *source, uint32_t index, const char *key_name, const char *value) {
	const uint32_t record_count = u16(source->data + native_halfword_size);
	uint32_t field_value		= 0;
	if(index >= record_count) {
		return scene_error("NPD waypoint index outside source");
	}

	unsigned char *record_bytes = source->data + npd_header_size + static_cast<size_t>(index) * npd_record_size;
	if(!strcmp(key_name, "origin")) {
		float coordinates[vector_components] = {};
		if(!float_vector(value, coordinates)) {
			return 0;
		}

		for(size_t i = 0; i < vector_components; ++i) {
			const size_t offset = npd_position_offset + i * native_word_size;
			if(coordinates[i] != f32(record_bytes + offset)) {
				put_float(record_bytes + offset, coordinates[i]);
			}
		}
	} else if(!strcmp(key_name, "nod_extra")) {
		Lexer lexer = {reinterpret_cast<const unsigned char *>(value), strlen(value), 0, {0}, 0};
		for(size_t i = 0; i < npd_extra_fields; ++i) {
			if(read_token(&lexer) != 1 || !parse_unsigned_integer(lexer.token, UINT16_MAX, &field_value)) {
				return 0;
			}

			put16(record_bytes + npd_extra_offset + i * native_halfword_size, static_cast<uint16_t>(field_value));
		}

		if(read_token(&lexer) != 0) {
			return scene_error("unexpected data after waypoint extra fields");
		}
	} else {
		if(!parse_unsigned_integer(value, !strcmp(key_name, "nod_color") ? UINT32_MAX : UINT16_MAX, &field_value)) {
			return 0;
		}

		if(!strcmp(key_name, "nod_color")) {
			put32(record_bytes + npd_color_offset, field_value);
		} else {
			put16(record_bytes + npd_flags_offset, static_cast<uint16_t>(field_value));
		}
	}

	return 1;
}

/*
 */
int patch_npd_link(Blob *source, uint32_t index, const char *key_name, const char *value) {
	const uint32_t record_count = u16(source->data + native_halfword_size);
	const size_t offset			= npd_header_size + static_cast<size_t>(record_count) * npd_record_size +
						  static_cast<size_t>(index) * npd_link_size;
	const size_t field =
		!strcmp(key_name, "nod_from") ? 0 : (!strcmp(key_name, "nod_to") ? native_halfword_size : native_word_size);
	uint32_t field_value = 0;
	if(offset > source->size || source->size - offset < npd_link_size ||
	   !parse_unsigned_integer(value, UINT16_MAX, &field_value) ||
	   (field != native_word_size && field_value >= record_count)) {
		return scene_error("invalid NPD link edit");
	}

	put16(source->data + offset + field, static_cast<uint16_t>(field_value));
	return 1;
}

/*
 */
int patch_property(Blob sources[SOURCE_COUNT], const Entity *entity, const char *key_name, const char *value) {
	const char *identity = entity_id(entity);
	const char *suffix	 = strchr(identity, ':');
	uint32_t index		 = 0;
	if(!suffix || !parse_unsigned_integer(suffix + 1, UINT32_MAX, &index)) {
		return scene_error("invalid patch identity");
	}

	if(!strncmp(identity, "nsd:", nsd_identity_prefix_size)) {
		return patch_nsd_property(sources, entity, index, key_name, value);
	}

	if(!strncmp(identity, "npd:", nsd_identity_prefix_size)) {
		return patch_npd_waypoint(&sources[SOURCE_NPD], index, key_name, value);
	}

	if(!strncmp(identity, "link:", link_identity_prefix_size)) {
		return patch_npd_link(&sources[SOURCE_NPD], index, key_name, value);
	}

	return scene_error("unsupported editable entity");
}

/*
 */
int compile_map(const Blob *input, Blob sources[SOURCE_COUNT], const Map *bindings, GeometryMode geometry,
				const std::filesystem::path &workspace, const std::filesystem::path &assets) {
	Blob baseline{};
	Map actual{};
	Map expected{};
	const bool successful = [&]() {
		if(!scene_map(sources, &baseline, bindings, geometry) || !parse_map(&baseline, &expected) ||
		   !parse_authoring_map(input, &actual)) {
			return false;
		}

		NSDParser parser;
		if(!parser.parseBuffer(sources[SOURCE_NSD].data, sources[SOURCE_NSD].size)) {
			return false;
		}

		const auto &scene = parser.getSceneData();
		const std::vector<uint8_t> original_npd_bytes(sources[SOURCE_NPD].data,
												 sources[SOURCE_NPD].data + sources[SOURCE_NPD].size);
		std::map<std::string, Entity *> identities;
		std::set<std::string> expected_ids;
		for(size_t i = 0; i < expected.count; ++i) {
			expected_ids.emplace(entity_id(&expected.entities[i]));
		}

		for(size_t i = 0; i < actual.count; ++i) {
			auto &entity = actual.entities[i];
			const auto *id = entity_id(&entity);
			if(id) {
				if(!expected_ids.count(id) || !identities.emplace(id, &entity).second) {
					return scene_error("Unknown or duplicate stable entity identity") != 0;
				}
			} else if(!nod::script::authoring_class(entity) && !nod::script::template_class(entity) &&
					  !nod::path::authoring_class(entity)) {
				return scene_error("Only NOT placements, native controls and NPD points may be added") != 0;
			}

			if((nod::script::authoring_class(entity) || nod::script::template_class(entity)) &&
			   scene.entity_count != scene.entity_array_count) {
				return scene_error("Appending presets requires a fully verified imported NSD object set") != 0;
			}

			if(entity.brush_count) {
				std::sort(entity.brushes, entity.brushes + entity.brush_count,
						  [](const char *a, const char *b) { return std::strcmp(a, b) < 0; });
			}
		}

		std::map<std::string, uint32_t> targets;
		std::set<uint32_t> ai_paths;
		std::set<uint32_t> action_scripts;
		for(uint32_t i = 0; i < scene.entity_array_count; ++i) {
			targets.emplace("nsd:" + std::to_string(scene.entities[i].block_offset), i);
			std::vector<NODModifier> modifiers;
			if(parser.getModifiers(scene.entities[i], modifiers)) {
				for(size_t slot = 4; slot < modifiers.size(); ++slot) {
					std::string script;
					if(modifiers[slot].prop_id == 15 && nodModifierString(modifiers[slot], script)) {
						if(script == "cAIFightPath") {
							ai_paths.insert(i);
						} else if(script == "cTriggerOnAction") {
							action_scripts.insert(i);
						}
					}
				}
			}
		}

		std::vector<std::vector<uint8_t>> records;
		std::string error;
		const auto imported_targets = targets;
		if(!nod::script::targets(actual, scene.entity_array_count, targets, error)) {
			return scene_error(error.c_str()) != 0;
		}

		NOTDatabase templates;
		bool loaded_templates = false;
		std::map<std::string, uint32_t> existing_targets = imported_targets;
		uint32_t first_preset = scene.entity_array_count;
		for(size_t i = 0; i < actual.count; ++i) {
			const auto &entity = actual.entities[i];
			if(!nod::script::template_class(entity)) {
				continue;
			}

			if(!loaded_templates) {
				if(workspace.empty() || assets.empty() ||
				   !templates.load(workspace, parser.getTemplateFiles(), error, assets)) {
					return scene_error(("New NOT placements require workspace and asset-root arguments: " + error).c_str()) != 0;
				}

				loaded_templates = true;
			}

			std::string context;
			for(const auto &archive : parser.getTemplateFiles()) {
				if(!context.empty()) {
					context += ';';
				}

				context += archive;
			}

			const auto *placement_context = get_property(&entity, "nod_template_archives");
			if(!placement_context || context != placement_context) {
				return scene_error("New NOT placements must use the imported scene's ordered archive context") != 0;
			}

			std::vector<uint8_t> record;
			if(!nod::script::placement(entity, templates, targets, record, error)) {
				return scene_error(error.c_str()) != 0;
			}

			records.push_back(std::move(record));
			const auto *name = get_property(&entity, "targetname");
			if(name && *name) {
				existing_targets.emplace(name, first_preset);
			}

			++first_preset;
		}

		if(!nod::script::compile(actual, first_preset, existing_targets, records, error)) {
			return scene_error(error.c_str()) != 0;
		}

		uint32_t index = first_preset;
		for(size_t i = 0; i < actual.count; ++i) {
			const auto &entity = actual.entities[i];
			if(nod::script::authoring_class(entity)) {
				const auto *target = get_property(&entity, "target");
				const auto receiver = target ? targets.find(target) : targets.end();
				if(receiver != targets.end() && ai_paths.count(receiver->second)) {
					return scene_error("Connect an imported cAIFightPath through a start/stop pulse relay, not directly") != 0;
				}

				const bool spawn = !std::strcmp(get_property(&entity, "classname"), "nod_enemy_spawn");
				const auto *name = get_property(&entity, "targetname");
				if(name && *name) {
					targets.emplace(name, index + (spawn ? 1 : 0));
				}

				index += spawn ? 2 : 1;
			}
		}

		size_t change_count = 0;
		for(size_t i = 0; i < expected.count; ++i) {
			const auto &before = expected.entities[i];
			const auto identity = std::string(entity_id(&before));
			const auto found = identities.find(identity);
			if(found == identities.end()) {
				if(nod::path::authoring_class(before)) {
					continue;
				}

				return scene_error("Deletion of existing NSD/geometry/metadata entities is unsupported") != 0;
			}

			const auto &after = *found->second;
			if(after.brush_count != before.brush_count) {
				return scene_error("geometry edits are not supported") != 0;
			}

			for(size_t j = 0; j < before.brush_count; ++j) {
				if(std::strcmp(after.brushes[j], before.brushes[j])) {
					return scene_error("geometry/material/UV edits are not supported") != 0;
				}
			}

			for(size_t j = 0; j < after.pair_count; ++j) {
				const auto &pair = after.pairs[j];
				const std::string key = pair.key;
				const bool environment_key = nod::imported::optional_key(key) &&
											 key.compare(0, 11, "nod_script_") != 0 &&
											 key.compare(0, 10, "nod_frame_") != 0;
				if(!get_property(&before, pair.key) && !(identity == "worldspawn" && environment_key)) {
					return scene_error(("Unexpected property: " + std::string(pair.key)).c_str()) != 0;
				}
			}

			for(size_t j = 0; j < before.pair_count; ++j) {
				const auto &pair = before.pairs[j];
				if(nod::imported::optional_key(pair.key)) {
					continue;
				}

				const auto *value = get_property(&after, pair.key);
				if(!value) {
					return scene_error(("Required property missing: " + std::string(pair.key)).c_str()) != 0;
				}

				if(std::strcmp(value, pair.value)) {
					if(!is_editable(&before, pair.key) ||
					   (!std::strcmp(pair.key, "nod_prop_15") && get_property(&before, "nod_script_authoring"))) {
						fprintf(stderr, "NODscene: read-only property '%s' changed on '%s'\n", pair.key, identity.c_str());
						return false;
					}

					// Topology is rebuilt after validation; source indices must not
					// be patched before translating them to immutable waypoint IDs.
					if(std::strcmp(get_property(&before, "classname"), "nod_npd_link") &&
					   !patch_property(sources, &before, pair.key, value)) {
						return false;
					}

					++change_count;
				}
			}
		}

		std::vector<uint8_t> nsd(sources[SOURCE_NSD].data, sources[SOURCE_NSD].data + sources[SOURCE_NSD].size);
		std::vector<nod::imported::Splice> splices;
		for(uint32_t i = 0; i < scene.entity_array_count; ++i) {
			const auto &native = scene.entities[i];
			const auto identity = "nsd:" + std::to_string(native.block_offset);
			const auto &entity = *identities.at(identity);
			nod::imported::Fields fields;
			nod::imported::script_fields(parser, native, fields);
			const auto *target = get_property(&entity, "nod_script_param_16");
			const auto receiver = target ? targets.find(target) : targets.end();
			const auto original_target = fields.find("nod_script_param_16");
			if(action_scripts.count(i) && target &&
			   original_target != fields.end() && original_target->second != target &&
			   receiver != targets.end() && ai_paths.count(receiver->second)) {
				return scene_error("Connect an imported cAIFightPath through a start/stop pulse relay, not directly") != 0;
			}

			if(!nod::imported::patch_script(parser, native, entity, fields, targets, nsd, splices, error)) {
				return scene_error((identity + ": " + error).c_str()) != 0;
			}
		}

		nod::imported::Fields environment;
		nod::imported::environment_fields(parser, environment);
		if(!nod::imported::patch_environment(parser, *identities.at("worldspawn"), environment, nsd, error)) {
			return scene_error(error.c_str()) != 0;
		}

		std::vector<uint8_t> npd;
		const Blob original_npd{const_cast<unsigned char *>(original_npd_bytes.data()), original_npd_bytes.size()};
		if(!nod::imported::rebuild_navigation(actual, expected, original_npd, npd, error)) {
			return scene_error(error.c_str()) != 0;
		}

		// If topology stayed unchanged, retain the original adjacency order and
		// apply only legacy link field edits without rebuilding opaque fields.
		if(npd.empty()) {
			for(const auto &entry : identities) {
				if(entry.first.compare(0, 5, "link:") != 0) {
					continue;
				}

				for(const auto *key : {"nod_from", "nod_to", "nod_cost"}) {
					if(!patch_property(sources, entry.second, key, get_property(entry.second, key))) {
						return false;
					}
				}
			}
		}

		if(!records.empty()) {
			size_t insertion = 0;
			if(!nod::imported::record_insertion_offset(parser, insertion)) {
				return scene_error("Appending records requires a verified NSD object/trailer boundary") != 0;
			}

			nod::imported::Splice append{insertion, 0, {}};
			for(const auto &record : records) {
				append.bytes.insert(append.bytes.end(), record.begin(), record.end());
			}

			splices.push_back(std::move(append));
			put32(nsd.data() + scene.object_data_offset - 4, scene.entity_array_count + static_cast<uint32_t>(records.size()));
		}

		std::sort(splices.begin(), splices.end(),
				  [](const nod::imported::Splice &a, const nod::imported::Splice &b) { return a.offset > b.offset; });
		for(const auto &splice : splices) {
			nsd.erase(nsd.begin() + splice.offset, nsd.begin() + splice.offset + splice.size);
			nsd.insert(nsd.begin() + splice.offset, splice.bytes.begin(), splice.bytes.end());
		}

		NSDParser check;
		if(!check.parseBuffer(nsd.data(), nsd.size()) || check.getSceneData().entity_array_count != index) {
			return scene_error("Edited NSD failed parser cross-check") != 0;
		}

		const auto publish = [](Blob &source, const std::vector<uint8_t> &bytes) {
			auto *data = static_cast<unsigned char *>(realloc(source.data, bytes.size()));
			if(!data) {
				return false;
			}

			source.data = data;
			source.size = bytes.size();
			std::memcpy(source.data, bytes.data(), bytes.size());
			return true;
		};
		if(!publish(sources[SOURCE_NSD], nsd) || (!npd.empty() && !publish(sources[SOURCE_NPD], npd))) {
			return scene_error("Out of memory publishing authored records") != 0;
		}

		printf("NODscene: validated %zu imported entities; applied %zu property edits; appended %zu native records; NIL preserved\n",
			   expected.count, change_count, records.size());
		return true;
	}();

	free(baseline.data);
	free_map(&actual);
	free_map(&expected);
	return successful ? 1 : 0;
}

/*
 */
int compile_scene(char **arguments) {
	Scene scene;
	const auto workspace = arguments[7] ? std::filesystem::u8path(arguments[7]) : std::filesystem::path{};
	const auto assets = arguments[7] ? std::filesystem::u8path(arguments[8]) : std::filesystem::path{};
	if(!load_scene(&scene, arguments[2], arguments[3], workspace, assets)) {
		return EXIT_FAILURE;
	}

	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		if(!save(arguments[4 + i], &scene.sources[i])) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

} // namespace nod_scene
