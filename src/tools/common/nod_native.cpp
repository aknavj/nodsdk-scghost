/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_native.cpp
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

#include "nod_native.h"

#include "NMBParser.h"
#include "NODParser.h"
#include "NOTDatabase.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

/*
 */
struct NodTemplates {
		NOTDatabase parser;
};

/*
 */
extern "C" NodTemplates *nod_templates_open(const char *root, const char *const *archives, size_t count) {
	if(count && !archives) {
		fprintf(stderr, "NODscene: templates: missing archive-name array\n");
		return nullptr;
	}

	std::vector<std::string> names;
	for(size_t i = 0; i < count; ++i) {
		if(!archives[i]) {
			fprintf(stderr, "NODscene: templates: missing archive name\n");
			return nullptr;
		}

		names.emplace_back(archives[i]);
	}

	auto templates = std::make_unique<NodTemplates>();
	std::string error;
	if(!templates->parser.load(std::filesystem::u8path(root), names, error)) {
		fprintf(stderr, "NODscene: templates: %s\n", error.c_str());
		return nullptr;
	}

	for(const auto &warning : templates->parser.warnings()) {
		fprintf(stderr, "NODscene: templates: %s\n", warning.c_str());
	}

	return templates.release();
}

/*
 */
extern "C" void nod_templates_close(NodTemplates *templates) {
	delete templates;
}

/*
 */
extern "C" int nod_templates_model(const NodTemplates *templates, const char *type, const unsigned char *modifiers,
								   size_t count, NodBinding *binding) {
	std::vector<NOTParser::ResolvedProperty> properties;
	std::vector<NODModifier> local;
	if(modifiers) {
		if(count > NOTParser::MAX_PROPERTIES) {
			fprintf(stderr, "NODscene: templates: invalid placement modifier count\n");
			return 0;
		}

		for(size_t i = 0; i < count; ++i) {
			NODModifier property{};
			property.prop_id	= u32(modifiers + i * 36);
			property.value_size = 32;
			memcpy(property.value, modifiers + i * 36 + 4, 32);
			local.push_back(property);
		}
	} else if(!templates->parser.findTemplate(type)) {
		strcpy(binding->status, "unknown-template");
		return 1;
	}

	std::string error;
	const auto status = modifiers ? templates->parser.resolveModifiers(local.data(), local.size(), properties, error)
								  : templates->parser.resolveModifiers(type, properties, error);
	if(status != NOTParser::ResolveStatus::ok) {
		if(status == NOTParser::ResolveStatus::missing_template) {
			strcpy(binding->status, "missing-template-parent");
			fprintf(stderr, "NODscene: templates: %s: %s\n", type, error.c_str());
			return 1;
		}

		fprintf(stderr, "NODscene: templates: %s: %s\n", type, error.c_str());
		return 0;
	}

	NOTDatabase::ModelPreview preview;
	if(!NOTDatabase::modelPreview(properties, preview, error)) {
		fprintf(stderr, "NODscene: templates: %s: %s\n", type, error.c_str());
		return 0;
	}

	binding->model_scale = preview.scale;
	std::copy_n(preview.offset, 3, binding->model_offset);
	for(const auto &resolved : properties) {
		if(resolved.property->prop_id != 4) {
			continue;
		}

		memcpy(binding->reference, preview.model.c_str(), preview.model.size() + 1);
		if(preview.owner) {
			const auto *source = templates->parser.source(preview.owner);
			if(!source) {
				fprintf(stderr, "NODscene: templates: missing property archive provenance\n");
				return 0;
			}

			if(source->name == "global.not") {
				snprintf(binding->provenance, sizeof(binding->provenance), "not:%s:property4", preview.owner->name);
			} else {
				snprintf(binding->provenance, sizeof(binding->provenance), "not:%s:%s:property4", source->name.c_str(),
						 preview.owner->name);
			}
		} else {
			strcpy(binding->provenance, "nsd:tag4");
		}

		return 1;
	}

	strcpy(binding->status, "no-model");
	return 1;
}

