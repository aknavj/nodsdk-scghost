/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodscene_export.cpp
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
#include <array>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <set>

#include "scene_bridge.h"
#include "../editors/workspace.h"

/*
 */
namespace nod_scene {

/*
 */
int import_authoring(char **argv) {
	Scene scene;
	std::string error;
	const auto workspace = std::filesystem::u8path(argv[5]);
	const auto assets	 = std::filesystem::u8path(argv[6]);
	if(!std::filesystem::is_directory(workspace) || !std::filesystem::is_directory(assets)) {
		scene_error("import requires existing workspace and original asset directories");
		return EXIT_FAILURE;
	}

	for(int output = 7; output <= 8; ++output) {
		const auto path = std::filesystem::u8path(argv[output]);
		if(!nod::edit::workspace_contains(workspace, path, error) ||
		   nod::edit::workspace_contains(assets, path, error)) {
			scene_error(
				"authoring MAP and source archive outputs must be inside the workspace, outside original assets");
			return EXIT_FAILURE;
		}

		std::error_code ec;
		for(int input = 2; input <= 4; ++input) {
			if(std::filesystem::equivalent(path, std::filesystem::u8path(argv[input]), ec) && !ec) {
				scene_error("authoring output aliases a native input");
				return EXIT_FAILURE;
			}
		}
	}

	const auto map_path		= std::filesystem::u8path(argv[7]);
	const auto archive_path = std::filesystem::u8path(argv[8]);
	const bool overlapping	= nod::edit::workspace_contains(map_path, archive_path, error) ||
							 nod::edit::workspace_contains(archive_path, map_path, error);
	if(!error.empty()) {
		scene_error(error.c_str());
		return EXIT_FAILURE;
	}

	std::error_code ec;
	const bool equivalent = std::filesystem::equivalent(map_path, archive_path, ec);
	if(ec && ec != std::errc::no_such_file_or_directory) {
		scene_error(("Cannot compare authoring output paths: " + ec.message()).c_str());
		return EXIT_FAILURE;
	}

	if(overlapping || equivalent) {
		scene_error("authoring MAP and source archive outputs must be distinct, non-overlapping files");
		return EXIT_FAILURE;
	}

	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		if(!load(argv[2 + i], &scene.sources[i])) {
			return EXIT_FAILURE;
		}
	}

	NSDParser parser;
	if(!parser.parseBuffer(scene.sources[SOURCE_NSD].data, scene.sources[SOURCE_NSD].size)) {
		scene_error(parser.getLastError());
		return EXIT_FAILURE;
	}

	NOTDatabase templates;
	if(!templates.load(workspace, parser.getTemplateFiles(), error, assets)) {
		scene_error(error.c_str());
		return EXIT_FAILURE;
	}

	for(const auto &warning : templates.warnings()) {
		fprintf(stderr, "NODscene import: %s\n", warning.c_str());
	}

	if(!scene_map(scene.sources, &scene.map, nullptr, GEOMETRY_BRUSHES, &templates) ||
	   !archive_write(scene.sources, &scene.archive, nullptr, GEOMETRY_BRUSHES)) {
		return EXIT_FAILURE;
	}

