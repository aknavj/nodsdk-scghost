/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_assets.c
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

#include "nod_assets.h"
#include "nod_native.h"

#include "niltomap.h"
#include "nod_materials.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

/*
 */
enum {
	NATIVE_NAME_SIZE			   = 32,
	NATIVE_U32_SIZE				   = 4,
	MODEL_SET_LIMIT				   = 4,
	MODEL_SHADER_LIMIT			   = 256,
	MODEL_VERTEX_LIMIT			   = 500000,
	MODEL_INDEX_LIMIT			   = 2000000,
	MODEL_LOD_COUNT				   = 4,
	MODEL_LOD_SIZE				   = 6,
	MODEL_INDEX_SIZE			   = 2,
	NOD_HEADER_SIZE				   = 92,
	NOD_SHADER_COUNT_OFFSET		   = 4,
	NOD_BONE_COUNT_OFFSET		   = 5,
	NOD_SET_COUNT_OFFSET		   = 6,
	NOD_GROUP_COUNT_OFFSET		   = 7,
	NOD_VERTEX_TYPE_OFFSET		   = 36,
	NOD_VERTEX_COUNT_OFFSET		   = 40,
	NOD_VERTEX_DESCRIPTOR_SIZE	   = 8,
	NOD_INDEX_COUNT_OFFSET		   = 68,
	NOD_GROUP_SIZE				   = 56,
	NOD_GROUP_SET_OFFSET		   = 54,
	NOD_GROUP_PALETTE_OFFSET	   = 53,
	NOD_GROUP_LOD_OFFSET		   = 4,
	NOD_GROUP_ADVANCE_OFFSET	   = 28,
	NOD_BONE_SIZE				   = 64,
	NOD_TRAILING_RECORD_SIZE	   = 52,
	NMB_HEADER_SIZE				   = 12,
	NMB_COUNT_OFFSET			   = 4,
	NMB_MEMBER_HEADER_SIZE		   = 66,
	NMB_MEMBER_SHADER_COUNT_OFFSET = 0,
	NMB_MEMBER_BONE_COUNT_OFFSET   = 1,
	NMB_MEMBER_GROUP_COUNT_OFFSET  = 2,
	NMB_MEMBER_MARKER_COUNT_OFFSET = 3,
	NMB_MEMBER_NAME_OFFSET		   = 5,
	NMB_MEMBER_SET_COUNT_OFFSET	   = 37,
	NMB_VERTEX_HEADER_SIZE		   = 5,
	NMB_VERTEX_COUNT_OFFSET		   = 1,
	NMB_GROUP_SIZE				   = 33,
	NMB_GROUP_SET_OFFSET		   = 4,
	NMB_GROUP_PALETTE_OFFSET	   = 32,
	NMB_GROUP_LOD_OFFSET		   = 5,
	NMB_GROUP_ADVANCE_OFFSET	   = 9,
	NMB_BONE_SIZE				   = 26,
	NMB_EXTRA_RECORD_SIZE		   = 24,
	NMB_MARKER_SIZE				   = 68,
	NMB_MEMBER_LIMIT			   = 256,
	NMB_SHADER_LIMIT			   = 64,
	MODEL_PALETTE_LIMIT			   = 20,
	MODEL_COLOR_SIZE			   = 4,
	MODEL_VERTEX_ATTRIBUTE_COUNT   = 8,
	MODEL_ANIMATED_TYPE_FIRST	   = 3,
	NATIVE_FLOAT_SIZE			   = 4,
	TGA_HEADER_SIZE				   = 18,
	TGA_PIXEL_SIZE				   = 3,
	TGA_TYPE_OFFSET				   = 2,
	TGA_WIDTH_OFFSET			   = 12,
	TGA_HEIGHT_OFFSET			   = 14,
	TGA_BITS_OFFSET				   = 16,
	TGA_TYPE_TRUE_COLOR			   = 2,
	TGA_BITS_RGB				   = 24
};

/*
 */
static const double radians_per_degree	  = 0.017453292519943295;
static const double gimbal_lock_threshold = 1e-10;

/*
 */
typedef struct {
		const Blob *blob; // Borrows the owning AssetFile's raw buffer.
		size_t offset;
} Reader;

/*
 */
typedef struct {
		const unsigned char *vertices; // Borrows bytes from AssetFile.raw.
		uint32_t count;
		uint32_t stride;
		uint32_t type;
} VertexSet;

/*
 */
typedef struct {
		uint32_t shader;
		uint32_t set;
		uint32_t base;
		uint32_t strip_start;
		uint32_t strip_count;
		uint32_t list_start;
		uint32_t list_count;
} Group;

/*
 */
typedef struct {
		char name[NATIVE_NAME_SIZE + 1];
		VertexSet sets[MODEL_SET_LIMIT];
		uint32_t set_count;
		uint32_t index_count;
		uint32_t group_count;
		const unsigned char *indices; // Borrows bytes from AssetFile.raw.
		const unsigned char *shaders; // Borrows bytes from AssetFile.raw.
		Group *groups;				  // Owned by the containing AssetFile.
		char preview[256];
		int preview_known;
} Mesh;

/*
 */
typedef struct AssetFile {
		char name[256];
		Blob raw;	  // Owns the storage borrowed by mesh streams.
		Mesh *meshes; // Owns the array and each mesh's groups.
		uint32_t count;
		uint32_t checksum;
		char preview[256];
		int preview_known;
		struct AssetFile *next;
} AssetFile;

/*
 */
struct NodAssets {
		char root[1024];
		char preview_root[1024];
		NodTemplates *templates;
		char **catalog; // Owns the array and each filename.
		size_t catalog_count;
		AssetFile *files;		 // Owns the list, including all mesh and raw storage.
		AssetFile *bundle;		 // Borrows one entry in files.
		NodMaterials *materials; // Owns the material catalog.
		NodNativeAssets *native; // Owns shared-parser native model bindings.
};

