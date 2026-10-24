/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: scene_bridge.cpp
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
#include "niltomap.h"
#include "nod_assets.h"
#include "tool_common.h"
#include "tool_cli.h"

#include "NPDParser.h"
#include "NSDParser.h"
#include "NOTDatabase.h"
#include "../editors/metadata_xml.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>

/*
 */
namespace nod_scene {

/*
 */
const char *get_property(const Entity *entity, const char *key_name);
int parse_map(const Blob *blob, Map *map);
void free_map(Map *map);

/*
 */
int scene_error(const char *message) {
	fprintf(stderr, "NODscene: %s\n", message);
	return 0;
}

/*
 */
int append_text(Text *text, const char *addition) {
	size_t addition_size	 = strlen(addition);
	size_t required_capacity = 0;
	char *expanded_data		 = nullptr;
	if(addition_size >= SIZE_MAX - text->size) {
		return scene_error("MAP text is too large");
	}

	required_capacity = text->size + addition_size + 1;
	if(required_capacity > text->capacity) {
		size_t new_capacity = text->capacity ? text->capacity : map_value_capacity;
		while(new_capacity < required_capacity) {
			if(new_capacity > SIZE_MAX / 2) {
				new_capacity = required_capacity;
				break;
			}

			new_capacity *= 2;
		}

		expanded_data = static_cast<char *>(realloc(text->data, new_capacity));
		if(!expanded_data) {
			return scene_error("out of memory building MAP text");
		}

		text->data	   = expanded_data;
		text->capacity = new_capacity;
	}

	memcpy(text->data + text->size, addition, addition_size + 1);
	text->size += addition_size;
	return 1;
}

/*
 */
int write_key(FILE *file, const char *key_name, const char *value) {
	const unsigned char *character = reinterpret_cast<const unsigned char *>(value);
	if(fprintf(file, "\"%s\" \"", key_name) < 0) {
		return 0;
	}
	while(*character) {
		if((*character == '"' || *character == '\\') && fputc('\\', file) == EOF) {
			return 0;
		}

		if(fputc(*character++, file) == EOF) {
			return 0;
		}
	}

	return fputs("\"\n", file) != EOF;
}

/*
 */
int write_integer_key(FILE *file, const char *key_name, uint32_t numeric_value) {
	char value[32] = {};
	snprintf(value, sizeof(value), "%u", static_cast<unsigned>(numeric_value));
	return write_key(file, key_name, value);
}

/*
 */
int begin_entity(FILE *file, const char *classname, const char *kind, size_t source_index) {
	char value[64] = {};
	snprintf(value, sizeof(value), "%s:%zu", kind, source_index);
	return fputs("{\n", file) != EOF && write_key(file, "classname", classname) && write_key(file, "nod_id", value);
}

/*
 */
int write_vector_key(FILE *file, const char *key_name, const float *vector) {
	char value[128] = {};
	snprintf(value, sizeof(value), "%.9g %.9g %.9g", static_cast<double>(vector[0]), static_cast<double>(vector[1]),
			 static_cast<double>(vector[2]));
	return write_key(file, key_name, value);
}

/*
 */
static const Entity *binding_by_id(const Map *bindings, const char *identity) {
	if(!bindings) {
		return nullptr;
	}

	for(size_t i = 0; i < bindings->count; ++i) {
		if(!strcmp(get_property(&bindings->entities[i], "nod_id"), identity)) {
			return &bindings->entities[i];
		}
	}

	return nullptr;
}

/*
 */
static const Entity *binding_for(const Map *bindings, size_t source_offset) {
	char identity[map_key_capacity] = {};
	snprintf(identity, sizeof(identity), "nsd:%zu", source_offset);
	return binding_by_id(bindings, identity);
}

/*
 */
static int write_bound_brushes(FILE *file, const Entity *binding) {
	for(size_t i = 0; i < binding->brush_count; ++i) {
		Lexer lexer = {
			reinterpret_cast<const unsigned char *>(binding->brushes[i]), strlen(binding->brushes[i]), 0, {0}, false};
		const bool primitives = !strncmp(binding->brushes[i], "brushDef\n", 9);
		if(primitives) {
			lexer.offset = 9;
		}

		if(fputs(primitives ? "{\nbrushDef\n{\n" : "{\n", file) == EOF) {
			return scene_error("cannot write fallback brush");
		}

		int status = 0;
		while((status = read_token(&lexer)) == 1) {
			for(size_t coordinate = 0; coordinate < brush_coordinate_count; ++coordinate) {
				if(coordinate && read_token(&lexer) != 1) {
					return scene_error("incomplete archived brush plane");
				}

				if((coordinate % 3 == 0 && fputs("( ", file) == EOF) || fprintf(file, "%s ", lexer.token) < 0 ||
				   (coordinate % 3 == 2 && fputs(") ", file) == EOF)) {
					return scene_error("cannot write fallback brush plane");
				}
			}

			if(primitives) {
				char shader[map_value_capacity] = {};
				if(read_token(&lexer) != 1) {
					return scene_error("incomplete archived brush material");
				}

				strcpy(shader, lexer.token);
				if(fputs("( ( ", file) == EOF) {
					return scene_error("cannot write fallback UVs");
				}

				for(size_t field = 0; field < 6; ++field) {
					if(read_token(&lexer) != 1 || fprintf(file, "%s ", lexer.token) < 0 ||
					   (field == 2 && fputs(") ( ", file) == EOF)) {
						return scene_error("cannot write fallback UVs");
					}
				}

				if(fprintf(file, ") ) %s ", shader) < 0) {
					return scene_error("cannot write fallback material");
				}
			}

			const size_t fields = primitives ? brush_flag_fields : 1 + brush_texture_fields + brush_flag_fields;
			for(size_t field = 0; field < fields; ++field) {
				if(read_token(&lexer) != 1) {
					return scene_error("incomplete archived brush material");
				}

				if(fprintf(file, "%s%c", lexer.token, field + 1 == fields ? '\n' : ' ') < 0) {
					return scene_error("cannot write fallback brush material");
				}
			}
		}

		if(status < 0 || fputs(primitives ? "}\n}\n" : "}\n", file) == EOF) {
			return scene_error("cannot write fallback brush geometry");
		}
	}

	return 1;
}

/*
 */
std::string nsd_classname(const char *type) {
	std::string name		= "nod_nsd_type_";
	static const char hex[] = "0123456789ABCDEF";
	for(const unsigned char *p = reinterpret_cast<const unsigned char *>(type); *p; ++p) {
		if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
			name += static_cast<char>(*p);
		} else {
			name += '_';
			name += hex[*p >> 4];
			name += hex[*p & 15];
		}
	}

	return name;
}

/*
 */
bool nsd_property_id(const char *key, uint32_t *id) {
	return !strncmp(key, "nod_prop_", 9) && parse_unsigned_integer(key + 9, UINT32_MAX, id) &&
		   std::string(key) == "nod_prop_" + std::to_string(*id);
}

/*
 */