	return save(argv[8], &scene.archive) && save(argv[7], &scene.map) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 */
static std::string json_string(const std::string &text) {
	std::string result		= "\"";
	static const char hex[] = "0123456789abcdef";
	for(unsigned char c : text) {
		if(c == '"' || c == '\\') {
			result += '\\';
			result += static_cast<char>(c);
		} else if(c < 32) {
			result += "\\u00";
			result += hex[c >> 4];
			result += hex[c & 15];
		} else {
			result += static_cast<char>(c);
		}
	}

	return result + '"';
}

/*
 */
int entity_definitions(int argc, char **argv) {
	struct Observed {
			std::string type;
			size_t instances = 0;
			std::set<uint32_t> local;
			std::set<uint32_t> inherited;
	};
	std::map<std::string, Observed> types;
	std::string assets_root;
	std::string inventory_path;
	int first = 3;
	while(first < argc && !strncmp(argv[first], "--", 2)) {
		if(first + 1 >= argc) {
			scene_error("missing entitydefs option value");
			return EXIT_FAILURE;
		}

		if(!strcmp(argv[first], "--assets-root")) {
			assets_root = argv[first + 1];
		} else if(!strcmp(argv[first], "--inventory")) {
			inventory_path = argv[first + 1];
		} else {
			scene_error("unknown entitydefs option");
			return EXIT_FAILURE;
		}

		first += 2;
	}

	if(first == argc) {
		scene_error("entitydefs requires NSD inputs");
		return EXIT_FAILURE;
	}

	if(!inventory_path.empty()) {
		const auto output = std::filesystem::absolute(std::filesystem::u8path(inventory_path)).lexically_normal();
		for(int i = 2; i < argc; ++i) {
			if(i == 2 || i >= first) {
				if(output == std::filesystem::absolute(std::filesystem::u8path(argv[i])).lexically_normal()) {
					scene_error("inventory output must differ from definitions and inputs");
					return EXIT_FAILURE;
				}
			}
		}
	}

	std::ostringstream inventory;
	inventory << "{\"format\":1,\"scope\":\"observed NSD placements, not a complete engine registry\",\"entities\":[";
	size_t entities = 0;
	for(int i = first; i < argc; ++i) {
		Blob source = {};
		NSDParser parser;
		if(!load(argv[i], &source)) {
			return EXIT_FAILURE;
		}

		const bool parsed = parser.parseBuffer(source.data, source.size);
		free(source.data);
		if(!parsed) {
			scene_error(parser.getLastError());
			return EXIT_FAILURE;
		}

		NOTDatabase database;
		std::string error;
		if(!assets_root.empty() && !database.load(assets_root, parser.getTemplateFiles(), error)) {
			scene_error(error.c_str());
			return EXIT_FAILURE;
		}

		const auto &scene = parser.getSceneData();
		for(uint32_t j = 0; j < scene.entity_array_count; ++j) {
			const auto &entity = scene.entities[j];
			auto &observed	   = types[nsd_classname(entity.type_name)];
			observed.type	   = entity.type_name;
			++observed.instances;
			std::vector<NODModifier> local;
			std::vector<NOTParser::ResolvedProperty> effective;
			const bool counted = parser.getModifiers(entity, local);
			error.clear();
			if(!counted) {
				error = "Unverified modifier array; raw scene bytes remain in sidecar";
			} else if(assets_root.empty()) {
				error = "No NOT context supplied; inherited values unavailable";
			} else if(database.resolveModifiers(local.data(), local.size(), effective, error) !=
					  NOTParser::ResolveStatus::ok) {
				fprintf(stderr, "NODscene: %s nsd:%zu: %s\n", argv[i], entity.block_offset, error.c_str());
			}

			if(entities++) {
				inventory << ',';
			}

			inventory << "{\"file\":" << json_string(argv[i]) << ",\"type\":" << json_string(entity.type_name)
					  << ",\"nod_id\":" << json_string("nsd:" + std::to_string(entity.block_offset))
					  << ",\"block_size\":" << entity.block_size << ",\"counted\":" << (counted ? "true" : "false")
					  << ",\"resolution_error\":" << (error.empty() ? "null" : json_string(error));
			const auto emit = [&](const NODModifier &property, const std::string &provenance) {
				const auto *info = nodPropertyInfo(property.prop_id);
				std::string value;
				std::string raw;
				static const char hex[] = "0123456789abcdef";
				for(unsigned char byte : property.value) {
					raw += hex[byte >> 4];
					raw += hex[byte & 15];
				}

				inventory << "{\"id\":" << property.prop_id << ",\"name\":" << (info ? json_string(info->name) : "null")
						  << ",\"native_type\":" << (info ? std::to_string(info->native_type) : "null") << ",\"value\":"
						  << (nsd_property_text(property.prop_id, property.value, value) ? json_string(value) : "null")
						  << ",\"raw\":" << json_string(raw) << ",\"source\":" << json_string(provenance) << '}';
			};

			inventory << ",\"modifiers\":[";
			for(size_t p = 0; p < local.size(); ++p) {
				if(p) {
					inventory << ',';
				}

				observed.local.insert(local[p].prop_id);
				emit(local[p], "nsd:" + std::to_string(local[p].value_offset));
			}

			inventory << "],\"effective\":[";
			for(size_t p = 0; p < effective.size(); ++p) {
				if(p) {
					inventory << ',';
				}

				const auto &resolved   = effective[p];
				std::string provenance = "local";
				if(resolved.owner) {
					observed.inherited.insert(resolved.property->prop_id);
					const auto *archive = database.source(resolved.owner);
					if(!archive) {
						scene_error("missing inventory property provenance");
						return EXIT_FAILURE;
					}

					provenance = archive->name + ":" + resolved.owner->name;
				}

				emit(*resolved.property, provenance);
			}

			inventory << "]}";
		}
	}

	inventory << "]}";
	if(types.empty()) {
		scene_error("no tag-validated NSD types found for entity definitions");
		return EXIT_FAILURE;
	}

	FILE *file = tmpfile();
	if(!file) {
		scene_error("cannot create entity-definition temporary stream");
		return EXIT_FAILURE;
	}

	bool success = true;
	for(const auto &entry : types) {
		std::string display = entry.second.type;
		for(size_t pos = 0; (pos = display.find("*/", pos)) != std::string::npos; pos += 3) {
			display.replace(pos, 2, "* /");
		}

		if(fprintf(file,
				   "/*QUAKED %s (1 0.6 0.2) (-0.125 -0.125 -0.125) (0.125 0.125 0.125)\n"
				   "Native NSD type: %s\n"
				   "New instances are not yet compilable.\n"
				   "Edit origin, existing nod_sector, angles (models) or nod_rotation.\n"
				   "nod_geometry: edit brushes only; placement read-only.\n"
				   "Schema 14/15: existing local properties only; integers are uint32 bits.\n"
				   "No modifier/inheritance changes. Inherited values: NOTEditor.\n"
				   "Preview metadata read-only; model/offset/renderflag/script edits require re-enrichment.\n",
				   entry.first.c_str(), display.c_str()) < 0) {
			success = false;
			break;
		}

		if(fprintf(file, "Observed instances: %zu\n", entry.second.instances) < 0) {
			success = false;
			break;
		}

		auto properties = entry.second.local;
		properties.insert(entry.second.inherited.begin(), entry.second.inherited.end());
		for(uint32_t id : properties) {
			const auto *info	  = nodPropertyInfo(id);
			const bool local	  = entry.second.local.count(id) != 0;
			const char *kind	  = !info					 ? "raw"
									: info->native_type == 0 ? "string32"
									: info->native_type == 2 ? "float32"
									: info->native_type == 3 ? "uint32 bits"
									: info->native_type == 4 ? "vector3"
															 : "raw";
			const char *access	  = !local ? "inherited"
									: nsd_property_editable(id, false)
										? (nsd_property_editable(id, true) ? "editable local payload"
																		   : "source-map edit; re-enrich preview")
										: "read-only";
			const std::string key = id == 1	  ? "nod_rotation"
									: id == 2 ? "origin"
									: id == 3
										? "nod_sector"
										: std::string(!info || (info->native_type != 0 && info->native_type != 2 &&
																info->native_type != 3 && info->native_type != 4)
														  ? "nod_raw_"
														  : "nod_prop_") +
											  std::to_string(id);
			if(fprintf(file, "\"%s\" - %s; %s; %s.\n", key.c_str(), info ? info->name : "Unknown", kind, access) < 0) {
				success = false;
			}
		}

		if(fputs("*/\n\n", file) == EOF) {
			success = false;
		}
	}

	Blob definitions = {};
	if(success) {
		success = read_stream(file, &definitions) != 0;
	}

	if(fclose(file)) {
		success = false;
	}

	if(!success) {
		scene_error("cannot generate native entity definitions");
	}

	if(success && !inventory_path.empty()) {
		std::string text = inventory.str();
		Blob report		 = {reinterpret_cast<unsigned char *>(text.data()), text.size()};
		success			 = save(inventory_path.c_str(), &report) != 0;
	}

	if(success) {
		success = save(argv[2], &definitions) != 0;
	}

	free(definitions.data);
	if(success) {
		printf("NODscene: generated %zu observed native type definitions\n", types.size());
	}

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 */
static int obj_brushes(const char *root, const NodBinding *binding, const float origin[3], const float rotation[3],
					   Blob *brushes) {
	char path[2048] = {};
	Blob object		= {};
	FILE *output	= nullptr;
	if(!path_join(path, sizeof(path), root, binding->preview) || !load(path, &object)) {
		return 0;
	}

	std::istringstream input(std::string(reinterpret_cast<const char *>(object.data), object.size));
	free(object.data);
	std::vector<std::array<double, 3>> vertices;
	std::vector<std::array<double, 2>> texcoords;
	size_t normals = 0;
	std::string line;
	std::string material;
	const double radians	  = acos(-1.0) / 180;
	const double sx			  = sin(rotation[0] * radians);
	const double cx			  = cos(rotation[0] * radians);
	const double sy			  = sin(rotation[1] * radians);
	const double cy			  = cos(rotation[1] * radians);
	const double sz			  = sin(rotation[2] * radians);
	const double cz			  = cos(rotation[2] * radians);
	const double matrix[3][3] = {{cy * cz + sy * sx * sz, -cy * sz + sy * sx * cz, sy * cx},
								 {cx * sz, cx * cz, -sx},
								 {-sy * cz + cy * sx * sz, sy * sz + cy * sx * cz, cy * cx}};
	output					  = tmpfile();
	if(!output) {
		return scene_error("cannot create brush fallback temporary stream");
	}

	size_t count = 0;
	int success	 = 0;
	while(std::getline(input, line)) {
		std::istringstream fields(line);
		std::string kind;
		std::string extra;
		fields >> kind;
		if(kind.empty() || kind[0] == '#' || kind == "mtllib" || kind == "g") {
			continue;
		}

		if(kind == "v" || kind == "vt" || kind == "vn") {
			double values[3]		= {};
			const size_t components = kind == "vt" ? 2 : 3;
			for(size_t axis = 0; axis < components; ++axis) {
				if(!(fields >> values[axis]) || !isfinite(values[axis])) {
					scene_error("invalid generated OBJ vertex attribute");
					goto cleanup;
				}
			}

			if(fields >> extra) {
				scene_error("unexpected generated OBJ vertex data");
				goto cleanup;
			}

			if(kind == "v") {
				std::array<double, 3> point = {};
				for(size_t axis = 0; axis < 3; ++axis) {
					point[axis] = double(origin[axis]) + binding->model_offset[axis];
					for(size_t component = 0; component < 3; ++component) {
						point[axis] += matrix[axis][component] * values[component] * binding->model_scale;
					}

					if(!isfinite(point[axis]) || fabs(point[axis]) > FLT_MAX) {
						scene_error("model transform exceeds float32 coordinate range");
						goto cleanup;
					}
				}

				vertices.push_back(point);
			} else if(kind == "vt") {
				texcoords.push_back({values[0], -values[1]});
			} else {
				++normals;
			}
		} else if(kind == "usemtl") {
			if(!(fields >> material) || (fields >> extra)) {
				scene_error("invalid generated OBJ material");
				goto cleanup;
			}
		} else if(kind == "f") {
			double points[3][3] = {};
			double uv[3][2]		= {};
			if(material.empty()) {
				scene_error("generated OBJ triangle lacks material");
				goto cleanup;
			}

			for(size_t vertex = 0; vertex < 3; ++vertex) {
				std::string reference;
				uint32_t indices[3] = {};
				if(!(fields >> reference)) {
					scene_error("generated OBJ face is not a triangle");
					goto cleanup;
				}

				for(size_t part = 0; part < 3; ++part) {
					const size_t slash = reference.find('/');
					if((part < 2 && slash == std::string::npos) || (part == 2 && slash != std::string::npos) ||
					   !parse_unsigned_integer(reference.substr(0, slash).c_str(), UINT32_MAX, &indices[part]) ||
					   !indices[part]) {
						scene_error("invalid generated OBJ face index");
						goto cleanup;
					}

					if(part < 2) {
						reference.erase(0, slash + 1);
					}
				}

				if(indices[0] > vertices.size() || indices[1] > texcoords.size() || indices[2] > normals) {
					scene_error("generated OBJ face outside attribute buffers");
					goto cleanup;
				}

				for(size_t axis = 0; axis < 3; ++axis) {
					points[vertex][axis] = vertices[indices[0] - 1][axis];
				}

				for(size_t axis = 0; axis < 2; ++axis) {
					uv[vertex][axis] = texcoords[indices[1] - 1][axis];
				}
			}

			if(fields >> extra) {
				scene_error("generated OBJ face is not a triangle");
				goto cleanup;
			}

			int written = 0;
			if(!niltomap_write_triangle_uv(output, points, uv, material.c_str(), 0.01, &written)) {
				goto cleanup;
			}

			count += static_cast<size_t>(written);
		} else {
			scene_error("unsupported generated OBJ record");
			goto cleanup;
		}
	}

	if(!read_stream(output, brushes)) {
		goto cleanup;
	}

	printf("NODscene: converted %s to %zu editable triangle brushes (thickness 0.01)\n", binding->preview, count);
	success = 1;
cleanup:
	if(fclose(output)) {
		success = scene_error("cannot close brush fallback temporary stream");
	}

	return success;
}

/*
 */
int write_binding(FILE *file, const char *identity, const NodBinding *binding, bool native_models,
				  const Blob *brushes) {
	char checksum[16] = {};
	char scale[64]	  = {};
	snprintf(checksum, sizeof(checksum), "%08x", static_cast<unsigned>(binding->checksum));
	snprintf(scale, sizeof(scale), "%.9g", static_cast<double>(binding->model_scale));
	if(fputs("{\n", file) == EOF || !write_key(file, "classname", "nod_binding") ||
	   !write_key(file, "nod_id", identity) || !write_key(file, "nod_reference", binding->reference) ||
	   !write_key(file, "nod_asset", binding->asset) || !write_key(file, "nod_member", binding->member) ||
	   !write_key(file, "nod_provenance", binding->provenance) || !write_key(file, "nod_asset_crc", checksum) ||
	   !write_key(file, "nod_preview_status", binding->status) || !write_key(file, "model", binding->preview) ||
	   !write_key(file, "nod_modelscale", scale) || !write_vector_key(file, "nod_modeloffset", binding->model_offset) ||
	   (native_models && !write_integer_key(file, "_frame", binding->frame)) ||
	   (brushes->size && fwrite(brushes->data, 1, brushes->size, file) != brushes->size) || fputs("}\n", file) == EOF) {
		return scene_error("cannot write preview bindings");
	}

	return 1;
}

/*
 */
int preview_bindings(const Blob sources[SOURCE_COUNT], const char *root, const char *bundle, const char *preview_root,
					 Blob *text, bool native_models) {
	NSDParser native_parser;
	const auto &native_scene = native_parser.getSceneData();
	Blob baseline			 = {};
	Map map					 = {};
	NodAssets *assets		 = nullptr;
	FILE *file				 = nullptr;
	size_t resolved_count	 = 0;
	size_t entity_count		 = 0;
	int is_successful		 = 0;
	if(!native_parser.parseBuffer(sources[SOURCE_NSD].data, sources[SOURCE_NSD].size)) {
		return scene_error(native_parser.getLastError());
	}

	if(!scene_map(sources, &baseline, nullptr, GEOMETRY_MODELS) || !parse_map(&baseline, &map)) {
		goto cleanup;
	}

	{
		std::vector<const char *> archives;
		for(const auto &name : native_parser.getTemplateFiles()) {
			archives.push_back(name.c_str());
		}

		assets = native_models ? nod_assets_open_native(root, bundle, preview_root, archives.data(), archives.size())
							   : nod_assets_open(root, bundle, preview_root, archives.data(), archives.size());
	}

	file = tmpfile();
	if(!assets || !file) {
		if(!file) {
			scene_error("cannot open preview-binding temporary file");
		}

		goto cleanup;
	}

	{
		NodBinding world	= {};
		Blob brushes		= {};
		const float zero[3] = {};
		const bool success	= nod_assets_nil_preview(assets, &sources[SOURCE_NIL], &world) &&
							 (!*world.preview || obj_brushes(preview_root, &world, zero, zero, &brushes)) &&
							 write_binding(file, "nil:world", &world, native_models, &brushes);
		free(brushes.data);
		if(!success) {
			goto cleanup;
		}
	}

	for(size_t i = 0; i < map.count; ++i) {
		const Entity *entity				 = &map.entities[i];
		const NSDParser::SceneEntity *native = nullptr;
		NodBinding binding					 = {};
		Blob brushes						 = {};
		uint32_t offset						 = 0;
		if(strcmp(get_property(entity, "classname"), "nod_nsd_entity")) {
			continue;
		}

		if(!parse_unsigned_integer(get_property(entity, "nod_id") + nsd_identity_prefix_size, UINT32_MAX, &offset)) {
			goto cleanup;
		}

		for(size_t j = 0; j < native_scene.entity_array_count; ++j) {
			if(native_scene.entities[j].block_offset == offset) {
				native = &native_scene.entities[j];
			}
		}

		if(!native) {
			scene_error("native entity identity missing from shared parser");
			goto cleanup;
		}

		std::vector<NODModifier> modifiers;
		const bool counted = native_parser.getModifiers(*native, modifiers);
		if(!counted) {
			fprintf(stderr, "NODscene: %s: unverified modifier array; using legacy placement preview selection\n",
					get_property(entity, "nod_id"));
		}

		if(!counted && native->has_model_override && !native->model_override_valid) {
			scene_error("non-ASCII explicit NSD model reference");
			goto cleanup;
		}

		if(!nod_assets_resolve(
			   assets, get_property(entity, "nod_type"), native->has_model_override ? native->model_name : nullptr,
			   counted ? sources[SOURCE_NSD].data + native->block_offset - 4 : nullptr, modifiers.size(), &binding)) {
			goto cleanup;
		}

		if(binding.model_scale == 0 && *binding.preview) {
			binding.preview[0] = 0;
			strcpy(binding.status, "zero-model-scale");
		}

		const size_t length = strlen(binding.preview);
		const bool is_obj	= length >= 4 && !ci(binding.preview + length - 4, ".obj");
		const bool success =
			(!is_obj || obj_brushes(preview_root, &binding, native->position, native->rotation, &brushes)) &&
			write_binding(file, get_property(entity, "nod_id"), &binding, native_models, &brushes);
		free(brushes.data);
		if(!success) {
			goto cleanup;
		}

		++entity_count;
		if(*binding.preview) {
			++resolved_count;
		} else {
			fprintf(stderr, "NODscene: %s (%s): %s; model='%s'\n", get_property(entity, "nod_id"),
					get_property(entity, "nod_type"), binding.status, binding.reference);
		}
	}

	if(!read_stream(file, text)) {
		goto cleanup;
	}

	printf("NODscene: resolved %zu/%zu native entities to source-mesh previews\n", resolved_count, entity_count);
	is_successful = 1;
cleanup:
	if(file && fclose(file)) {
		is_successful = scene_error("cannot close preview-binding temporary file");
	}

	nod_assets_close(assets);
	free(baseline.data);
	free_map(&map);
	return is_successful;
}

/*
 */
int export_scene(Blob sources[SOURCE_COUNT], Blob *map, Blob *archive, char **argv) {
	for(size_t i = 0; i < SOURCE_COUNT; ++i) {
		if(!load(argv[2 + i], &sources[i])) {
			return 0;
		}
	}

	const GeometryMode geometry =
		!strcmp(argv[1], "export-properties") ? GEOMETRY_PROPERTIES_MODELS : GEOMETRY_TYPED_MODELS;
	if(!scene_map(sources, map, nullptr, geometry) || !archive_write(sources, archive, nullptr, geometry)) {
		return 0;
	}

	return save(argv[6], archive) && save(argv[5], map);
}

/*
 */
int export_scene(char **arguments) {
	Scene scene;
	if(!strcmp(arguments[1], "export") || !strcmp(arguments[1], "export-properties")) {
		return export_scene(scene.sources, &scene.map, &scene.archive, arguments) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if(!load_scene(&scene, arguments[2], arguments[3])) {
		return EXIT_FAILURE;
	}

	free(scene.map.data);
	scene.map = {};
	free(scene.archive.data);
	scene.archive = {};
	free(scene.binding_text.data);
	scene.binding_text = {};
	free_map(&scene.bindings);
	scene.bindings = {};

	const bool native_models	= !strcmp(arguments[1], "preview");
	const GeometryMode geometry = native_properties(scene.geometry)
									  ? (native_models ? GEOMETRY_PROPERTIES_NATIVE : GEOMETRY_PROPERTIES_MODELS)
									  : (native_models ? GEOMETRY_TRANSFORMED_NATIVE : GEOMETRY_TRANSFORMED_MODELS);
	if(!preview_bindings(scene.sources, arguments[4], arguments[5], arguments[6], &scene.binding_text, native_models)) {
		return EXIT_FAILURE;
	}

	if(scene.binding_text.size &&
	   (!parse_map(&scene.binding_text, &scene.bindings) || !validate_bindings(&scene.bindings, geometry))) {
		return EXIT_FAILURE;
	}

	if(!scene_map(scene.sources, &scene.map, scene.binding_text.size ? &scene.bindings : nullptr, geometry) ||
	   !archive_write(scene.sources, &scene.archive, &scene.binding_text, geometry) ||
	   !save(arguments[8], &scene.archive) || !save(arguments[7], &scene.map)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

} // namespace nod_scene