/*
 */
static int fail(const char *message) {
	fprintf(stderr, "NODscene: assets: %s\n", message);
	return 0;
}

/*
 */
static const unsigned char *read_records(Reader *reader, const size_t count, const size_t stride) {
	if(count > (reader->blob->size - reader->offset) / stride) {
		fprintf(stderr, "NODscene: assets: truncated model at byte %zu\n", reader->offset);
		return NULL;
	}

	const unsigned char *records = reader->blob->data + reader->offset;
	reader->offset += count * stride;
	return records;
}

/*
 */
static int read_native_name(const unsigned char *bytes, char name[NATIVE_NAME_SIZE + 1]) {
	size_t length = 0;
	for(; length < NATIVE_NAME_SIZE && bytes[length]; ++length) {
		if(bytes[length] < 32 || bytes[length] > 126) {
			return fail("non-ASCII native name");
		}

		name[length] = (char)bytes[length];
	}

	name[length] = '\0';
	return 1;
}

/*
 */
static int has_extension(const char *name, const char *suffix) {
	const size_t name_length   = strlen(name);
	const size_t suffix_length = strlen(suffix);
	return name_length >= suffix_length && !ci(name + name_length - suffix_length, suffix);
}

/*
 */
static int reference_match(const char *reference, const char *name) {
	const size_t name_length = strlen(name);
	if(has_extension(reference, ".nod") || has_extension(reference, ".nmb")) {
		return !ci(reference, name);
	}

	char stem[256] = {0};
	if(name_length < 4 || name_length >= sizeof(stem) ||
	   !(has_extension(name, ".nod") || has_extension(name, ".nmb"))) {
		return 0;
	}

	memcpy(stem, name, name_length - 4);
	stem[name_length - 4] = '\0';
	return !ci(reference, stem);
}

/*
 */
static int catalog_add(NodAssets *assets, const char *name) {
	if(!(has_extension(name, ".nod") || has_extension(name, ".nmb"))) {
		return 1;
	}

	if(strlen(name) >= 256 || assets->catalog_count >= SIZE_MAX / sizeof(*assets->catalog) - 1) {
		return fail("invalid model catalog name/count");
	}

	for(size_t i = 0; i < assets->catalog_count; ++i) {
		if(!ci(assets->catalog[i], name)) {
			return fail("case-insensitive model filename collision");
		}
	}

	char *copy = malloc(strlen(name) + 1);
	if(!copy) {
		return fail("out of memory indexing model names");
	}

	strcpy(copy, name);
	char **list = realloc(assets->catalog, (assets->catalog_count + 1) * sizeof(*list));
	if(!list) {
		free(copy);
		return fail("out of memory indexing model names");
	}

	assets->catalog				  = list;
	list[assets->catalog_count++] = copy;
	return 1;
}

/*
 */
static int catalog_read(NodAssets *assets) {
	char directory[1024] = {0};
	int is_successful	 = 1;
	if(!path_join(directory, sizeof(directory), assets->root, "3D/Models")) {
		return 0;
	}
#ifdef _WIN32
	{
		WIN32_FIND_DATAA entry = {0};
		char pattern[1100]	   = {0};
		if(!path_join(pattern, sizeof(pattern), directory, "*")) {
			return 0;
		}

		const HANDLE search = FindFirstFileA(pattern, &entry);
		if(search == INVALID_HANDLE_VALUE) {
			return fail("cannot enumerate 3D/Models");
		}

		do {
			if(!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !catalog_add(assets, entry.cFileName)) {
				is_successful = 0;
				break;
			}
		} while(FindNextFileA(search, &entry));
		if(is_successful && GetLastError() != ERROR_NO_MORE_FILES) {
			is_successful = fail("model directory enumeration failed");
		}

		if(!FindClose(search)) {
			is_successful = fail("cannot close directory enumeration");
		}
	}
#else
	{
		DIR *directory_stream = opendir(directory);
		struct dirent *entry  = NULL;
		if(!directory_stream) {
			return fail("cannot enumerate 3D/Models");
		}

		errno = 0;
		while((entry = readdir(directory_stream)) != NULL) {
			char path[1100]			= {0};
			struct stat file_status = {0};
			if(!path_join(path, sizeof(path), directory, entry->d_name) || stat(path, &file_status)) {
				is_successful = fail("cannot inspect model catalog entry");
				break;
			}

			if(S_ISREG(file_status.st_mode) && !catalog_add(assets, entry->d_name)) {
				is_successful = 0;
				break;
			}

			errno = 0;
		}

		if(is_successful && errno) {
			is_successful = fail("model directory enumeration failed");
		}

		if(closedir(directory_stream)) {
			is_successful = fail("cannot close model directory");
		}
	}
#endif
	return is_successful;
}

/******************************************************************************\
 * Native template and model parsing
\******************************************************************************/

/*
 */
static int templates_read(NodAssets *assets, const char *const *archives, size_t count) {
	assets->templates = nod_templates_open(assets->root, archives, count);
	return assets->templates != NULL;
}

/*
 */
static int read_vertex_set(Reader *reader, VertexSet *set, const uint32_t type, const uint32_t count) {
	static const uint32_t native_vertex_strides[] = {32, 36, 48, 68, 72, 84};
	if(type >= sizeof(native_vertex_strides) / sizeof(*native_vertex_strides) || count > MODEL_VERTEX_LIMIT) {
		return fail("unsupported vertex type/count");
	}

	set->type  = type;
	set->count = count;
	// Preserve the preview's static stride for animated type counterparts.
	set->stride	  = native_vertex_strides[type >= MODEL_ANIMATED_TYPE_FIRST ? type - MODEL_ANIMATED_TYPE_FIRST : type];
	set->vertices = read_records(reader, count, native_vertex_strides[type]);
	return set->vertices != NULL;
}