bool nsd_property_editable(uint32_t id, bool preview) {
	return nodPropertyMapEditable(id, preview);
}

/*
 */
bool nsd_property_text(uint32_t id, const unsigned char *payload, std::string &text) {
	const auto *info = nodPropertyInfo(id);
	if(!info) {
		return false;
	}

	char value[128] = {};
	if(info->native_type == 0) {
		NODModifier property{};
		property.value_size = 32;
		memcpy(property.value, payload, 32);
		return nodModifierString(property, text);
	}

	if(info->native_type == 3) {
		text = std::to_string(u32(payload));
		return true;
	}

	const size_t components = info->native_type == 2 ? 1 : info->native_type == 4 ? 3 : 0;
	if(!components) {
		return false;
	}

	for(size_t i = 0; i < components; ++i) {
		if(!isfinite(f32(payload + i * 4))) {
			return false;
		}
	}

	if(components == 1) {
		snprintf(value, sizeof(value), "%.9g", static_cast<double>(f32(payload)));
	} else {
		snprintf(value, sizeof(value), "%.9g %.9g %.9g", static_cast<double>(f32(payload)),
				 static_cast<double>(f32(payload + 4)), static_cast<double>(f32(payload + 8)));
	}

	text = value;
	return true;
}

/*
 */
int nsd_entities(FILE *file, const Blob *source, size_t *entity_count, const Map *bindings, GeometryMode geometry,
				 const NOTDatabase *templates) {
	NSDParser parser;
	if(!parser.parseBuffer(source->data, source->size)) {
		return scene_error(parser.getLastError());
	}

	const auto &scene = parser.getSceneData();
	*entity_count	  = scene.entity_array_count;
	for(uint32_t i = 0; i < scene.entity_array_count; ++i) {
		const auto &native_entity  = scene.entities[i];
		const size_t source_offset = native_entity.block_offset;
		const Entity *binding	   = nullptr;
		int has_preview			   = 0;
		binding					   = binding_for(bindings, source_offset);
		if(bindings && !binding) {
			return scene_error("preview bindings do not cover the native entity set");
		}

		has_preview				 = binding && *get_property(binding, "model");
		const bool brush_preview = fallback_brushes(geometry) && binding && binding->brush_count;
		const bool typed		 = typed_entities(geometry);
		const auto *definition	 = templates ? templates->findTemplate(native_entity.type_name) : nullptr;
		if(templates && !definition) {
			return scene_error(("Cannot resolve imported template: " + std::string(native_entity.type_name)).c_str());
		}

		const std::string classname		   = templates		 ? definition->name
											 : typed		 ? nsd_classname(native_entity.type_name)
											 : brush_preview ? "func_group"
											 : has_preview	 ? "misc_model"
															 : "nod_nsd_entity";
		char geometry_id[map_key_capacity] = {};
		snprintf(geometry_id, sizeof(geometry_id), "nsdgeom:%zu", source_offset);
		if(!begin_entity(file, classname.c_str(), "nsd", source_offset) ||
		   !write_key(file, "nod_type", native_entity.type_name) ||
		   !write_vector_key(file, "origin", native_entity.position) ||
		   !write_vector_key(file, "nod_rotation", native_entity.rotation)) {
			return 0;
		}

		if(templates) {
			const auto *owner = definition ? templates->source(definition) : nullptr;
			std::vector<NODModifier> modifiers;
			std::vector<NOTParser::ResolvedProperty> effective;
			std::string error;
			std::string context;
			if(!nod::metadata::map_name(classname) || !owner || !parser.getModifiers(native_entity, modifiers) ||
			   modifiers.empty() || modifiers.size() > 128) {
				return scene_error(("Cannot import verified template/modifier identity: " + classname).c_str());
			}

			// The fourth-slot tag 15 is a region-class alias, not global property 15.
			if(nodPlacementPrefix(modifiers) == 4 && modifiers[3].prop_id == 15) {
				modifiers.erase(modifiers.begin() + 3);
			}

			if(templates->resolveModifiers(modifiers.data(), modifiers.size(), effective, error) !=
			   NOTParser::ResolveStatus::ok) {
				return scene_error(error.c_str());
			}

			const NODModifier *runtime_type = nullptr;
			for(const auto &property : effective) {
				if(property.property->prop_id == 20) {
					runtime_type = property.property;
				}
			}

			if(!runtime_type) {
				return scene_error(("Imported placement has no effective runtime type: " + classname).c_str());
			}

			for(const auto &archive : templates->snapshots()) {
				if(archive.name != "global.not") {
					if(!context.empty()) {
						context += ';';
					}

					context += archive.name;
				}
			}

			float angles[3];
			nod_angles_to_editor(native_entity.rotation, angles);
			if(!write_key(file, "nod_authoring", "not-template-v2") ||
			   !write_key(file, "nod_template", definition->name) ||
			   !write_key(file, "nod_template_archive", owner->name.c_str()) ||
			   !write_key(file, "nod_template_archives", context.c_str()) ||
			   !write_integer_key(file, "nod_template_type", u32(runtime_type->value)) ||
			   !write_vector_key(file, "angles", angles)) {
				return 0;
			}

			for(size_t slot = 0; slot < modifiers.size(); ++slot) {
				static const char hex[] = "0123456789abcdef";
				std::string text;
				for(size_t byte = 0; byte < 36; ++byte) {
					const unsigned char value = byte < 4
													? static_cast<unsigned char>(modifiers[slot].prop_id >> (byte * 8))
													: modifiers[slot].value[byte - 4];
					text += hex[value >> 4];
					text += hex[value & 15];
				}

				if(!write_key(file, ("nod_modifier_" + std::to_string(slot)).c_str(), text.c_str())) {
					return 0;
				}
			}

			if(!write_integer_key(file, "nod_modifier_count", static_cast<uint32_t>(modifiers.size()))) {
				return 0;
			}
		}

		if(typed && brush_preview && !write_key(file, "nod_geometry", geometry_id)) {
			return 0;
		}

		if(native_entity.has_sector && !write_integer_key(file, "nod_sector", native_entity.sector)) {
			return 0;
		}

		if(*native_entity.region_class && !write_key(file, "nod_region_class", native_entity.region_class)) {
			return 0;
		}

		if(entity_bounds(geometry) && *native_entity.region_class) {
			if(!write_key(file, "nod_bounds_status", native_entity.bounds_status)) {
				return 0;
			}

			if(native_entity.has_bounds) {
				if(!write_vector_key(file, "nod_bounds_min", native_entity.bounds_min) ||
				   !write_vector_key(file, "nod_bounds_max", native_entity.bounds_max)) {
					return 0;
				}
			} else {
				fprintf(stderr, "NODscene: nsd:%zu (%s): region bounds unavailable: %s\n", source_offset,
						native_entity.type_name, native_entity.bounds_status);
			}
		}

		if(native_entity.has_model && strlen(native_entity.model_name) < native_name_size &&
		   !write_key(file, "nod_model", native_entity.model_name)) {
			return 0;
		}

		if(native_properties(geometry)) {
			std::vector<NODModifier> local;
			if(parser.getModifiers(native_entity, local)) {
				std::map<uint32_t, const NODModifier *> last;
				for(const auto &property : local) {
					last[property.prop_id] = &property;
				}

				for(const auto &entry : last) {
					if(entry.first >= 1 && entry.first <= 3) {
						continue;
					}

					std::string value;
					std::string key = "nod_prop_" + std::to_string(entry.first);
					if(!nsd_property_text(entry.first, entry.second->value, value)) {
						key = "nod_raw_" + std::to_string(entry.first);
						value.clear();
						static const char hex[] = "0123456789abcdef";
						for(unsigned char byte : entry.second->value) {
							value += hex[byte >> 4];
							value += hex[byte & 15];
						}

						fprintf(stderr, "NODscene: nsd:%zu property %u retained as read-only raw payload\n",
								source_offset, entry.first);
					}

					if(!write_key(file, key.c_str(), value.c_str())) {
						return 0;
					}
				}
			} else {
				fprintf(stderr, "NODscene: nsd:%zu has no verified modifier array; property edits unavailable\n",
						source_offset);
			}
		}

		if(binding) {
			for(size_t j = 0; j < binding->pair_count; ++j) {
				const Pair *pair = &binding->pairs[j];
				if(strcmp(pair->key, "classname") && strcmp(pair->key, "nod_id") &&
				   (!brush_preview || (strcmp(pair->key, "model") && strcmp(pair->key, "_frame"))) &&
				   !write_key(file, pair->key, pair->value)) {
					return 0;
				}
			}
		}

		if(has_preview && (!typed || !brush_preview)) {
			float angles[vector_components] = {};
			char value[128]					= {};
			nod_angles_to_editor(scene.entities[i].rotation, angles);
			snprintf(value, sizeof(value), "%.9g %.9g %.9g", static_cast<double>(angles[0]),
					 static_cast<double>(angles[1]), static_cast<double>(angles[2]));
			if(!write_key(file, "angles", value)) {
				return 0;
			}
		}

		if((brush_preview && !typed && !write_bound_brushes(file, binding)) || fputs("}\n", file) == EOF) {
			return 0;
		}

		if(typed && brush_preview) {
			char parent[map_key_capacity] = {};
			snprintf(parent, sizeof(parent), "nsd:%zu", source_offset);
			if(!begin_entity(file, "func_group", "nsdgeom", source_offset) || !write_key(file, "nod_parent", parent) ||
			   !write_key(file, "nod_role", "nsd_geometry") || !write_key(file, "origin", "0 0 0") ||
			   !write_bound_brushes(file, binding) || fputs("}\n", file) == EOF) {
				return 0;
			}
		}
	}

	if(*entity_count != scene.entity_count) {
		fprintf(stderr,
				"NODscene: warning: NSD reports %u entities, %zu tagged transforms exposed; all bytes preserved\n",
				static_cast<unsigned>(scene.entity_count), *entity_count);
	}

	if(bindings && *entity_count + (binding_by_id(bindings, "nil:world") != nullptr) != bindings->count) {
		return scene_error("preview bindings contain unknown native identities");
	}

	return 1;
}