namespace {

/*
 */
constexpr size_t vertex_attribute_bytes = 8 * sizeof(float);
constexpr size_t color_bytes			= 4;
constexpr uint16_t strip_restart		= UINT16_MAX;

/*
 */
struct NativeFile {
		std::unique_ptr<NODParser> model;
		std::unique_ptr<NMBParser> bundle;
		uint32_t checksum = 0;
};

/*
 */
struct NativeModel {
		const NODParser::VertexSet *vertex_sets;
		uint32_t vertex_set_count;
		const uint8_t *vertex_data;
		const uint16_t *indices;
		uint32_t index_count;
		const char (*shader_names)[33];
		uint32_t shader_count;
		const NODParser::MeshGroup *mesh_groups;
		uint32_t mesh_group_count;
		const uint8_t *colors;
};

/*
 */
template <typename Model> NativeModel model_view(const Model &model, const uint8_t *colors) {
	return {model.vertex_sets,	model.vertex_set_count, model.vertex_data, model.indices,		   model.index_count,
			model.shader_names, model.shader_count,		model.mesh_groups, model.mesh_group_count, colors};
}

/*
 */
int native_error(const char *message) {
	fprintf(stderr, "NODscene: native assets: %s\n", message);
	return 0;
}

/*
 */
std::string virtual_path(std::string path) {
	std::replace(path.begin(), path.end(), '\\', '/');
	return path;
}

/*
 */
bool has_extension(const std::string &path, const char *extension) {
	return path.size() >= 4 && !ci(path.c_str() + path.size() - 4, extension);
}

/*
 */
bool reference_matches(const std::string &reference, const std::string &path) {
	if(has_extension(reference, ".nod") || has_extension(reference, ".nmb")) {
		return !ci(reference.c_str(), path.c_str());
	}

	return (has_extension(path, ".nod") || has_extension(path, ".nmb")) &&
		   !ci(reference.c_str(), path.substr(0, path.size() - 4).c_str());
}

/*
 */
bool safe_reference(const std::string &path) {
	if(path.empty() || path.front() == '/' || path.find(':') != std::string::npos) {
		return false;
	}

	size_t start = 0;
	while(start < path.size()) {
		const size_t end	   = path.find('/', start);
		const std::string part = path.substr(start, end - start);
		if(part.empty() || part == "." || part == "..") {
			return false;
		}

		if(end == std::string::npos) {
			return true;
		}

		start = end + 1;
	}

	return false;
}

/*
 */
const uint8_t *vertex_attributes(const NativeModel &model, uint32_t set_index, uint32_t vertex) {
	const auto &set = model.vertex_sets[set_index];
	return model.vertex_data + set.data_offset + static_cast<size_t>(vertex) * set.base_stride;
}

/*
 */
int validate_model(const NativeModel &model, bool *has_triangles) {
	*has_triangles = false;
	for(uint32_t s = 0; s < model.vertex_set_count; ++s) {
		for(uint32_t v = 0; v < model.vertex_sets[s].count; ++v) {
			const uint8_t *attributes = vertex_attributes(model, s, v);
			for(size_t a = 0; a < vertex_attribute_bytes; a += sizeof(float)) {
				if(!std::isfinite(f32(attributes + a))) {
					return native_error("non-finite native vertex attributes");
				}
			}
		}
	}

	for(uint32_t g = 0; g < model.mesh_group_count; ++g) {
		const auto &group = model.mesh_groups[g];
		const auto &lod	  = group.lods[0];
		if(group.vertex_set_index >= model.vertex_set_count || group.shader_index >= model.shader_count ||
		   lod.list_count % 3 || lod.list_start > model.index_count ||
		   lod.list_count > model.index_count - lod.list_start || lod.strip_start > model.index_count ||
		   lod.strip_count > model.index_count - lod.strip_start) {
			return native_error("invalid native LOD-0 group");
		}

		const uint32_t count = model.vertex_sets[group.vertex_set_index].count;
		auto triangle		 = [&](uint16_t a, uint16_t b, uint16_t c) {
			   if(a == b || b == c || a == c) {
				   return true;
			   }

			   if(static_cast<uint32_t>(a) + group.base_vertex >= count ||
				  static_cast<uint32_t>(b) + group.base_vertex >= count ||
				  static_cast<uint32_t>(c) + group.base_vertex >= count) {
				   return false;
			   }

			   *has_triangles = true;
			   return true;
		};
		for(uint32_t i = 0; i < lod.list_count; i += 3) {
			const uint16_t *indices = model.indices + lod.list_start + i;
			if(!triangle(indices[0], indices[1], indices[2])) {
				return native_error("native triangle outside vertex set");
			}
		}

		uint16_t previous[2] = {};
		uint32_t count_seen	 = 0;
		for(uint32_t i = 0; i < lod.strip_count; ++i) {
			const uint16_t index = model.indices[lod.strip_start + i];
			if(index == strip_restart) {
				count_seen = 0;
			} else if(count_seen < 2) {
				previous[count_seen++] = index;
			} else {
				if(!triangle(previous[0], previous[1], index)) {
					return native_error("native strip outside vertex set");
				}

				previous[0] = previous[1];
				previous[1] = index;
			}
		}
	}

	return 1;
}

/*
 */
// Compare the attributes Radiant renders, not incompatible NOD/NMB binary headers.
bool equivalent_render_data(const NativeModel &standalone, const NativeModel &member) {
	if(standalone.vertex_set_count != member.vertex_set_count ||
	   standalone.mesh_group_count != member.mesh_group_count || standalone.index_count != member.index_count) {
		return false;
	}

	for(uint32_t s = 0; s < member.vertex_set_count; ++s) {
		if(standalone.vertex_sets[s].count != member.vertex_sets[s].count) {
			return false;
		}

		for(uint32_t v = 0; v < member.vertex_sets[s].count; ++v) {
			if(memcmp(vertex_attributes(standalone, s, v), vertex_attributes(member, s, v), vertex_attribute_bytes)) {
				return false;
			}
		}
	}

	if(member.index_count && memcmp(standalone.indices, member.indices, member.index_count * sizeof(uint16_t))) {
		return false;
	}

	for(uint32_t g = 0; g < member.mesh_group_count; ++g) {
		const auto &left  = standalone.mesh_groups[g];
		const auto &right = member.mesh_groups[g];
		const auto &a	  = left.lods[0];
		const auto &b	  = right.lods[0];
		if(left.vertex_set_index != right.vertex_set_index || left.base_vertex != right.base_vertex ||
		   strcmp(standalone.shader_names[left.shader_index], member.shader_names[right.shader_index]) ||
		   a.strip_start != b.strip_start || a.strip_count != b.strip_count || a.list_start != b.list_start ||
		   a.list_count != b.list_count) {
			return false;
		}
	}

	if(member.colors) {
		size_t vertex_count = 0;
		for(uint32_t s = 0; s < member.vertex_set_count; ++s) {
			vertex_count += member.vertex_sets[s].count;
		}

		for(size_t i = 0; i < vertex_count * color_bytes; ++i) {
			if(member.colors[i] != 255) {
				return false;
			}
		}
	}

	return true;
}

} // namespace