/*
 */
static int groups_read(Reader *reader, Mesh *mesh, const int is_bundle) {
	uint32_t index_cursor				   = 0;
	uint32_t vertex_bases[MODEL_SET_LIMIT] = {0};
	mesh->groups						   = calloc(mesh->group_count ? mesh->group_count : 1, sizeof(*mesh->groups));
	if(!mesh->groups) {
		return fail("out of memory reading mesh groups");
	}

	for(uint32_t group_index = 0; group_index < mesh->group_count; ++group_index) {
		const unsigned char *record = read_records(reader, 1, is_bundle ? NMB_GROUP_SIZE : NOD_GROUP_SIZE);
		Group *group				= &mesh->groups[group_index];
		if(!record) {
			return 0;
		}

		group->shader				 = u32(record);
		group->set					 = is_bundle ? record[NMB_GROUP_SET_OFFSET] : record[NOD_GROUP_SET_OFFSET];
		const uint32_t palette_count = is_bundle ? record[NMB_GROUP_PALETTE_OFFSET] : record[NOD_GROUP_PALETTE_OFFSET];
		if(group->shader >= (uint32_t)(mesh->shaders ? MODEL_SHADER_LIMIT : 0) || group->set >= mesh->set_count ||
		   palette_count > MODEL_PALETTE_LIMIT) {
			return fail("invalid model shader/set/palette");
		}

		if(is_bundle && !read_records(reader, palette_count, 1)) {
			return 0;
		}

		for(uint32_t lod = 0; lod < MODEL_LOD_COUNT; ++lod) {
			const size_t lod_offset = (is_bundle ? NMB_GROUP_LOD_OFFSET : NOD_GROUP_LOD_OFFSET) + lod * MODEL_LOD_SIZE;
			const uint32_t strip_count = u16(record + lod_offset);
			const uint32_t list_count  = u16(record + lod_offset + MODEL_INDEX_SIZE);
			if(index_cursor > mesh->index_count || strip_count + list_count > mesh->index_count - index_cursor ||
			   list_count % 3) {
				return fail("invalid model LOD index range");
			}

			if(!lod) {
				group->strip_start = index_cursor;
				group->strip_count = strip_count;
				group->list_start  = index_cursor + strip_count;
				group->list_count  = list_count;
			}

			index_cursor += strip_count + list_count;
		}

		const uint32_t vertex_advance = u16(record + (is_bundle ? NMB_GROUP_ADVANCE_OFFSET : NOD_GROUP_ADVANCE_OFFSET));
		group->base					  = vertex_bases[group->set];
		if(vertex_advance > mesh->sets[group->set].count - vertex_bases[group->set]) {
			return fail("model base vertex advance outside vertex set");
		}

		vertex_bases[group->set] += vertex_advance;
	}

	return 1;
}

/*
 */
static int nod_read(AssetFile *file) {
	Reader reader				= {&file->raw, 0};
	const unsigned char *header = read_records(&reader, 1, NOD_HEADER_SIZE);
	if(!header) {
		return 0;
	}

	file->count	 = 1;
	file->meshes = calloc(1, sizeof(*file->meshes));
	if(!file->meshes) {
		return fail("out of memory reading NOD");
	}

	Mesh *mesh					= file->meshes;
	const uint32_t shader_count = header[NOD_SHADER_COUNT_OFFSET];
	const uint32_t bone_count	= header[NOD_BONE_COUNT_OFFSET];
	mesh->set_count				= header[NOD_SET_COUNT_OFFSET];
	mesh->group_count			= header[NOD_GROUP_COUNT_OFFSET];
	mesh->index_count			= u32(header + NOD_INDEX_COUNT_OFFSET);
	if(mesh->set_count > MODEL_SET_LIMIT || mesh->index_count > MODEL_INDEX_LIMIT) {
		return fail("invalid NOD counts");
	}

	if(!(mesh->shaders = read_records(&reader, shader_count, NATIVE_NAME_SIZE)) ||
	   !read_records(&reader, bone_count, NOD_BONE_SIZE)) {
		return 0;
	}

	for(uint32_t i = 0; i < mesh->set_count; ++i) {
		if(!read_vertex_set(&reader, &mesh->sets[i],
							u32(header + NOD_VERTEX_TYPE_OFFSET + i * NOD_VERTEX_DESCRIPTOR_SIZE),
							u32(header + NOD_VERTEX_COUNT_OFFSET + i * NOD_VERTEX_DESCRIPTOR_SIZE))) {
			return 0;
		}
	}

	if(!(mesh->indices = read_records(&reader, mesh->index_count, MODEL_INDEX_SIZE)) ||
	   !groups_read(&reader, mesh, 0)) {
		return 0;
	}

	for(uint32_t i = 0; i < mesh->group_count; ++i) {
		if(mesh->groups[i].shader >= shader_count) {
			return fail("NOD shader index outside name array");
		}
	}

	if(reader.offset != file->raw.size) {
		const unsigned char *trailing_count = read_records(&reader, 1, NATIVE_U32_SIZE);
		if(!trailing_count || !read_records(&reader, u32(trailing_count), NOD_TRAILING_RECORD_SIZE)) {
			return 0;
		}
	}

	if(reader.offset != file->raw.size) {
		fprintf(stderr, "NODscene: warning: NOD preview ignores %zu trailing non-mesh bytes in '%s'\n",
				file->raw.size - reader.offset, file->name);
	}

	return 1;
}

/*
 */