/*
 */
int npd_entities(FILE *file, const Blob *source, size_t *waypoint_count) {
	NPDParser parser;
	if(!parser.parseBuffer(source->data, source->size)) {
		return scene_error(parser.getLastError());
	}

	const auto &placement_data = parser.getPlacementData();
	*waypoint_count			   = placement_data.record_count;
	for(size_t i = 0; i < *waypoint_count; ++i) {
		const auto &record = placement_data.records[i];
		char extra[64]	   = {};
		snprintf(extra, sizeof(extra), "%u %u %u", static_cast<unsigned>(u16(record.extra)),
				 static_cast<unsigned>(u16(record.extra + native_halfword_size)),
				 static_cast<unsigned>(u16(record.extra + native_word_size)));
		if(!begin_entity(file, "nod_npd_waypoint", "npd", i) || !write_key(file, "nod_space", "navigation") ||
		   !write_vector_key(file, "origin", &record.position.x) ||
		   !write_integer_key(file, "nod_waypoint_id", record.entity_id) ||
		   !write_integer_key(file, "nod_flags", record.flags) || !write_integer_key(file, "nod_color", record.color) ||
		   !write_key(file, "nod_extra", extra) || fputs("}\n", file) == EOF) {
			return 0;
		}
	}

	for(size_t i = 0; i < placement_data.link_count; ++i) {
		const auto &record = placement_data.links[i];
		if(!begin_entity(file, "nod_npd_link", "link", i) || !write_key(file, "origin", "0 0 0") ||
		   !write_integer_key(file, "nod_from", record.from) || !write_integer_key(file, "nod_to", record.to) ||
		   !write_integer_key(file, "nod_cost", record.cost) || fputs("}\n", file) == EOF) {
			return 0;
		}
	}

	return 1;
}

/*
 */
int nil_world_entity(FILE *file, const Blob *nil_source, const Map *bindings, size_t triangles, GeometryMode geometry) {
	const Entity *binding = binding_by_id(bindings, "nil:world");
	char checksum[16]	  = {};
	char model_path[128]  = {};
	if(!bindings) {
		return 1;
	}

	if(!binding) {
		return scene_error("NIL world preview binding is missing");
	}

	snprintf(checksum, sizeof(checksum), "%08x", static_cast<unsigned>(crc32(nil_source)));
	snprintf(model_path, sizeof(model_path), "models/nodpreview/nil-%s.obj", checksum);
	if(strcmp(get_property(binding, "nod_asset_crc"), checksum) ||
	   strcmp(get_property(binding, "nod_asset"), "source:nil") ||
	   strcmp(get_property(binding, "nod_reference"), "NIL render geometry") ||
	   strcmp(get_property(binding, "nod_provenance"), "nil:render-sectors") || *get_property(binding, "nod_member")) {
		return scene_error("NIL world binding does not match its source");
	}

	if(!*get_property(binding, "model")) {
		if(triangles || strcmp(get_property(binding, "nod_preview_status"), "no-render-triangles")) {
			return scene_error("invalid empty NIL world preview status");
		}

		return 1;
	}

	{
		const char *path	   = get_property(binding, "model");
		size_t prefix		   = strlen(model_path) - 4;
		int has_diffuse_suffix = strlen(path) == prefix + 14 && !memcmp(path, model_path, prefix) &&
								 !memcmp(path + prefix, "-d", 2) && !strcmp(path + prefix + 10, ".obj");
		if(has_diffuse_suffix) {
			for(size_t k = prefix + 2; k < prefix + 10; ++k) {
				if(!isxdigit(static_cast<unsigned char>(path[k]))) {
					has_diffuse_suffix = 0;
				}
			}
		}

		if(!triangles || (strcmp(path, model_path) && !has_diffuse_suffix)) {
			return scene_error("NIL world preview path does not match its source");
		}
	}

	const bool brush_preview = fallback_brushes(geometry);
	if(brush_preview && binding->brush_count != triangles) {
		return scene_error("NIL fallback brush count does not match source triangles");
	}

	if(fputs("{\n", file) == EOF || !write_key(file, "classname", brush_preview ? "func_group" : "misc_model") ||
	   !write_key(file, "nod_id", "nil:world") || !write_key(file, "nod_role", "nil_geometry") ||
	   !write_key(file, "origin", "0 0 0") || !write_key(file, "angles", "0 0 0")) {
		return 0;
	}

	for(size_t j = 0; j < binding->pair_count; ++j) {
		const Pair *property = &binding->pairs[j];
		if(strcmp(property->key, "classname") && strcmp(property->key, "nod_id") &&
		   (!brush_preview || (strcmp(property->key, "model") && strcmp(property->key, "_frame"))) &&
		   !write_key(file, property->key, property->value)) {
			return 0;
		}
	}

	return (!brush_preview || write_bound_brushes(file, binding)) && fputs("}\n", file) != EOF;
}

