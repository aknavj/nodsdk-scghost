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

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene_bridge.h"

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
int compile_map(const Blob *input, Blob sources[SOURCE_COUNT], const Map *bindings, GeometryMode geometry) {
	Blob baseline		= {};
	Map actual			= {};
	Map expected		= {};
	size_t change_count = 0;
	int is_successful	= 0;
	if(!scene_map(sources, &baseline, bindings, geometry) || !parse_map(&baseline, &expected) ||
	   !parse_map(input, &actual)) {
		goto cleanup;
	}

	if(actual.count != expected.count) {
		scene_error("entity addition/deletion is not supported");
		goto cleanup;
	}

	for(size_t i = 0; i < expected.count; ++i) {
		Entity *actual_entity	= &actual.entities[i];
		Entity *expected_entity = &expected.entities[i];
		if(strcmp(entity_id(actual_entity), entity_id(expected_entity)) ||
		   actual_entity->pair_count != expected_entity->pair_count) {
			scene_error("entity identity or property set changed");
			goto cleanup;
		}

		if(actual_entity->brush_count != expected_entity->brush_count) {
			scene_error("geometry edits are not supported");
			goto cleanup;
		}

		for(size_t j = 0; j < expected_entity->brush_count; ++j) {
			if(strcmp(actual_entity->brushes[j], expected_entity->brushes[j])) {
				scene_error("geometry/material/UV edits are not supported");
				goto cleanup;
			}
		}

		for(size_t j = 0; j < expected_entity->pair_count; ++j) {
			const Pair *property	 = &expected_entity->pairs[j];
			const char *actual_value = get_property(actual_entity, property->key);
			if(!actual_value) {
				scene_error("required property missing");
				goto cleanup;
			}

			if(strcmp(actual_value, property->value)) {
				if(!is_editable(expected_entity, property->key)) {
					fprintf(stderr, "NODscene: read-only property '%s' changed on '%s'\n", property->key,
							entity_id(expected_entity));
					goto cleanup;
				}

				if(!patch_property(sources, expected_entity, property->key, actual_value)) {
					goto cleanup;
				}

				++change_count;
			}
		}
	}

	printf("NODscene: validated %zu entities; applied %zu property edits; NIL geometry preserved\n", expected.count,
		   change_count);
	is_successful = 1;
cleanup:
	free(baseline.data);
	free_map(&actual);
	free_map(&expected);
	return is_successful;
}

/*
 */
int compile_scene(char **arguments) {
	Scene scene;
	if(!load_scene(&scene, arguments[2], arguments[3])) {
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