static int nmb_read(AssetFile *file) {
	Reader reader				= {&file->raw, 0};
	const unsigned char *record = read_records(&reader, 1, NMB_HEADER_SIZE);
	if(!record) {
		return 0;
	}

	const uint32_t version = u32(record);
	file->count			   = u32(record + NMB_COUNT_OFFSET);
	if((version != 1 && version != 2) || file->count > NMB_MEMBER_LIMIT) {
		return fail("unsupported NMB version/count");
	}

	file->meshes = calloc(file->count ? file->count : 1, sizeof(*file->meshes));
	if(!file->meshes) {
		return fail("out of memory reading NMB");
	}

	for(uint32_t i = 0; i < file->count; ++i) {
		Mesh *mesh = &file->meshes[i];
		if(!(record = read_records(&reader, 1, NMB_MEMBER_HEADER_SIZE))) {
			return 0;
		}

		const uint32_t shader_count = record[NMB_MEMBER_SHADER_COUNT_OFFSET];
		const uint32_t bone_count	= record[NMB_MEMBER_BONE_COUNT_OFFSET];
		mesh->group_count			= record[NMB_MEMBER_GROUP_COUNT_OFFSET];
		const uint32_t marker_count = u16(record + NMB_MEMBER_MARKER_COUNT_OFFSET);
		mesh->set_count				= record[NMB_MEMBER_SET_COUNT_OFFSET];
		if(!read_native_name(record + NMB_MEMBER_NAME_OFFSET, mesh->name) || !*mesh->name || !mesh->set_count ||
		   mesh->set_count > MODEL_SET_LIMIT || shader_count > NMB_SHADER_LIMIT) {
			return fail("invalid NMB entry header");
		}

		for(uint32_t j = 0; j < i; ++j) {
			if(!ci(mesh->name, file->meshes[j].name)) {
				return fail("duplicate NMB member name");
			}
		}

		uint32_t total_vertices = 0;
		for(uint32_t j = 0; j < mesh->set_count; ++j) {
			if(!(record = read_records(&reader, 1, NMB_VERTEX_HEADER_SIZE)) ||
			   !read_vertex_set(&reader, &mesh->sets[j], record[0], u32(record + NMB_VERTEX_COUNT_OFFSET))) {
				return 0;
			}

			total_vertices += mesh->sets[j].count;
		}

		if(!(record = read_records(&reader, 1, NATIVE_U32_SIZE))) {
			return 0;
		}

		mesh->index_count = u32(record);
		if(mesh->index_count > MODEL_INDEX_LIMIT) {
			return fail("invalid NMB index count");
		}

		if(!(mesh->indices = read_records(&reader, mesh->index_count, MODEL_INDEX_SIZE)) ||
		   !(mesh->shaders = read_records(&reader, shader_count, NATIVE_NAME_SIZE)) ||
		   !read_records(&reader, bone_count, NMB_BONE_SIZE) || !(record = read_records(&reader, 1, NATIVE_U32_SIZE)) ||
		   !read_records(&reader, u32(record), NMB_EXTRA_RECORD_SIZE) || !groups_read(&reader, mesh, 1) ||
		   !read_records(&reader, marker_count, NMB_MARKER_SIZE)) {
			return 0;
		}

		for(uint32_t j = 0; j < mesh->group_count; ++j) {
			if(mesh->groups[j].shader >= shader_count) {
				return fail("NMB shader index outside name array");
			}
		}

		if(version == 2) {
			if(!(record = read_records(&reader, 1, 1))) {
				return 0;
			}

			if(record[0] > 1) {
				return fail("invalid NMB color-stream flag");
			}

			if(record[0] && !read_records(&reader, total_vertices, MODEL_COLOR_SIZE)) {
				return 0;
			}
		}
	}

	if(reader.offset != file->raw.size) {
		fprintf(stderr, "NODscene: warning: NMB preview ignores %zu trailing bytes in '%s'\n",
				file->raw.size - reader.offset, file->name);
	}

	return 1;
}

/*
 */
static AssetFile *asset_load(NodAssets *assets, const char *name) {
	for(AssetFile *file = assets->files; file; file = file->next) {
		if(!ci(file->name, name)) {
			return file;
		}
	}

	AssetFile *file = calloc(1, sizeof(*file));
	if(!file) {
		fail("out of memory loading model");
		return NULL;
	}

	if(strlen(name) >= sizeof(file->name)) {
		free(file);
		fail("asset filename too long");
		return NULL;
	}

	strcpy(file->name, name);
	// Link before parsing so close() also owns partially loaded mesh data.
	file->next			 = assets->files;
	assets->files		 = file;
	char directory[1100] = {0};
	char path[1400]		 = {0};
	if(!path_join(directory, sizeof(directory), assets->root, "3D/Models") ||
	   !path_join(path, sizeof(path), directory, name) || !load(path, &file->raw)) {
		return NULL;
	}

	file->checksum = crc32(&file->raw);
	if(!(has_extension(name, ".nmb") ? nmb_read(file) : nod_read(file))) {
		return NULL;
	}

	return file;
}

/******************************************************************************\
 * Source mesh preview export
\******************************************************************************/

/*
 */
static int make_directory(const char *path) {
#ifdef _WIN32
	if(_mkdir(path) == 0) {
		return 1;
	}

	if(errno == EEXIST) {
		const DWORD attributes = GetFileAttributesA(path);
		if(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
			return 1;
		}
	}
#else
	struct stat file_status = {0};
	if(mkdir(path, 0777) == 0) {
		return 1;
	}

	if(errno == EEXIST && !stat(path, &file_status) && S_ISDIR(file_status.st_mode)) {
		return 1;
	}
#endif
	fprintf(stderr, "NODscene: cannot create preview directory '%s': %s\n", path, strerror(errno));
	return 0;
}

/*
 */
static int staging_directories(const NodAssets *assets) {
	char path[1400]						   = {0};
	static const char *const directories[] = {"models", "models/nodpreview", "textures", "textures/nodpreview"};
	if(!make_directory(assets->preview_root)) {
		return 0;
	}

	const size_t count = assets->native ? 2 : sizeof(directories) / sizeof(*directories);
	for(size_t i = 0; i < count; ++i) {
		if(!path_join(path, sizeof(path), assets->preview_root, directories[i]) || !make_directory(path)) {
			return 0;
		}
	}

	return 1;
}