/*
 */
int scene_map(const Blob sources[SOURCE_COUNT], Blob *map, const Map *bindings, GeometryMode geometry,
			  const NOTDatabase *templates) {
	FILE *map_stream						 = tmpfile();
	FILE *sector_stream						 = tmpfile();
	size_t nsd_count						 = 0;
	size_t npd_count						 = 0;
	size_t nil_triangles					 = 0;
	size_t read_count						 = 0;
	unsigned char buffer[stream_buffer_size] = {};
	char checksums[64]						 = {};
	int is_successful						 = 0;
	if(!map_stream || !sector_stream) {
		scene_error("cannot create temporary scene streams");
		goto cleanup;
	}

	if(!(geometry == GEOMETRY_LEGACY ? niltomap_write_legacy(sources[SOURCE_NIL].data, sources[SOURCE_NIL].size,
															 map_stream, 1, 1, sector_stream)
		 : geometry == GEOMETRY_BRUSHES
			 ? niltomap_write(sources[SOURCE_NIL].data, sources[SOURCE_NIL].size, map_stream, 1, 1, sector_stream)
			 : niltomap_write_metadata(sources[SOURCE_NIL].data, sources[SOURCE_NIL].size, map_stream, sector_stream,
									   &nil_triangles)) ||
	   fflush(sector_stream) || fseek(sector_stream, 0, SEEK_SET)) {
		goto cleanup;
	}
	while((read_count = fread(buffer, 1, sizeof(buffer), sector_stream)) != 0) {
		if(fwrite(buffer, 1, read_count, map_stream) != read_count) {
			goto cleanup;
		}
	}

	if(ferror(sector_stream) ||
	   (model_geometry(geometry) &&
		!nil_world_entity(map_stream, &sources[SOURCE_NIL], bindings, nil_triangles, geometry)) ||
	   !nsd_entities(map_stream, &sources[SOURCE_NSD], &nsd_count, bindings, geometry, templates) ||
	   !npd_entities(map_stream, &sources[SOURCE_NPD], &npd_count)) {
		goto cleanup;
	}

	snprintf(checksums, sizeof(checksums), "%08x %08x %08x", static_cast<unsigned>(crc32(&sources[SOURCE_NIL])),
			 static_cast<unsigned>(crc32(&sources[SOURCE_NSD])), static_cast<unsigned>(crc32(&sources[SOURCE_NPD])));
	if(fputs("{\n", map_stream) == EOF || !write_key(map_stream, "classname", "nod_scene") ||
	   !write_key(map_stream, "nod_id", "scene") || !write_key(map_stream, "origin", "0 0 0") ||
	   !write_key(map_stream, "nod_schema",
				  geometry == GEOMETRY_PROPERTIES_NATIVE		 ? "15"
				  : geometry == GEOMETRY_PROPERTIES_MODELS		 ? "14"
				  : geometry == GEOMETRY_TRANSFORMED_NATIVE		 ? "13"
				  : geometry == GEOMETRY_TRANSFORMED_MODELS		 ? "12"
				  : geometry == GEOMETRY_TYPED_NATIVE			 ? "11"
				  : geometry == GEOMETRY_TYPED_MODELS			 ? "10"
				  : geometry == GEOMETRY_NATIVE_FALLBACK_BRUSHES ? "9"
				  : geometry == GEOMETRY_MODELS_FALLBACK_BRUSHES ? "8"
				  : geometry == GEOMETRY_NATIVE_BOUNDS			 ? "7"
				  : geometry == GEOMETRY_MODELS_BOUNDS			 ? "6"
				  : geometry == GEOMETRY_NATIVE					 ? "5"
				  : geometry == GEOMETRY_MODELS					 ? "4"
				  : geometry == GEOMETRY_BRUSHES				 ? "3"
				  : bindings									 ? "2"
																 : "1") ||
	   !write_key(map_stream, "nod_sources", checksums) || fputs("}\n", map_stream) == EOF || fflush(map_stream)) {
		goto cleanup;
	}

	is_successful = read_stream(map_stream, map);
	if(is_successful) {
		printf("NODscene: exposed %zu tagged NSD entities and %zu native-space NPD waypoints\n", nsd_count, npd_count);
	}

cleanup:
	if(!is_successful) {
		scene_error("scene export failed");
	}

	if(map_stream && fclose(map_stream)) {
		scene_error("cannot close temporary MAP");
		is_successful = 0;
	}

	if(sector_stream && fclose(sector_stream)) {
		scene_error("cannot close sector stream");
		is_successful = 0;
	}

	return is_successful;
}

/*
 */
int archive_write(const Blob sources[SOURCE_COUNT], Blob *archive, const Blob *bindings, GeometryMode geometry) {
	size_t offset		= sidecar_header_size;
	size_t archive_size = sidecar_header_size + sidecar_bindings_header_size;
	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		if(sources[i].size > UINT32_MAX || sources[i].size > SIZE_MAX - archive_size) {
			return scene_error("source exceeds sidecar size limit");
		}

		archive_size += sources[i].size;
	}

	if(bindings && bindings->size) {
		if(bindings->size > UINT32_MAX || bindings->size > SIZE_MAX - archive_size) {
			return scene_error("preview bindings exceed sidecar size limit");
		}

		archive_size += bindings->size;
	}

	archive->data = static_cast<unsigned char *>(calloc(archive_size, 1));
	archive->size = archive_size;
	if(!archive->data) {
		return scene_error("out of memory creating source sidecar");
	}

	const char *magic = geometry == GEOMETRY_PROPERTIES_NATIVE		   ? "NODSRC15"
						: geometry == GEOMETRY_PROPERTIES_MODELS	   ? "NODSRC14"
						: geometry == GEOMETRY_TRANSFORMED_NATIVE	   ? "NODSRC13"
						: geometry == GEOMETRY_TRANSFORMED_MODELS	   ? "NODSRC12"
						: geometry == GEOMETRY_TYPED_NATIVE			   ? "NODSRC11"
						: geometry == GEOMETRY_TYPED_MODELS			   ? "NODSRC10"
						: geometry == GEOMETRY_NATIVE_FALLBACK_BRUSHES ? "NODSRC9\0"
						: geometry == GEOMETRY_MODELS_FALLBACK_BRUSHES ? "NODSRC8\0"
						: geometry == GEOMETRY_NATIVE_BOUNDS		   ? "NODSRC7\0"
						: geometry == GEOMETRY_MODELS_BOUNDS		   ? "NODSRC6\0"
						: native_geometry(geometry)					   ? "NODSRC5\0"
																	   : "NODSRC4\0";
	memcpy(archive->data, magic, sidecar_magic_size);
	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		put32(archive->data + sidecar_magic_size + i * sidecar_descriptor_size, static_cast<uint32_t>(sources[i].size));
		put32(archive->data + sidecar_magic_size + native_word_size + i * sidecar_descriptor_size, crc32(&sources[i]));
		memcpy(archive->data + offset, sources[i].data, sources[i].size);
		offset += sources[i].size;
	}

	if(bindings && bindings->size) {
		put32(archive->data + offset, static_cast<uint32_t>(bindings->size));
		put32(archive->data + offset + native_word_size, crc32(bindings));
		memcpy(archive->data + offset + sidecar_bindings_header_size, bindings->data, bindings->size);
	}

	return 1;
}