/*
 */
struct NodNativeAssets {
		std::string root;
		std::vector<std::string> catalog;
		std::map<std::string, NativeFile> files;
		NativeFile *level_bundle = nullptr; // Borrows a stable map entry.
		std::string level_path;
};

/*
 */
namespace {

/*
 */
int index_native_file(void *context, const char *path, const char *) {
	auto *assets				 = static_cast<NodNativeAssets *>(context);
	const std::string normalized = virtual_path(path);
	if(!has_extension(normalized, ".nod") && !has_extension(normalized, ".nmb")) {
		return 1;
	}

	const std::string relative = normalized.substr(assets->root.size() + 1);
	if(relative.size() >= sizeof(NodBinding::asset)) {
		return native_error("native asset path too long");
	}

	for(const auto &entry : assets->catalog) {
		if(!ci(entry.c_str(), relative.c_str())) {
			return native_error("case-insensitive native asset path collision");
		}
	}

	assets->catalog.push_back(relative);
	return 1;
}

/*
 */
const std::string *find_asset(const NodNativeAssets &assets, const std::string &reference, size_t *matches,
							  bool nod_only = false) {
	const std::string *found = nullptr;
	*matches				 = 0;
	for(const auto &path : assets.catalog) {
		if(nod_only && !has_extension(path, ".nod")) {
			continue;
		}

		const std::string candidate =
			reference.find('/') == std::string::npos ? path.substr(path.find_last_of('/') + 1) : path;
		if(reference_matches(reference, candidate)) {
			found = &path;
			++*matches;
		}
	}

	return found;
}

/*
 */
NativeFile *load_native_file(NodNativeAssets *assets, const std::string &relative) {
	const auto cached = assets->files.find(relative);
	if(cached != assets->files.end()) {
		return &cached->second;
	}

	char path[2048] = {};
	Blob bytes		= {};
	if(!path_join(path, sizeof(path), assets->root.c_str(), relative.c_str()) || !load(path, &bytes)) {
		free(bytes.data);
		return nullptr;
	}

	NativeFile file;
	file.checksum = crc32(&bytes);
	bool parsed	  = false;
	if(has_extension(relative, ".nod")) {
		file.model = std::make_unique<NODParser>();
		parsed	   = file.model->parseBuffer(bytes.data, bytes.size) != 0;
		if(!parsed) {
			native_error(file.model->getLastError().c_str());
		}
	} else {
		file.bundle = std::make_unique<NMBParser>();
		parsed		= file.bundle->parseBuffer(bytes.data, bytes.size) != 0;
		if(!parsed) {
			native_error(file.bundle->getLastError().c_str());
		}
	}

	free(bytes.data);
	if(!parsed) {
		return nullptr;
	}

	return &assets->files.emplace(relative, std::move(file)).first->second;
}

/*
 */
NativeModel selected_model(const NativeFile &file, uint32_t frame) {
	if(file.model) {
		return model_view(file.model->getModelData(), nullptr);
	}

	const auto &entry = file.bundle->getBundleData().models[frame];
	return model_view(entry, entry.vertex_colors);
}

} // namespace