/*
 */
static int write_obj_face(FILE *stream, const Mesh *mesh, const Group *group, const uint32_t base, const uint16_t first,
						  const uint16_t second, const uint16_t third, size_t *face_count) {
	const uint32_t indices[3] = {first, second, third};
	const VertexSet *set	  = &mesh->sets[group->set];
	for(size_t i = 0; i < 3; ++i) {
		if(group->base > set->count || indices[i] >= set->count - group->base) {
			return fail("model triangle index outside its vertex set");
		}
	}

	if(first == second || second == third || first == third) {
		return 1;
	}

	if(fprintf(stream, "f %u/%u/%u %u/%u/%u %u/%u/%u\n", (unsigned)(base + group->base + first),
			   (unsigned)(base + group->base + first), (unsigned)(base + group->base + first),
			   (unsigned)(base + group->base + second), (unsigned)(base + group->base + second),
			   (unsigned)(base + group->base + second), (unsigned)(base + group->base + third),
			   (unsigned)(base + group->base + third), (unsigned)(base + group->base + third)) < 0) {
		return fail("cannot write OBJ face");
	}

	++*face_count;
	return 1;
}

/*
 */
static int write_obj_vertices(FILE *stream, const Mesh *mesh, uint32_t *cursor, uint32_t bases[MODEL_SET_LIMIT]) {
	for(uint32_t i = 0; i < mesh->set_count; ++i) {
		const VertexSet *set = &mesh->sets[i];
		bases[i]			 = *cursor;
		for(uint32_t j = 0; j < set->count; ++j) {
			const unsigned char *vertex					   = set->vertices + (size_t)j * set->stride;
			float attributes[MODEL_VERTEX_ATTRIBUTE_COUNT] = {0};
			for(size_t k = 0; k < MODEL_VERTEX_ATTRIBUTE_COUNT; ++k) {
				attributes[k] = f32(vertex + k * NATIVE_FLOAT_SIZE);
				if(!isfinite(attributes[k])) {
					return fail("non-finite model vertex attribute");
				}
			}

			if(fprintf(stream, "v %.9g %.9g %.9g\nvt %.9g %.9g\nvn %.9g %.9g %.9g\n", (double)attributes[0],
					   (double)attributes[1], (double)attributes[2], (double)attributes[6],
					   attributes[7] ? -(double)attributes[7] : 0.0, (double)attributes[3], (double)attributes[4],
					   (double)attributes[5]) < 0) {
				return fail("cannot write OBJ vertex");
			}
		}

		*cursor += set->count;
	}

	return 1;
}

/*
 */
static int write_obj_mesh(FILE *stream, const Mesh *mesh, uint32_t *cursor, size_t *face_count,
						  NodMaterialOutput *materials) {
	uint32_t bases[MODEL_SET_LIMIT] = {0};
	if(!write_obj_vertices(stream, mesh, cursor, bases)) {
		return 0;
	}

	for(uint32_t i = 0; i < mesh->group_count; ++i) {
		const Group *group				  = &mesh->groups[i];
		char shader[NATIVE_NAME_SIZE + 1] = {0};
		if(!read_native_name(mesh->shaders + (size_t)group->shader * NATIVE_NAME_SIZE, shader)) {
			return 0;
		}

		const char *material = nod_materials_use(materials, shader);
		if(!material || fprintf(stream, "# native_shader %s\ng surface_%u_%u\nusemtl %s\n", shader,
								(unsigned)bases[group->set], (unsigned)i, material) < 0) {
			return fail("cannot write OBJ group");
		}

		uint16_t previous_first	 = 0;
		uint16_t previous_second = 0;
		uint32_t strip_run		 = 0;
		for(uint32_t j = 0; j < group->strip_count; ++j) {
			const uint16_t index = u16(mesh->indices + ((size_t)group->strip_start + j) * MODEL_INDEX_SIZE);
			if(index == UINT16_MAX) {
				strip_run = 0;
				continue;
			}

			if(group->base > mesh->sets[group->set].count || index >= mesh->sets[group->set].count - group->base) {
				return fail("model strip index outside its vertex set");
			}

			if(strip_run >= 2 &&
			   !write_obj_face(stream, mesh, group, bases[group->set],
							   (strip_run & 1) ? previous_second : previous_first,
							   (strip_run & 1) ? previous_first : previous_second, index, face_count)) {
				return 0;
			}

			previous_first	= previous_second;
			previous_second = index;
			++strip_run;
		}

		for(uint32_t j = 0; j < group->list_count; j += 3) {
			const unsigned char *indices = mesh->indices + ((size_t)group->list_start + j) * MODEL_INDEX_SIZE;
			if(!write_obj_face(stream, mesh, group, bases[group->set], u16(indices), u16(indices + MODEL_INDEX_SIZE),
							   u16(indices + 2 * MODEL_INDEX_SIZE), face_count)) {
				return 0;
			}
		}
	}

	return 1;
}

/*
 */