/*
 */
int archive_read(const Blob *archive, Blob sources[SOURCE_COUNT], Blob *bindings, GeometryMode *geometry) {
	size_t offset	 = sidecar_header_size;
	int is_version2	 = 0;
	int is_version3	 = 0;
	int is_version4	 = 0;
	int is_version5	 = 0;
	int is_version6	 = 0;
	int is_version7	 = 0;
	int is_version8	 = 0;
	int is_version9	 = 0;
	int is_version10 = 0;
	int is_version11 = 0;
	int is_version12 = 0;
	int is_version13 = 0;
	int is_version14 = 0;
	int is_version15 = 0;
	if(archive->size < sidecar_header_size || (memcmp(archive->data, "NODSRC1\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC2\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC3\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC4\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC5\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC6\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC7\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC8\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC9\0", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC10", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC11", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC12", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC13", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC14", sidecar_magic_size) &&
											   memcmp(archive->data, "NODSRC15", sidecar_magic_size))) {
		return scene_error("unsupported NOD source sidecar");
	}

	is_version2	 = !memcmp(archive->data, "NODSRC2\0", sidecar_magic_size);
	is_version3	 = !memcmp(archive->data, "NODSRC3\0", sidecar_magic_size);
	is_version4	 = !memcmp(archive->data, "NODSRC4\0", sidecar_magic_size);
	is_version5	 = !memcmp(archive->data, "NODSRC5\0", sidecar_magic_size);
	is_version6	 = !memcmp(archive->data, "NODSRC6\0", sidecar_magic_size);
	is_version7	 = !memcmp(archive->data, "NODSRC7\0", sidecar_magic_size);
	is_version8	 = !memcmp(archive->data, "NODSRC8\0", sidecar_magic_size);
	is_version9	 = !memcmp(archive->data, "NODSRC9\0", sidecar_magic_size);
	is_version10 = !memcmp(archive->data, "NODSRC10", sidecar_magic_size);
	is_version11 = !memcmp(archive->data, "NODSRC11", sidecar_magic_size);
	is_version12 = !memcmp(archive->data, "NODSRC12", sidecar_magic_size);
	is_version13 = !memcmp(archive->data, "NODSRC13", sidecar_magic_size);
	is_version14 = !memcmp(archive->data, "NODSRC14", sidecar_magic_size);
	is_version15 = !memcmp(archive->data, "NODSRC15", sidecar_magic_size);
	*geometry	 = is_version15	  ? GEOMETRY_PROPERTIES_NATIVE
				   : is_version14 ? GEOMETRY_PROPERTIES_MODELS
				   : is_version13 ? GEOMETRY_TRANSFORMED_NATIVE
				   : is_version12 ? GEOMETRY_TRANSFORMED_MODELS
				   : is_version11 ? GEOMETRY_TYPED_NATIVE
				   : is_version10 ? GEOMETRY_TYPED_MODELS
				   : is_version9  ? GEOMETRY_NATIVE_FALLBACK_BRUSHES
				   : is_version8  ? GEOMETRY_MODELS_FALLBACK_BRUSHES
				   : is_version7  ? GEOMETRY_NATIVE_BOUNDS
				   : is_version6  ? GEOMETRY_MODELS_BOUNDS
				   : is_version5  ? GEOMETRY_NATIVE
				   : is_version4  ? GEOMETRY_MODELS
				   : is_version3  ? GEOMETRY_BRUSHES
								  : GEOMETRY_LEGACY;
	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		const size_t size = u32(archive->data + sidecar_magic_size + i * sidecar_descriptor_size);
		if(size > archive->size - offset ||
		   u32(archive->data + sidecar_magic_size + 2 * native_word_size + i * sidecar_descriptor_size)) {
			return scene_error("invalid sidecar size or reserved field");
		}

		sources[i].data = static_cast<unsigned char *>(malloc(size ? size : 1));
		sources[i].size = size;
		if(!sources[i].data) {
			return scene_error("out of memory loading source sidecar");
		}

		memcpy(sources[i].data, archive->data + offset, size);
		if(crc32(&sources[i]) !=
		   u32(archive->data + sidecar_magic_size + native_word_size + i * sidecar_descriptor_size)) {
			return scene_error("source sidecar checksum mismatch");
		}

		offset += size;
	}

	if(is_version2 || is_version3 || is_version4 || is_version5 || is_version6 || is_version7 || is_version8 ||
	   is_version9 || is_version10 || is_version11 || is_version12 || is_version13 || is_version14 || is_version15) {
		size_t size = 0;
		if(archive->size - offset < sidecar_bindings_header_size) {
			return scene_error("truncated preview-binding header");
		}

		size = u32(archive->data + offset);
		if((is_version2 && !size) || size != archive->size - offset - sidecar_bindings_header_size) {
			return scene_error("invalid preview-binding size");
		}

		bindings->data = static_cast<unsigned char *>(malloc(size ? size : 1));
		bindings->size = size;
		if(!bindings->data) {
			return scene_error("out of memory reading preview bindings");
		}

		memcpy(bindings->data, archive->data + offset + sidecar_bindings_header_size, size);
		if(crc32(bindings) != u32(archive->data + offset + native_word_size)) {
			return scene_error("preview-binding checksum mismatch");
		}

		offset += sidecar_bindings_header_size + size;
	}

	return offset == archive->size ? 1 : scene_error("unexpected bytes after sidecar sources");
}

/*
 */
