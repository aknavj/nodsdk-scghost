/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: scene_bridge.h
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

#ifndef __SCENE_BRIDGE_H__
#define __SCENE_BRIDGE_H__

// forward declarations
class NOTDatabase;

#include "tool_common.h"
#include <filesystem>
#include <string>

/*
 */
namespace nod_scene {

/*
 */
enum { 
	SOURCE_NIL, 
	SOURCE_NSD, 
	SOURCE_NPD, 
	SOURCE_COUNT 
};

/*
 */
constexpr size_t map_key_capacity			  = 64;
constexpr size_t map_value_capacity			  = 256;
constexpr size_t native_name_size			  = 32;
constexpr size_t vector_components			  = 3;
constexpr size_t brush_coordinate_count		  = vector_components * vector_components;
constexpr size_t brush_texture_fields		  = 5;
constexpr size_t brush_flag_fields			  = 3;
constexpr size_t stream_buffer_size			  = 8192;
constexpr size_t sidecar_magic_size			  = 8;
constexpr size_t sidecar_header_size		  = 44;
constexpr size_t sidecar_descriptor_size	  = 12;
constexpr size_t sidecar_bindings_header_size = 8;
constexpr size_t npd_header_size			  = 8;
constexpr size_t npd_record_size			  = 26;
constexpr size_t npd_link_size				  = 6;
constexpr size_t npd_position_offset		  = 2;
constexpr size_t npd_flags_offset			  = 14;
constexpr size_t npd_color_offset			  = 16;
constexpr size_t npd_extra_offset			  = 20;
constexpr size_t npd_extra_fields			  = 3;
constexpr size_t nil_shader_count_offset	  = 92;
constexpr size_t nil_shader_table_offset	  = 96;
constexpr size_t nsd_minimum_entity_size	  = 84;
constexpr size_t nsd_rotation_offset		  = 36;
constexpr size_t nsd_position_offset		  = 72;
constexpr size_t nsd_sector_tag_offset		  = 104;
constexpr size_t nsd_sector_offset			  = 108;
constexpr size_t nsd_sector_block_size		  = 140;
constexpr size_t nsd_identity_prefix_size	  = sizeof("nsd:") - 1;
constexpr size_t link_identity_prefix_size	  = sizeof("link:") - 1;
constexpr size_t native_word_size			  = sizeof(uint32_t);
constexpr size_t native_halfword_size		  = sizeof(uint16_t);
constexpr int export_argument_count			  = 7;
constexpr int preview_argument_count		  = 9;

/*
 */
struct Text {
		// Owns its allocation until explicitly transferred to the resulting brush.
		char *data;
		size_t size;
		size_t capacity;
};

/*
 */
struct Pair {
		char key[map_key_capacity];
		char value[map_value_capacity];
};

/*
 */
struct Entity {
		Pair *pairs;
		size_t pair_count;
		char **brushes;
		size_t brush_count;
};

/*
 */
struct Map {
		// Owns entities, their properties, and their brush strings; released by free_map.
		Entity *entities;
		size_t count;
};

/*
 */
struct Lexer {
		const unsigned char *data;
		size_t size;
		size_t offset;
		char token[map_value_capacity];
		bool is_quoted;
};

/*
 */
enum GeometryMode {
	GEOMETRY_LEGACY,
	GEOMETRY_BRUSHES,
	GEOMETRY_MODELS,
	GEOMETRY_NATIVE,
	GEOMETRY_MODELS_BOUNDS,
	GEOMETRY_NATIVE_BOUNDS,
	GEOMETRY_MODELS_FALLBACK_BRUSHES,
	GEOMETRY_NATIVE_FALLBACK_BRUSHES,
	GEOMETRY_TYPED_MODELS,
	GEOMETRY_TYPED_NATIVE,
	GEOMETRY_TRANSFORMED_MODELS,
	GEOMETRY_TRANSFORMED_NATIVE,
	GEOMETRY_PROPERTIES_MODELS,
	GEOMETRY_PROPERTIES_NATIVE
};

/*
 */
inline bool native_properties(GeometryMode geometry) {
	return geometry == GEOMETRY_PROPERTIES_MODELS || geometry == GEOMETRY_PROPERTIES_NATIVE;
}

/*
 */
inline bool typed_entities(GeometryMode geometry) {
	return geometry == GEOMETRY_TYPED_MODELS || geometry == GEOMETRY_TYPED_NATIVE ||
		   geometry == GEOMETRY_TRANSFORMED_MODELS || geometry == GEOMETRY_TRANSFORMED_NATIVE ||
		   native_properties(geometry);
}

/*
 */
inline bool transformed_models(GeometryMode geometry) {
	return geometry == GEOMETRY_TRANSFORMED_MODELS || geometry == GEOMETRY_TRANSFORMED_NATIVE ||
		   native_properties(geometry);
}

/*
 */
inline bool fallback_brushes(GeometryMode geometry) {
	return geometry == GEOMETRY_MODELS_FALLBACK_BRUSHES || geometry == GEOMETRY_NATIVE_FALLBACK_BRUSHES ||
		   typed_entities(geometry);
}

/*
 */
inline bool native_geometry(GeometryMode geometry) {
	return geometry == GEOMETRY_NATIVE || geometry == GEOMETRY_NATIVE_BOUNDS ||
		   geometry == GEOMETRY_NATIVE_FALLBACK_BRUSHES || geometry == GEOMETRY_TYPED_NATIVE ||
		   geometry == GEOMETRY_TRANSFORMED_NATIVE || geometry == GEOMETRY_PROPERTIES_NATIVE;
}

/*
 */
inline bool model_geometry(GeometryMode geometry) {
	return geometry == GEOMETRY_MODELS || geometry == GEOMETRY_MODELS_BOUNDS ||
		   geometry == GEOMETRY_MODELS_FALLBACK_BRUSHES || geometry == GEOMETRY_TYPED_MODELS ||
		   geometry == GEOMETRY_TRANSFORMED_MODELS || geometry == GEOMETRY_PROPERTIES_MODELS ||
		   native_geometry(geometry);
}

/*
 */
inline bool entity_bounds(GeometryMode geometry) {
	return geometry == GEOMETRY_MODELS_BOUNDS || geometry == GEOMETRY_NATIVE_BOUNDS || fallback_brushes(geometry);
}

/*
 */
struct Scene {
		Blob sources[SOURCE_COUNT] = {};
		Blob archive			   = {};
		Blob map				   = {};
		Blob binding_text		   = {};
		Map bindings			   = {};
		GeometryMode geometry	   = GEOMETRY_TYPED_MODELS;