static int publish_model(const NodAssets *assets, FILE *body, FILE *mtl, const char *stem, char preview[256]) {
	Blob object		   = {0};
	Blob material	   = {0};
	char relative[256] = {0};
	char path[1400]	   = {0};
	char filename[256] = {0};
	FILE *complete	   = NULL;
	int is_successful  = 0;
	if(!read_stream(body, &object) || !read_stream(mtl, &material)) {
		goto cleanup;
	}

	const int filename_length = snprintf(filename, sizeof(filename), "%s-d%08x.obj", stem, (unsigned)crc32(&material));
	if(filename_length < 0 || (size_t)filename_length >= sizeof(filename)) {
		fail("preview filename is too long");
		goto cleanup;
	}

	const int relative_length = snprintf(relative, sizeof(relative), "models/nodpreview/%s", filename);
	if(relative_length < 0 || (size_t)relative_length >= sizeof(relative)) {
		fail("preview path is too long");
		goto cleanup;
	}

	complete = tmpfile();
	if(!complete) {
		fail("cannot create complete OBJ");
		goto cleanup;
	}

	if(fprintf(complete, "mtllib %.*s.mtl\n", (int)strlen(filename) - 4, filename) < 0 ||
	   fwrite(object.data, 1, object.size, complete) != object.size) {
		fail("cannot write complete OBJ");
		goto cleanup;
	}

	free(object.data);
	object.data = NULL;
	if(!read_stream(complete, &object) || !path_join(path, sizeof(path), assets->preview_root, relative) ||
	   !save_generated(path, &object)) {
		goto cleanup;
	}

	memcpy(path + strlen(path) - 3, "mtl", 3);
	if(!save_generated(path, &material)) {
		goto cleanup;
	}

	strcpy(preview, relative);
	is_successful = 1;
cleanup:
	free(object.data);
	free(material.data);
	if(complete && fclose(complete)) {
		is_successful = fail("cannot close complete OBJ");
	}

	return is_successful;
}

/*
 */
static int encode_preview_name(const char *name, char *safe_name, const size_t capacity) {
	size_t safe_length = 0;
	for(size_t j = 0; name[j]; ++j) {
		const unsigned char character = (unsigned char)name[j];
		if(safe_length + 4 >= capacity) {
			return fail("preview filename is too long");
		}

		if(isalnum(character) || character == '.' || character == '_' || character == '-') {
			safe_name[safe_length++] = (char)character;
		} else {
			snprintf(safe_name + safe_length, capacity - safe_length, "~%02x", (unsigned)character);
			safe_length += 3;
		}
	}

	safe_name[safe_length] = '\0';
	return 1;
}

/*
 */
static int preview_write(NodAssets *assets, AssetFile *file, const uint32_t member, NodBinding *binding) {
	char stem[256]				= {0};
	char safe_name[128]			= {0};
	FILE *object_stream			= NULL;
	FILE *material_stream		= NULL;
	NodMaterialOutput materials = {assets->materials, NULL, NULL, 0};
	const uint32_t first		= member == UINT32_MAX ? 0 : member;
	const uint32_t end			= member == UINT32_MAX ? file->count : member + 1;
	int is_successful			= 0;
	if(!file->count && member == UINT32_MAX) {
		strcpy(binding->status, "no-render-triangles");
		return 1;
	}

	if(first >= file->count || end > file->count) {
		return fail("preview member outside bundle");
	}

	char *cached_preview  = member == UINT32_MAX ? file->preview : file->meshes[member].preview;
	int *is_preview_known = member == UINT32_MAX ? &file->preview_known : &file->meshes[member].preview_known;
	if(*is_preview_known) {
		strcpy(binding->preview, cached_preview);
		strcpy(binding->status, *cached_preview ? "source-mesh" : "no-render-triangles");
		return 1;
	}

	if(!encode_preview_name(file->name, safe_name, sizeof(safe_name))) {
		return 0;
	}

	snprintf(stem, sizeof(stem), "%s-%08x-%u", safe_name, (unsigned)file->checksum, (unsigned)member);
	object_stream	= tmpfile();
	material_stream = tmpfile();
	if(!object_stream || !material_stream) {
		fail("cannot create temporary OBJ/MTL");
		goto cleanup;
	}

	materials.mtl = material_stream;
	if(fputs("# Generated by NODscene; native XYZ, PicoModel-compatible UVs\n", object_stream) == EOF) {
		goto cleanup;
	}

	uint32_t vertex_cursor = 1;
	size_t face_count	   = 0;
	for(uint32_t i = first; i < end; ++i) {
		if(!write_obj_mesh(object_stream, &file->meshes[i], &vertex_cursor, &face_count, &materials)) {
			goto cleanup;
		}
	}

	if(!face_count) {
		strcpy(binding->status, "no-render-triangles");
		*is_preview_known = 1;
		is_successful	  = 1;
		goto cleanup;
	}

	if(!publish_model(assets, object_stream, material_stream, stem, binding->preview)) {
		goto cleanup;
	}

	strcpy(binding->status, "source-mesh");
	strcpy(cached_preview, binding->preview);
	*is_preview_known = 1;
	is_successful	  = 1;
cleanup:
	if(!is_successful) {
		fail("model preview export failed");
	}

	nod_materials_output_close(&materials);
	if(object_stream && fclose(object_stream)) {
		fail("cannot close temporary OBJ");
		is_successful = 0;
	}

	if(material_stream && fclose(material_stream)) {
		is_successful = fail("cannot close temporary MTL");
	}

	return is_successful;
}

/******************************************************************************\
 * Public asset index and bindings
\******************************************************************************/

/*
 */
typedef struct {
		FILE *stream; // Borrows the temporary material stream.
		char name[256];
} NativeWorldMaterial;

/*
 */
static const char *native_world_material(void *context, const char *shader) {
	NativeWorldMaterial *material = context;
	const int length			  = snprintf(material->name, sizeof(material->name), "textures/nod/%s", shader);
	if(length < 0 || (size_t)length >= sizeof(material->name)) {
		fail("native material name is too long");
		return NULL;
	}

	if(fprintf(material->stream, "newmtl %s\nKd 1 1 1\n", material->name) < 0) {
		fail("cannot write native material binding");
		return NULL;
	}

	return material->name;
}

/*
 */