/* A bounded lexer accepts Radiant comments and quoted key/value pairs. */
int read_token(Lexer *lexer) {
	size_t token_size		= 0;
	unsigned char character = 0;
	lexer->is_quoted		= 0;
	for(;;) {
		while(lexer->offset < lexer->size && isspace(lexer->data[lexer->offset])) {
			++lexer->offset;
		}

		if(lexer->offset == lexer->size) {
			lexer->token[0] = '\0';
			return 0;
		}

		if(lexer->size - lexer->offset >= 2 && lexer->data[lexer->offset] == '/') {
			if(lexer->data[lexer->offset + 1] == '/') {
				while(lexer->offset < lexer->size && lexer->data[lexer->offset] != '\n') {
					++lexer->offset;
				}

				continue;
			}

			if(lexer->data[lexer->offset + 1] == '*') {
				lexer->offset += 2;
				while(lexer->size - lexer->offset >= 2 &&
					  !(lexer->data[lexer->offset] == '*' && lexer->data[lexer->offset + 1] == '/')) {
					++lexer->offset;
				}

				if(lexer->size - lexer->offset < 2) {
					scene_error("unterminated MAP comment");
					return -1;
				}

				lexer->offset += 2;
				continue;
			}
		}

		break;
	}

	character = lexer->data[lexer->offset++];
	if(character == '"') {
		lexer->is_quoted = true;
		while(lexer->offset < lexer->size && lexer->data[lexer->offset] != '"') {
			character = lexer->data[lexer->offset++];
			if(character == '\\' && lexer->offset < lexer->size &&
			   (lexer->data[lexer->offset] == '\\' || lexer->data[lexer->offset] == '"')) {
				character = lexer->data[lexer->offset++];
			}

			if(!character || token_size + 1 >= sizeof(lexer->token)) {
				scene_error("invalid or oversized MAP string");
				return -1;
			}

			lexer->token[token_size++] = static_cast<char>(character);
		}

		if(lexer->offset == lexer->size) {
			scene_error("unterminated MAP string");
			return -1;
		}

		++lexer->offset;
	} else if(character == '{' || character == '}' || character == '(' || character == ')') {
		lexer->token[token_size++] = static_cast<char>(character);
	} else {
		if(!character) {
			scene_error("embedded NUL in MAP");
			return -1;
		}

		lexer->token[token_size++] = static_cast<char>(character);
		while(lexer->offset < lexer->size && !isspace(lexer->data[lexer->offset]) &&
			  !strchr("{}()", lexer->data[lexer->offset])) {
			character = lexer->data[lexer->offset++];
			if(!character || token_size + 1 >= sizeof(lexer->token)) {
				scene_error("invalid or oversized MAP token");
				return -1;
			}

			lexer->token[token_size++] = static_cast<char>(character);
		}
	}

	lexer->token[token_size] = '\0';
	return 1;
}

/*
 */
int expect_token(Lexer *lexer, const char *expected_token) {
	return read_token(lexer) == 1 && !lexer->is_quoted && !strcmp(lexer->token, expected_token)
			   ? 1
			   : scene_error("unexpected MAP syntax");
}

/*
 */
int parse_float(const char *text, float *result) {
	char *end_pointer = nullptr;
	double value	  = 0.0;
	errno			  = 0;
	value			  = strtod(text, &end_pointer);
	if(end_pointer == text || *end_pointer || errno || !isfinite(value) || fabs(value) > FLT_MAX) {
		return scene_error("invalid MAP float");
	}

	*result = static_cast<float>(value);
	if(value != 0 && *result == 0) {
		return scene_error("MAP float underflows binary32");
	}

	return 1;
}

/*
 */
int float_vector(const char *text, float vector[vector_components]) {
	Lexer lexer = {reinterpret_cast<const unsigned char *>(text), strlen(text), 0, {0}, 0};
	for(size_t i = 0; i < vector_components; ++i) {
		if(read_token(&lexer) != 1 || !parse_float(lexer.token, &vector[i])) {
			return scene_error("expected three finite native-space floats");
		}
	}

	return read_token(&lexer) == 0 ? 1 : scene_error("unexpected data after vector");
}

/*
 */
int parse_unsigned_integer(const char *text, uint32_t maximum, uint32_t *result) {
	char *end_pointer		 = nullptr;
	unsigned long long value = 0;
	if(!*text || !isdigit(static_cast<unsigned char>(*text))) {
		return scene_error("expected unsigned decimal integer");
	}

	errno = 0;
	value = strtoull(text, &end_pointer, 10);
	if(*end_pointer || errno || value > maximum) {
		return scene_error("integer outside allowed range");
	}

	*result = static_cast<uint32_t>(value);
	return 1;
}

/*
 */
int compare_strings(const void *left, const void *right) {
	return strcmp(*static_cast<char *const *>(left), *static_cast<char *const *>(right));
}

/*
 */
int add_string(char ***list, size_t *count, char *string) {
	char **expanded_strings = nullptr;
	if(*count >= SIZE_MAX / sizeof(*expanded_strings) - 1) {
		return scene_error("too many MAP elements");
	}

	expanded_strings = static_cast<char **>(realloc(*list, (*count + 1) * sizeof(*expanded_strings)));
	if(!expanded_strings) {
		return scene_error("out of memory parsing MAP");
	}

	*list						 = expanded_strings;
	expanded_strings[(*count)++] = string;
	return 1;
}

/*
 */
int read_brush_coordinates(Lexer *lexer, float coordinates[brush_coordinate_count]) {
	if(lexer->is_quoted || strcmp(lexer->token, "(")) {
		return scene_error("unsupported MAP brush face syntax");
	}

	for(size_t j = 0; j < vector_components; ++j) {
		if(j && !expect_token(lexer, "(")) {
			return 0;
		}

		for(size_t k = 0; k < vector_components; ++k) {
			if(read_token(lexer) != 1 || !parse_float(lexer->token, &coordinates[j * vector_components + k])) {
				return 0;
			}
		}

		if(!expect_token(lexer, ")")) {
			return 0;
		}
	}

	return 1;
}

/*
 */
int read_brush_material(Lexer *lexer, char shader[map_value_capacity], float texture[6],
						uint32_t flags[brush_flag_fields], bool primitives) {
	if(primitives) {
		if(!expect_token(lexer, "(")) {
			return 0;
		}

		for(size_t row = 0; row < 2; ++row) {
			if(!expect_token(lexer, "(")) {
				return 0;
			}

			for(size_t column = 0; column < 3; ++column) {
				if(read_token(lexer) != 1 || !parse_float(lexer->token, &texture[row * 3 + column])) {
					return 0;
				}
			}

			if(!expect_token(lexer, ")")) {
				return 0;
			}
		}

		if(!expect_token(lexer, ")")) {
			return 0;
		}
	}

	if(read_token(lexer) != 1 || !*lexer->token) {
		return scene_error("missing brush material");
	}

	strcpy(shader, lexer->token);
	for(size_t j = 0; !primitives && j < brush_texture_fields; ++j) {
		if(read_token(lexer) != 1 || !parse_float(lexer->token, &texture[j])) {
			return 0;
		}
	}

	const size_t saved_offset = lexer->offset;
	if(read_token(lexer) != 1) {
		return scene_error("unterminated brush");
	}

	if(!strcmp(lexer->token, "(") || !strcmp(lexer->token, "}")) {
		lexer->offset = saved_offset;
		return 1;
	}

	if(!parse_unsigned_integer(lexer->token, UINT32_MAX, &flags[0])) {
		return 0;
	}

	for(size_t j = 1; j < brush_flag_fields; ++j) {
		if(read_token(lexer) != 1 || !parse_unsigned_integer(lexer->token, UINT32_MAX, &flags[j])) {
			return 0;
		}
	}

	return 1;
}