		Scene() = default;
		~Scene();
		Scene(const Scene &)			= delete;
		Scene &operator=(const Scene &) = delete;
};

/*
 */
int scene_error(const char *message);
const char *get_property(const Entity *entity, const char *key_name);
const char *entity_id(const Entity *entity);
int read_token(Lexer *lexer);
int parse_float(const char *text, float *result);
int float_vector(const char *text, float vector[vector_components]);
int write_vector_key(FILE *file, const char *key_name, const float *vector);
int parse_unsigned_integer(const char *text, uint32_t maximum, uint32_t *result);
int write_key(FILE *file, const char *key_name, const char *value);
int write_integer_key(FILE *file, const char *key_name, uint32_t value);
int scene_map(const Blob sources[SOURCE_COUNT], Blob *map, const Map *bindings, GeometryMode geometry,
			  const NOTDatabase *templates = nullptr);
int archive_write(const Blob sources[SOURCE_COUNT], Blob *archive, const Blob *bindings, GeometryMode geometry);
int validate_bindings(const Map *bindings, GeometryMode geometry);
int parse_map(const Blob *blob, Map *map);
// New authoring maps have no archived nod_id; retain entity and brush order.
int parse_authoring_map(const Blob *blob, Map *map);
void free_map(Map *map);

/*
 */
class AuthoringMap {
	public:
		AuthoringMap() = default;
		~AuthoringMap();
		AuthoringMap(const AuthoringMap &)			  = delete;
		AuthoringMap &operator=(const AuthoringMap &) = delete;

		bool loadFile(const std::filesystem::path &path, std::string &error);
		const Map &getMap() const { return map_data; }

	private:
		Map map_data{};
};
int compile_map(const Blob *input, Blob sources[SOURCE_COUNT], const Map *bindings, GeometryMode geometry);
int load_scene(Scene *scene, const char *map_path, const char *archive_path);
int export_scene(char **arguments);
int compile_scene(char **arguments);
std::string nsd_classname(const char *type);
bool nsd_property_text(uint32_t id, const unsigned char *payload, std::string &text);
bool nsd_property_id(const char *key, uint32_t *id);
bool nsd_property_editable(uint32_t id, bool preview);
int entity_definitions(int argc, char **argv);
int import_authoring(char **argv);

} // namespace nod_scene

#endif /* __SCENE_BRIDGE_H__ */