int nod_assets_nil_preview(NodAssets *assets, const Blob *nil, NodBinding *binding) {
	char stem[64]						= {0};
	FILE *object_stream					= tmpfile();
	FILE *material_stream				= tmpfile();
	NodMaterialOutput materials			= {assets->materials, material_stream, NULL, 0};
	NativeWorldMaterial native_material = {material_stream, {0}};
	size_t triangle_count				= 0;
	int is_successful					= 0;
	memset(binding, 0, sizeof(*binding));
	binding->model_scale = 1;
	strcpy(binding->reference, "NIL render geometry");
	strcpy(binding->asset, "source:nil");
	strcpy(binding->provenance, "nil:render-sectors");
	binding->checksum = crc32(nil);
	snprintf(stem, sizeof(stem), "nil-%08x", (unsigned)binding->checksum);
	if(!object_stream || !material_stream) {
		fail("cannot create temporary NIL OBJ/MTL");
		goto cleanup;
	}

	if(!niltomap_write_obj(nil->data, nil->size, object_stream, &triangle_count,
						   assets->native ? native_world_material : nod_materials_use,
						   assets->native ? (void *)&native_material : (void *)&materials)) {
		goto cleanup;
	}

	if(!triangle_count) {
		strcpy(binding->status, "no-render-triangles");
		is_successful = 1;
		goto cleanup;
	}

	if(!publish_model(assets, object_stream, material_stream, stem, binding->preview)) {
		goto cleanup;
	}

	strcpy(binding->status, "source-mesh");
	printf("NODscene: NIL world preview contains %zu source triangles, no brushes\n", triangle_count);
	is_successful = 1;
cleanup:
	if(!is_successful) {
		fail("NIL model preview export failed");
	}

	nod_materials_output_close(&materials);
	if(object_stream && fclose(object_stream)) {
		is_successful = fail("cannot close temporary NIL model");
	}

	if(material_stream && fclose(material_stream)) {
		is_successful = fail("cannot close temporary NIL MTL");
	}

	return is_successful;
}

/*
 */
static NodAssets *assets_open(const char *root, const char *bundle, const char *preview_root, int native_models,
							  const char *const *archives, size_t count) {
	NodAssets *assets									  = calloc(1, sizeof(*assets));
	char white_path[1400]								  = {0};
	unsigned char white[TGA_HEADER_SIZE + TGA_PIXEL_SIZE] = {0};
	const Blob image									  = {white, sizeof(white)};
	if(!assets) {
		fail("out of memory creating asset index");
		return NULL;
	}

	if(strlen(root) >= sizeof(assets->root) || strlen(preview_root) >= sizeof(assets->preview_root)) {
		fail("asset/staging path too long");
		goto failed;
	}

	strcpy(assets->root, root);
	strcpy(assets->preview_root, preview_root);
	if(!templates_read(assets, archives, count)) {
		goto failed;
	}

	if(native_models) {
		assets->native = nod_native_open(root, bundle);
		if(!assets->native || !staging_directories(assets)) {
			goto failed;
		}

		return assets;
	}

	if(!catalog_read(assets)) {
		goto failed;
	}

	if(strcmp(bundle, "-")) {
		const char *name = NULL;
		if(strchr(bundle, '/') || strchr(bundle, '\\') || !has_extension(bundle, ".nmb")) {
			fail("level bundle must be an NMB filename from 3D/Models, or '-'");
			goto failed;
		}

		for(size_t i = 0; i < assets->catalog_count; ++i) {
			if(!ci(assets->catalog[i], bundle)) {
				name = assets->catalog[i];
				break;
			}
		}

		if(!name) {
			fail("selected level bundle is missing from 3D/Models");
			goto failed;
		}

		assets->bundle = asset_load(assets, name);
		if(!assets->bundle) {
			goto failed;
		}
	}

	if(!staging_directories(assets) ||
	   !path_join(white_path, sizeof(white_path), preview_root, "textures/nodpreview/white.tga")) {
		goto failed;
	}

	white[TGA_TYPE_OFFSET]	 = TGA_TYPE_TRUE_COLOR;
	white[TGA_WIDTH_OFFSET]	 = 1;
	white[TGA_HEIGHT_OFFSET] = 1;
	white[TGA_BITS_OFFSET]	 = TGA_BITS_RGB;
	white[TGA_HEADER_SIZE] = white[TGA_HEADER_SIZE + 1] = white[TGA_HEADER_SIZE + 2] = 255;
	if(!save_generated(white_path, &image)) {
		goto failed;
	}

	assets->materials = nod_materials_open(root, assets->bundle ? assets->bundle->name : "-", preview_root);
	if(!assets->materials) {
		goto failed;
	}

	return assets;
failed:
	nod_assets_close(assets);
	return NULL;
}

/*
 */
NodAssets *nod_assets_open(const char *root, const char *bundle, const char *preview_root, const char *const *archives,
						   size_t count) {
	return assets_open(root, bundle, preview_root, 0, archives, count);
}

/*
 */
NodAssets *nod_assets_open_native(const char *root, const char *bundle, const char *preview_root,
								  const char *const *archives, size_t count) {
	return assets_open(root, bundle, preview_root, 1, archives, count);
}

/*
 */
void nod_assets_close(NodAssets *assets) {
	if(!assets) {
		return;
	}

	nod_materials_close(assets->materials);
	nod_native_close(assets->native);
	while(assets->files) {
		AssetFile *file = assets->files;
		assets->files	= file->next;
		for(uint32_t j = 0; j < file->count && file->meshes; ++j) {
			free(file->meshes[j].groups);
		}

		free(file->meshes);
		free(file->raw.data);
		free(file);
	}

	for(size_t i = 0; i < assets->catalog_count; ++i) {
		free(assets->catalog[i]);
	}

	free(assets->catalog);
	nod_templates_close(assets->templates);
	free(assets);
}

/*
 */