/*
 */
int canonical_face(Text *face, const float coordinates[brush_coordinate_count], const char *shader,
				   const float texture[6], const uint32_t flags[brush_flag_fields], bool primitives) {
	size_t rotation_index = 0;
	// BP matrices depend on the binary32 normal: cyclic permutations can change its rounding.
	for(size_t j = 1; !primitives && j < vector_components; ++j) {
		for(size_t k = 0; k < brush_coordinate_count; ++k) {
			const float left_coordinate = coordinates[(j * vector_components + k) % brush_coordinate_count];
			const float right_coordinate =
				coordinates[(rotation_index * vector_components + k) % brush_coordinate_count];
			if(left_coordinate < right_coordinate) {
				rotation_index = j;
				break;
			}

			if(left_coordinate > right_coordinate) {
				break;
			}
		}
	}

	char field[map_key_capacity] = {};
	for(size_t j = 0; j < brush_coordinate_count; ++j) {
		const float value = coordinates[(rotation_index * vector_components + j) % brush_coordinate_count];
		snprintf(field, sizeof(field), "%.9g ", value == 0 ? 0.0 : static_cast<double>(value));
		if(!append_text(face, field)) {
			return 0;
		}
	}

	if(!append_text(face, shader) || !append_text(face, " ")) {
		return 0;
	}

	for(size_t j = 0; j < (primitives ? 6 : brush_texture_fields); ++j) {
		snprintf(field, sizeof(field), "%.9g ", texture[j] == 0 ? 0.0 : static_cast<double>(texture[j]));
		if(!append_text(face, field)) {
			return 0;
		}
	}

	snprintf(field, sizeof(field), "%u %u %u", static_cast<unsigned>(flags[0]), static_cast<unsigned>(flags[1]),
			 static_cast<unsigned>(flags[2]));
	return append_text(face, field);
}

/*
 */
int canonical_brush(Lexer *lexer, char **output) {
	char **faces		  = nullptr;
	size_t face_count	  = 0;
	Text brush			  = {};
	int is_successful	  = 0;
	const size_t start	  = lexer->offset;
	const bool primitives = read_token(lexer) == 1 && !strcmp(lexer->token, "brushDef");
	if(primitives) {
		if(!expect_token(lexer, "{") || !append_text(&brush, "brushDef\n")) {
			goto cleanup;
		}
	} else {
		lexer->offset = start;
	}
	while(read_token(lexer) == 1) {
		float coordinates[brush_coordinate_count] = {};
		float texture[6]						  = {};
		uint32_t flags[brush_flag_fields]		  = {};
		char shader[map_value_capacity]			  = {};
		Text face								  = {};
		if(!lexer->is_quoted && !strcmp(lexer->token, "}")) {
			if(!face_count) {
				scene_error("empty brush");
				goto cleanup;
			}

			if(primitives && !expect_token(lexer, "}")) {
				goto cleanup;
			}

			qsort(faces, face_count, sizeof(*faces), compare_strings);
			for(size_t i = 0; i < face_count; ++i) {
				if(!append_text(&brush, faces[i]) || !append_text(&brush, "\n")) {
					goto cleanup;
				}
			}

			*output		  = brush.data;
			brush.data	  = nullptr;
			is_successful = 1;
			goto cleanup;
		}

		if(!read_brush_coordinates(lexer, coordinates) ||
		   !read_brush_material(lexer, shader, texture, flags, primitives)) {
			goto cleanup;
		}

		if(!canonical_face(&face, coordinates, shader, texture, flags, primitives) ||
		   !add_string(&faces, &face_count, face.data)) {
			goto face_failed;
		}

		continue;
	face_failed:
		free(face.data);
		goto cleanup;
	}

	scene_error("unterminated brush");
cleanup:
	for(size_t i = 0; i < face_count; ++i) {
		free(faces[i]);
	}

	free(faces);
	free(brush.data);
	return is_successful;
}

const char *get_property(const Entity *entity, const char *key_name) {
	for(size_t i = 0; i < entity->pair_count; ++i) {
		if(!strcmp(entity->pairs[i].key, key_name)) {
			return entity->pairs[i].value;
		}
	}

	return nullptr;
}

/*
 */
void free_map(Map *map) {
	for(size_t i = 0; i < map->count; ++i) {
		Entity *entity = &map->entities[i];
		for(size_t j = 0; j < entity->brush_count; ++j) {
			free(entity->brushes[j]);
		}

		free(entity->brushes);
		free(entity->pairs);
	}

	free(map->entities);
}

/*
 */
const char *entity_id(const Entity *entity) {
	const char *classname = get_property(entity, "classname");
	return classname && !strcmp(classname, "worldspawn") ? "worldspawn" : get_property(entity, "nod_id");
}

/*
 */
int compare_entities(const void *left, const void *right) {
	return strcmp(entity_id(static_cast<const Entity *>(left)), entity_id(static_cast<const Entity *>(right)));
}

/*
 */
static int parse_map_impl(const Blob *blob, Map *map, bool authoring) {
	Lexer lexer = {blob->data, blob->size, 0, {0}, 0};
	int status	= 0;
	while((status = read_token(&lexer)) == 1) {
		Entity *expanded_entities = nullptr;
		Entity *entity			  = nullptr;
		if(lexer.is_quoted || strcmp(lexer.token, "{")) {
			return scene_error("expected MAP entity opening brace");
		}

		if(map->count >= SIZE_MAX / sizeof(*expanded_entities) - 1) {
			return scene_error("too many entities");
		}

		expanded_entities =
			static_cast<Entity *>(realloc(map->entities, (map->count + 1) * sizeof(*expanded_entities)));
		if(!expanded_entities) {
			return scene_error("out of memory parsing entities");
		}

		map->entities = expanded_entities;
		entity		  = &expanded_entities[map->count++];
		memset(entity, 0, sizeof(*entity));
		for(;;) {
			Pair pair	= {};
			Pair *pairs = nullptr;
			if(read_token(&lexer) != 1) {
				return scene_error("unterminated MAP entity");
			}

			if(!lexer.is_quoted && !strcmp(lexer.token, "}")) {
				break;
			}

			if(!lexer.is_quoted && !strcmp(lexer.token, "{")) {
				char *brush = nullptr;
				if(!canonical_brush(&lexer, &brush)) {
					return 0;
				}

				if(!add_string(&entity->brushes, &entity->brush_count, brush)) {
					free(brush);
					return 0;
				}

				continue;
			}

			if(!lexer.is_quoted || strlen(lexer.token) >= sizeof(pair.key) || get_property(entity, lexer.token)) {
				return scene_error("invalid or duplicate entity key");
			}

			strcpy(pair.key, lexer.token);
			if(read_token(&lexer) != 1 || !lexer.is_quoted) {
				return scene_error("expected quoted entity value");
			}

			strcpy(pair.value, lexer.token);
			if(entity->pair_count >= SIZE_MAX / sizeof(*pairs) - 1) {
				return scene_error("too many entity properties");
			}

			pairs = static_cast<Pair *>(realloc(entity->pairs, (entity->pair_count + 1) * sizeof(*pairs)));
			if(!pairs) {
				return scene_error("out of memory parsing properties");
			}

			entity->pairs				= pairs;
			pairs[entity->pair_count++] = pair;
		}

		if((!authoring && !entity_id(entity)) || !get_property(entity, "classname")) {
			return scene_error(authoring ? "entity lacks classname" : "entity lacks classname or stable nod_id");
		}

		if(!authoring && entity->brush_count) {
			qsort(entity->brushes, entity->brush_count, sizeof(*entity->brushes), compare_strings);
		}
	}

	if(status < 0) {
		return 0;
	}

	if(!map->count) {
		return scene_error("empty MAP");
	}

	if(!authoring) {
		qsort(map->entities, map->count, sizeof(*map->entities), compare_entities);
	}

	for(size_t i = 1; !authoring && i < map->count; ++i) {
		if(!strcmp(entity_id(&map->entities[i - 1]), entity_id(&map->entities[i]))) {
			return scene_error("duplicate stable entity identity");
		}
	}

	return 1;
}