/*
 */
NodNativeAssets *nod_native_open(const char *root, const char *bundle) {
	auto assets	 = std::make_unique<NodNativeAssets>();
	assets->root = virtual_path(root);
	while(!assets->root.empty() && assets->root.back() == '/') {
		assets->root.pop_back();
	}

	char directory[2048] = {};
	if(!path_join(directory, sizeof(directory), root, "3D/Models") ||
	   !visit_files(directory, 1, 0, index_native_file, assets.get()) ||
	   !path_join(directory, sizeof(directory), root, "3D/Terrains") ||
	   !visit_files(directory, 1, 1, index_native_file, assets.get())) {
		return nullptr;
	}

	if(strcmp(bundle, "-")) {
		if(!safe_reference(bundle) || strchr(bundle, '/') || strchr(bundle, '\\') || !has_extension(bundle, ".nmb")) {
			native_error("level bundle must be an NMB filename from 3D/Models, or '-'");
			return nullptr;
		}

		size_t matches			= 0;
		const std::string *path = find_asset(*assets, std::string("3D/Models/") + bundle, &matches);
		if(matches != 1) {
			native_error("selected level bundle is missing or ambiguous");
			return nullptr;
		}

		assets->level_path	 = *path;
		assets->level_bundle = load_native_file(assets.get(), *path);
		if(!assets->level_bundle) {
			return nullptr;
		}
	}

	return assets.release();
}

/*
 */
void nod_native_close(NodNativeAssets *assets) {
	delete assets;
}

/*
 */
int nod_native_resolve(NodNativeAssets *assets, NodBinding *binding) {
	const std::string reference = virtual_path(binding->reference);
	if(!safe_reference(reference)) {
		strcpy(binding->status, "unsupported-reference-path");
		return 1;
	}

	uint32_t frame				  = 0;
	size_t member_matches		  = 0;
	const std::string member_name = reference.substr(reference.find_last_of('/') + 1);
	const bool is_flat_model_path =
		!ci(reference.substr(0, 10).c_str(), "3D/Models/") && reference.find('/', 10) == std::string::npos;
	if(assets->level_bundle && (reference.find('/') == std::string::npos || is_flat_model_path)) {
		const auto &bundle = assets->level_bundle->bundle->getBundleData();
		for(uint32_t i = 0; i < bundle.parsed_count; ++i) {
			if(reference_matches(member_name, bundle.models[i].name)) {
				frame = i;
				++member_matches;
			}
		}
	}

	if(member_matches > 1) {
		strcpy(binding->status, "ambiguous-bundle-member");
		return 1;
	}

	size_t matches			= 0;
	const std::string *path = find_asset(*assets, reference, &matches, member_matches == 1);
	NativeFile *file		= nullptr;
	std::string selected_path;
	if(member_matches == 1) {
		file		  = assets->level_bundle;
		selected_path = assets->level_path;
		strcpy(binding->member, file->bundle->getBundleData().models[frame].name);
		if(matches == 1) {
			NativeFile *standalone = load_native_file(assets, *path);
			if(!standalone) {
				return 0;
			}

			bool has_triangles = false;
			if(!validate_model(selected_model(*standalone, 0), &has_triangles) ||
			   !validate_model(selected_model(*file, frame), &has_triangles)) {
				return 0;
			}

			if(equivalent_render_data(selected_model(*standalone, 0), selected_model(*file, frame))) {
				file			   = standalone;
				selected_path	   = *path;
				frame			   = 0;
				binding->member[0] = '\0';
			}
		}
	} else {
		if(matches != 1) {
			strcpy(binding->status, matches ? "ambiguous-asset" : "missing-asset");
			return 1;
		}

		file = load_native_file(assets, *path);
		if(!file) {
			return 0;
		}

		selected_path = *path;
		if(file->bundle) {
			const auto &bundle = file->bundle->getBundleData();
			if(bundle.parsed_count != 1) {
				strcpy(binding->status, bundle.parsed_count ? "bundle-member-required" : "no-render-triangles");
				strcpy(binding->asset, path->c_str());
				binding->checksum = file->checksum;
				return 1;
			}

			strcpy(binding->member, bundle.models[0].name);
		}
	}

	bool has_triangles = false;
	if(!validate_model(selected_model(*file, frame), &has_triangles)) {
		return 0;
	}

	strcpy(binding->asset, selected_path.c_str());
	binding->checksum = file->checksum;
	binding->frame	  = frame;
	strcpy(binding->status, has_triangles ? "source-mesh" : "no-render-triangles");
	if(has_triangles) {
		strcpy(binding->preview, selected_path.c_str());
	}

	return 1;
}