int nod_assets_resolve(NodAssets *assets, const char *type, const char *reference, const unsigned char *modifiers,
					   size_t count, NodBinding *binding) {
	AssetFile *file			 = NULL;
	uint32_t member			 = UINT32_MAX;
	size_t match_count		 = 0;
	const char *catalog_name = NULL;
	memset(binding, 0, sizeof(*binding));
	binding->model_scale = 1;
	if(modifiers) {
		if(!nod_templates_model(assets->templates, type, modifiers, count, binding)) {
			return 0;
		}
	} else if(reference) {
		if(strlen(reference) >= sizeof(binding->reference)) {
			return fail("native model reference too long");
		}

		strcpy(binding->reference, reference);
		strcpy(binding->provenance, "nsd:tag4");
	} else if(!nod_templates_model(assets->templates, type, NULL, 0, binding)) {
		return 0;
	}

	if(!*binding->reference) {
		if(!*binding->status) {
			strcpy(binding->status, "no-model");
		}

		return 1;
	}

	if(assets->native) {
		return nod_native_resolve(assets->native, binding);
	}

	const char *name = binding->reference;
	if(!strncmp(name, "3D/Models/", 10) || !strncmp(name, "3D\\Models\\", 10)) {
		name += 10;
	}

	if(strchr(name, '/') || strchr(name, '\\') || !*name || !strcmp(name, ".") || !strcmp(name, "..")) {
		strcpy(binding->status, "unsupported-reference-path");
		return 1;
	}

	if(assets->bundle) {
		for(size_t i = 0; i < assets->bundle->count; ++i) {
			if(reference_match(name, assets->bundle->meshes[i].name)) {
				member = (uint32_t)i;
				++match_count;
			}
		}

		if(match_count > 1) {
			strcpy(binding->status, "ambiguous-bundle-member");
			return 1;
		}

		if(match_count == 1) {
			file = assets->bundle;
		}
	}

	if(!file) {
		match_count = 0;
		for(size_t i = 0; i < assets->catalog_count; ++i) {
			if(reference_match(name, assets->catalog[i])) {
				catalog_name = assets->catalog[i];
				++match_count;
			}
		}

		if(match_count != 1) {
			strcpy(binding->status, match_count ? "ambiguous-asset" : "missing-asset");
			return 1;
		}

		file = asset_load(assets, catalog_name);
		if(!file) {
			return 0;
		}

		member = UINT32_MAX;
	}

	{
		const int result = snprintf(binding->asset, sizeof(binding->asset), "3D/Models/%s", file->name);
		if(result < 0 || (size_t)result >= sizeof(binding->asset)) {
			return fail("asset reference is too long");
		}
	}

	binding->checksum = file->checksum;
	if(member == UINT32_MAX && has_extension(file->name, ".nmb")) {
		if(file->count != 1) {
			strcpy(binding->status, file->count ? "bundle-member-required" : "no-render-triangles");
			return 1;
		}

		member = 0;
	}

	if(member != UINT32_MAX) {
		strcpy(binding->member, file->meshes[member].name);
	}

	return preview_write(assets, file, member, binding);
}

/*
 */
// Match the viewer's Ry * Rx * Rz convention, without changing the Z-up axes.
void nod_angles_to_editor(const float native[3], float editor[3]) {
	const double x		  = native[0] * radians_per_degree;
	const double y		  = native[1] * radians_per_degree;
	const double z		  = native[2] * radians_per_degree;
	const double sin_x	  = sin(x);
	const double cos_x	  = cos(x);
	const double sin_y	  = sin(y);
	const double cos_y	  = cos(y);
	const double sin_z	  = sin(z);
	const double cos_z	  = cos(z);
	const double r00	  = cos_y * cos_z + sin_y * sin_x * sin_z;
	const double r10	  = cos_x * sin_z;
	const double r20	  = -sin_y * cos_z + cos_y * sin_x * sin_z;
	const double r21	  = sin_y * sin_z + cos_y * sin_x * cos_z;
	const double r22	  = cos_y * cos_x;
	const double editor_y = asin(fmax(-1, fmin(1, -r20)));
	double editor_x		  = 0;
	double editor_z		  = 0;
	if(fabs(cos(editor_y)) > gimbal_lock_threshold) {
		editor_x = atan2(r21, r22);
		editor_z = atan2(r10, r00);
	} else {
		editor_x = 0;
		editor_z = atan2(cos_y * sin_z - sin_y * sin_x * cos_z, cos_x * cos_z);
	}

	editor[0] = (float)(editor_y / radians_per_degree);
	editor[1] = (float)(editor_z / radians_per_degree);
	editor[2] = (float)(editor_x / radians_per_degree);
}

/*
 */
void nod_angles_from_editor(const float editor[3], float native[3]) {
	const double x		  = editor[2] * radians_per_degree;
	const double y		  = editor[0] * radians_per_degree;
	const double z		  = editor[1] * radians_per_degree;
	const double sin_x	  = sin(x);
	const double cos_x	  = cos(x);
	const double sin_y	  = sin(y);
	const double cos_y	  = cos(y);
	const double sin_z	  = sin(z);
	const double cos_z	  = cos(z);
	const double r12	  = cos_x * sin_y * sin_z - sin_x * cos_z;
	const double r02	  = cos_x * sin_y * cos_z + sin_x * sin_z;
	const double r22	  = cos_x * cos_y;
	const double r10	  = cos_y * sin_z;
	const double r11	  = sin_x * sin_y * sin_z + cos_x * cos_z;
	const double native_x = asin(fmax(-1, fmin(1, -r12)));
	double native_y		  = 0;
	double native_z		  = 0;
	if(fabs(cos(native_x)) > gimbal_lock_threshold) {
		native_y = atan2(r02, r22);
		native_z = atan2(r10, r11);
	} else {
		native_y = atan2(sin_y, cos_y * cos_z);
		native_z = 0;
	}

	native[0] = (float)(native_x / radians_per_degree);
	native[1] = (float)(native_y / radians_per_degree);
	native[2] = (float)(native_z / radians_per_degree);
}