/*
 */
int parse_map(const Blob *blob, Map *map) {
	return parse_map_impl(blob, map, false);
}

/*
 */
int parse_authoring_map(const Blob *blob, Map *map) {
	return parse_map_impl(blob, map, true);
}

/*
 */
AuthoringMap::~AuthoringMap() {
	free_map(&map_data);
}

/*
 */
bool AuthoringMap::loadFile(const std::filesystem::path &path, std::string &error) {
	constexpr size_t maximum_map_bytes = 64u * 1024u * 1024u;
	std::vector<uint8_t> text;
	if(!NodTools::read_file(path, text, error, maximum_map_bytes)) {
		return false;
	}

	Blob blob{text.data(), text.size()};
	Map parsed{};
	if(!parse_authoring_map(&blob, &parsed)) {
		free_map(&parsed);
		error = "Invalid MAP";
		return false;
	}

	free_map(&map_data);
	map_data = parsed;
	return true;
}

/*
 */
int validate_bindings(const Map *bindings, GeometryMode geometry) {
	static const char *fields[] = {"classname",		 "nod_id",		  "nod_reference",		"nod_asset", "nod_member",
								   "nod_provenance", "nod_asset_crc", "nod_preview_status", "model"};
	for(size_t i = 0; i < bindings->count; ++i) {
		const Entity *entity = &bindings->entities[i];
		const bool is_native = native_geometry(geometry);
		if((entity->brush_count && !fallback_brushes(geometry)) ||
		   entity->pair_count != sizeof(fields) / sizeof(*fields) + is_native + 2 * transformed_models(geometry)) {
			return scene_error("invalid preview-binding property set");
		}

		for(size_t j = 0; j < sizeof(fields) / sizeof(*fields); ++j) {
			if(!get_property(entity, fields[j])) {
				return scene_error("required preview-binding field missing");
			}
		}

		if(transformed_models(geometry)) {
			const char *scale_text	= get_property(entity, "nod_modelscale");
			const char *offset_text = get_property(entity, "nod_modeloffset");
			float scale				= 1;
			float offset[3]			= {};
			if(!scale_text || !offset_text || !parse_float(scale_text, &scale) || !float_vector(offset_text, offset)) {
				return scene_error("invalid native model transform binding");
			}

			if(!strcmp(get_property(entity, "nod_id"), "nil:world") &&
			   (scale != 1 || offset[0] || offset[1] || offset[2])) {
				return scene_error("NIL world cannot have an entity model transform");
			}

			if((scale == 0 && *get_property(entity, "model")) ||
			   (!strcmp(get_property(entity, "nod_preview_status"), "zero-model-scale") && scale != 0)) {
				return scene_error("inconsistent zero-scale model binding");
			}
		}

		if(strcmp(get_property(entity, "classname"), "nod_binding") ||
		   (strncmp(get_property(entity, "nod_id"), "nsd:", 4) &&
			(!model_geometry(geometry) || strcmp(get_property(entity, "nod_id"), "nil:world")))) {
			return scene_error("invalid preview-binding identity");
		}

		if(*get_property(entity, "model") && strcmp(get_property(entity, "nod_preview_status"), "source-mesh")) {
			return scene_error("preview model has inconsistent resolution status");
		}

		if(fallback_brushes(geometry)) {
			const char *model	= get_property(entity, "model");
			const size_t length = strlen(model);
			const bool is_obj	= length >= 4 && !ci(model + length - 4, ".obj");
			if(is_obj != (entity->brush_count != 0)) {
				return scene_error("OBJ fallback must have archived brush geometry; native models must not");
			}
		}

		if(is_native) {
			uint32_t frame		   = 0;
			const char *frame_text = get_property(entity, "_frame");
			const char *model	   = get_property(entity, "model");
			const size_t length	   = strlen(model);
			const bool is_world	   = !strcmp(get_property(entity, "nod_id"), "nil:world");
			if(!frame_text || !parse_unsigned_integer(frame_text, INT32_MAX, &frame)) {
				return scene_error("invalid native model frame");
			}

			if((!*model || is_world || (length >= 4 && !ci(model + length - 4, ".nod"))) && frame) {
				return scene_error("non-bundle models require frame zero");
			}

			if(*model && !is_world &&
			   (strcmp(model, get_property(entity, "nod_asset")) || length < 4 ||
				(ci(model + length - 4, ".nod") && ci(model + length - 4, ".nmb")) ||
				(!ci(model + length - 4, ".nmb") && !*get_property(entity, "nod_member")))) {
				return scene_error("invalid native asset model binding");
			}
		}
	}

	return 1;
}

/*
 */
Scene::~Scene() {
	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		free(sources[i].data);
	}

	free(archive.data);
	free(map.data);
	free(binding_text.data);
	free_map(&bindings);
}

/*
 */
int load_scene(Scene *scene, const char *map_path, const char *archive_path) {
	if(!load(map_path, &scene->map) || !load(archive_path, &scene->archive) ||
	   !archive_read(&scene->archive, scene->sources, &scene->binding_text, &scene->geometry)) {
		return 0;
	}

	if(scene->binding_text.size &&
	   (!parse_map(&scene->binding_text, &scene->bindings) || !validate_bindings(&scene->bindings, scene->geometry))) {
		return 0;
	}

	return compile_map(&scene->map, scene->sources, scene->binding_text.size ? &scene->bindings : nullptr,
					   scene->geometry);
}

} // namespace nod_scene
