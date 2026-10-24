/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_materials.c
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

#include "nod_materials.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

/*
 */
enum {
	DDS_HEADER_SIZE			 = 128,
	DDS_HEADER_LENGTH_OFFSET = 4,
	DDS_HEADER_LENGTH		 = 124,
	DDS_PIXEL_FORMAT_LENGTH	 = 32,
	DDS_FLAGS_OFFSET		 = 8,
	DDS_HEIGHT_OFFSET		 = 12,
	DDS_WIDTH_OFFSET		 = 16,
	DDS_PITCH_OFFSET		 = 20,
	DDS_DEPTH_OFFSET		 = 24,
	DDS_PIXEL_FORMAT_OFFSET	 = 76,
	DDS_PIXEL_FLAGS_OFFSET	 = 80,
	DDS_FOURCC_OFFSET		 = 84,
	DDS_PIXEL_BITS_OFFSET	 = 88,
	DDS_RGB_MASKS_OFFSET	 = 92,
	DDS_CAPS2_OFFSET		 = 112,
	DDS_FLAG_PITCH			 = 8,
	DDS_PIXEL_FLAG_FOURCC	 = 4,
	DDS_PIXEL_FLAG_RGB		 = 0x40,
	DDS_UNSUPPORTED_CAPS2	 = 0x0020fe00,
	DDS_FOURCC_DXT1			 = 0x31545844,
	DDS_FOURCC_DXT3			 = 0x33545844,
	DDS_FOURCC_DXT5			 = 0x35545844,
	DDS_MAX_DIMENSION		 = 16384,
	DXT_BLOCK_WIDTH			 = 4,
	DXT1_BLOCK_SIZE			 = 8,
	DXT_ALPHA_BLOCK_SIZE	 = 16,
	DXT_ALPHA_PREFIX_SIZE	 = 8,
	DXT_COLOR_ENDPOINT_SIZE	 = 2,
	DXT_COLOR_INDICES_OFFSET = 4,
	DXT_SELECTOR_BITS		 = 2,
	DXT_PALETTE_COLOR_COUNT	 = 4,
	NATIVE_U32_SIZE			 = 4,
	TGA_HEADER_SIZE			 = 18,
	TGA_TYPE_OFFSET			 = 2,
	TGA_WIDTH_OFFSET		 = 12,
	TGA_HEIGHT_OFFSET		 = 14,
	TGA_BITS_OFFSET			 = 16,
	TGA_DESCRIPTOR_OFFSET	 = 17,
	TGA_TYPE_TRUE_COLOR		 = 2,
	TGA_BITS_RGB			 = 24,
	TGA_ORIGIN_TOP			 = 0x20,
	RGB_CHANNEL_COUNT		 = 3,
	BYTE_BITS				 = 8,
	BYTE_MAX_VALUE			 = 255
};

/*
 */
typedef struct Material {
		char name[256];
		char texture[256];
		char editor_image[256];
		char editor_diffuse[256];
		char preview[128];
		int priority;
		int ambiguous;
		int editor_ambiguous;
		int name_ambiguous;
		int archived;
		int resolved;
		struct Material *next;
} Material;

/*
 */
typedef struct Texture {
		char path[2048];
		char relative[2048];
		char name[256];
		struct Texture *next;
} Texture;

/*
 */
struct NodMaterials {
		char root[1024];
		char preview_root[1024];
		char level[256];
		Material *materials; // Owns the list and the preview strings borrowed by output.
		Texture *textures;	 // Owns the texture catalog list.
		Texture *archives;
		struct NodMaterials *editor_catalog;
		int editor_mode;
		size_t resolved;
		size_t fallback;
};

/*
 */
typedef struct {
		const Blob *data; // Borrows the archive buffer for the duration of parsing.
		size_t pos;
		size_t line;
		char token[256];
} Lexer;

/*
 */
static int fail(const char *message) {
	fprintf(stderr, "NODscene: materials: %s\n", message);
	return 0;
}

/*
 */
static int has_suffix(const char *text, const char *suffix) {
	const size_t text_length   = strlen(text);
	const size_t suffix_length = strlen(suffix);
	return text_length >= suffix_length && !ci(text + text_length - suffix_length, suffix);
}

/*
 */
static int next_token(Lexer *lexer) {
	const unsigned char *bytes = lexer->data->data;
	const size_t size		   = lexer->data->size;
	while(lexer->pos < size) {
		const unsigned char character = bytes[lexer->pos];
		if(isspace(character)) {
			if(character == '\n') {
				++lexer->line;
			}

			++lexer->pos;
			continue;
		}

		if(character == '#' || character == ';' ||
		   (character == '/' && lexer->pos + 1 < size && bytes[lexer->pos + 1] == '/')) {
			while(lexer->pos < size && bytes[lexer->pos] != '\n') {
				++lexer->pos;
			}

			continue;
		}

		if(character == '/' && lexer->pos + 1 < size && bytes[lexer->pos + 1] == '*') {
			lexer->pos += 2;
			while(lexer->pos + 1 < size && !(bytes[lexer->pos] == '*' && bytes[lexer->pos + 1] == '/')) {
				if(bytes[lexer->pos++] == '\n') {
					++lexer->line;
				}
			}

			if(lexer->pos + 1 >= size) {
				fail("unterminated NSA comment");
				return -1;
			}

			lexer->pos += 2;
			continue;
		}

		break;
	}

	if(lexer->pos == size) {
		return 0;
	}

	if(bytes[lexer->pos] == '{' || bytes[lexer->pos] == '}') {
		lexer->token[0] = (char)bytes[lexer->pos++];
		lexer->token[1] = '\0';
		return 1;
	}

	const int is_quoted = bytes[lexer->pos] == '"';
	if(is_quoted) {
		++lexer->pos;
	}

	size_t token_length = 0;
	while(lexer->pos < size) {
		const unsigned char character = bytes[lexer->pos];
		if(is_quoted && character == '"') {
			++lexer->pos;
			lexer->token[token_length] = '\0';
			return 1;
		}

		if(!is_quoted && (isspace(character) || character == '{' || character == '}')) {
			break;
		}

		if(character < 32 || character > 126 || token_length + 1 >= sizeof(lexer->token)) {
			fail("invalid/oversized NSA token");
			return -1;
		}

		lexer->token[token_length++] = (char)character;
		++lexer->pos;
	}

	if(is_quoted) {
		fail("unterminated NSA string");
		return -1;
	}

	lexer->token[token_length] = '\0';
	return 1;
}

/*
 */
static int normalize_texture(const char *input, char output[256]) {
	const size_t length = strlen(input);
	if(length > 251) {
		return fail("texture reference is too long");
	}

	for(size_t i = 0; i <= length; ++i) {
		output[i] = input[i] == '\\' ? '/' : input[i];
	}

	if(length && !strchr(output, '.')) {
		strcat(output, ".dds");
	}

	return 1;
}

/*
 */
static Material *find_material(const NodMaterials *materials, const char *name) {
	for(Material *item = materials->materials; item; item = item->next) {
		if(!ci(item->name, name)) {
			return item;
		}
	}

	return NULL;
}

/*
 */
static int add_material(NodMaterials *materials, const char *name, const char *texture, const int priority,
						const char *editor_image, const char *editor_diffuse) {
	Material *item = find_material(materials, name);
	if(!item) {
		item = calloc(1, sizeof(*item));
		if(!item) {
			return fail("out of memory indexing materials");
		}

		strcpy(item->name, name);
		item->next			 = materials->materials;
		materials->materials = item;
	}

	if(priority > item->priority) {
		strcpy(item->name, name);
		strcpy(item->texture, texture);
		strcpy(item->editor_image, editor_image ? editor_image : "");
		strcpy(item->editor_diffuse, editor_diffuse ? editor_diffuse : "");
		item->archived		   = editor_image != NULL;
		item->editor_ambiguous = 0;
		item->name_ambiguous   = 0;
		item->priority		   = priority;
		item->ambiguous		   = 0;
	} else if(priority == item->priority) {
		if(ci(item->texture, texture)) {
			item->ambiguous = 1;
		}

		if(strcmp(item->name, name)) {
			item->name_ambiguous = 1;
		}

		if(strcmp(item->name, name) || ci(item->editor_image, editor_image ? editor_image : "") ||
		   ci(item->editor_diffuse, editor_diffuse ? editor_diffuse : "")) {
			item->editor_ambiguous = 1;
		}
	}

	return 1;
}

/*
 */
static int read_archive(void *context, const char *path, const char *name) {
	NodMaterials *materials = context;
	Blob data				= {0};
	Lexer lexer				= {&data, 0, 1, {0}};
	int result				= 0;
	int is_successful		= 0;
	const int priority		= !materials->editor_mode && !ci(name, materials->level) ? 2 : 1;
	if(!has_suffix(name, ".nsa") && !has_suffix(name, ".nas")) {
		return 1;
	}

	if(!materials->editor_mode && isdigit((unsigned char)name[0]) && priority != 2) {
		return 1;
	}

	if(!load(path, &data)) {
		goto cleanup;
	}
	while((result = next_token(&lexer)) == 1) {
		char material_name[256]	 = {0};
		char texture[256]		 = "";
		char frame[256]			 = "";
		char editor_image[256]	 = "";
		char editor_diffuse[256] = "";
		int depth				 = 1;
		if(!*lexer.token || !strcmp(lexer.token, "{") || !strcmp(lexer.token, "}")) {
			fail("expected NSA material name");
			goto cleanup;
		}

		strcpy(material_name, lexer.token);
		if(next_token(&lexer) != 1 || strcmp(lexer.token, "{")) {
			fail("expected NSA material block");
			goto cleanup;
		}
		while(depth) {
			if(next_token(&lexer) != 1) {
				fail("unterminated NSA material block");
				goto cleanup;
			}

			if(!strcmp(lexer.token, "{")) {
				++depth;
				continue;
			}

			if(!strcmp(lexer.token, "}")) {
				--depth;
				continue;
			}

			const int is_diffuse	  = depth == 1 && !ci(lexer.token, "texture");
			const int is_first_frame  = depth == 1 && !ci(lexer.token, "frame") && !*frame;
			const int is_editor_image = depth == 1 && !ci(lexer.token, "qer_editorimage");
			const int is_editor_diffuse =
				!*editor_diffuse && (!ci(lexer.token, "texture") || !ci(lexer.token, "diffuseMap"));
			if(!is_diffuse && !is_first_frame && !is_editor_image && !is_editor_diffuse) {
				continue;
			}

			const size_t value_line = lexer.line;
			if(next_token(&lexer) != 1 || ((is_diffuse || is_first_frame) && lexer.line != value_line) ||
			   !strcmp(lexer.token, "{") || !strcmp(lexer.token, "}")) {
				fail("missing NSA image value");
				goto cleanup;
			}

			char reference[256] = {0};
			if(!normalize_texture(lexer.token, reference)) {
				goto cleanup;
			}

			if(is_diffuse) {
				strcpy(texture, reference);
			}

			if(is_first_frame) {
				strcpy(frame, reference);
			}

			if(is_editor_image) {
				strcpy(editor_image, reference);
			}

			if(is_editor_diffuse) {
				strcpy(editor_diffuse, reference);
			}
		}

		if(!add_material(materials, material_name, *texture ? texture : frame, priority, editor_image,
						 editor_diffuse)) {
			goto cleanup;
		}
	}

	is_successful = result == 0;
cleanup:
	if(!is_successful) {
		fprintf(stderr, "NODscene: materials: invalid archive '%s', line %zu\n", path, lexer.line);
	}

	free(data.data);
	return is_successful;
}

/*
 */
static int read_effect_archive(void *context, const char *path, const char *name) {
	return !has_suffix(name, ".nsa") || read_archive(context, path, name);
}

/*
 */
static int index_texture(void *context, const char *path, const char *name) {
	NodMaterials *materials	 = context;
	const size_t root_length = strlen(materials->root);
	const int archive		 = has_suffix(name, ".nsa") || has_suffix(name, ".nas");
	if(!archive && !has_suffix(name, ".dds") && !has_suffix(name, ".tga") && !has_suffix(name, ".jpg") &&
	   !has_suffix(name, ".png")) {
		return 1;
	}

	if(strlen(name) >= 256 || strlen(path) >= 2048) {
		return fail("texture catalog path is too long");
	}

	Texture *item = calloc(1, sizeof(*item));
	if(!item) {
		return fail("out of memory indexing textures");
	}

	strcpy(item->name, name);
	strcpy(item->path, path);
	strcpy(item->relative, path + root_length + 1);
	for(size_t i = 0; item->relative[i]; ++i) {
		if(item->relative[i] == '\\') {
			item->relative[i] = '/';
		}
	}

	Texture **list = archive ? &materials->archives : &materials->textures;
	item->next	   = *list;
	*list		   = item;
	return 1;
}

/*
 */
static void decode_rgb565(const uint16_t value, unsigned char color[RGB_CHANNEL_COUNT]) {
	const unsigned red	 = (value >> 11) & 31;
	const unsigned green = (value >> 5) & 63;
	const unsigned blue	 = value & 31;
	color[0]			 = (unsigned char)((red << 3) | (red >> 2));
	color[1]			 = (unsigned char)((green << 2) | (green >> 4));
	color[2]			 = (unsigned char)((blue << 3) | (blue >> 2));
}

/*
 */
static int decode_channel(const uint32_t pixel, uint32_t mask) {
	unsigned shift = 0;
	if(!mask) {
		return 0;
	}
	while(!(mask & 1)) {
		mask >>= 1;
		++shift;
	}

	return (int)(((uint64_t)((pixel >> shift) & mask) * BYTE_MAX_VALUE) / mask);
}

/*
 */
static int is_valid_mask(uint32_t mask, const unsigned bits) {
	if(!mask || (bits < 32 && (mask >> bits))) {
		return 0;
	}
	while(!(mask & 1)) {
		mask >>= 1;
	}

	return (mask & (mask + 1)) == 0;
}

/*
 */
typedef struct {
		uint32_t width;
		uint32_t height;
		uint32_t bits;
		uint32_t masks[RGB_CHANNEL_COUNT];
		size_t row_size;
		unsigned block_size;
} DdsLayout;

/*
 */
static int read_dds_layout(const Blob *dds, DdsLayout *layout) {
	if(dds->size < DDS_HEADER_SIZE || memcmp(dds->data, "DDS ", 4) ||
	   u32(dds->data + DDS_HEADER_LENGTH_OFFSET) != DDS_HEADER_LENGTH ||
	   u32(dds->data + DDS_PIXEL_FORMAT_OFFSET) != DDS_PIXEL_FORMAT_LENGTH) {
		return fail("invalid DDS header");
	}

	layout->width		  = u32(dds->data + DDS_WIDTH_OFFSET);
	layout->height		  = u32(dds->data + DDS_HEIGHT_OFFSET);
	const uint32_t flags  = u32(dds->data + DDS_PIXEL_FLAGS_OFFSET);
	const uint32_t fourcc = u32(dds->data + DDS_FOURCC_OFFSET);
	layout->bits		  = u32(dds->data + DDS_PIXEL_BITS_OFFSET);
	if(!layout->width || !layout->height || layout->width > DDS_MAX_DIMENSION || layout->height > DDS_MAX_DIMENSION ||
	   (size_t)layout->width > (SIZE_MAX - TGA_HEADER_SIZE) / RGB_CHANNEL_COUNT / layout->height) {
		return fail("invalid DDS dimensions");
	}

	if(u32(dds->data + DDS_DEPTH_OFFSET) > 1 || (u32(dds->data + DDS_CAPS2_OFFSET) & DDS_UNSUPPORTED_CAPS2)) {
		return fail("only 2D diffuse DDS images are supported");
	}

	size_t payload_size = 0;
	if(flags & DDS_PIXEL_FLAG_FOURCC) {
		layout->block_size = fourcc == DDS_FOURCC_DXT1									? DXT1_BLOCK_SIZE
							 : (fourcc == DDS_FOURCC_DXT3 || fourcc == DDS_FOURCC_DXT5) ? DXT_ALPHA_BLOCK_SIZE
																						: 0;
		if(!layout->block_size) {
			return fail("unsupported DDS compression (expected DXT1/3/5)");
		}

		layout->row_size = ((size_t)layout->width + DXT_BLOCK_WIDTH - 1) / DXT_BLOCK_WIDTH * layout->block_size;
		payload_size	 = layout->row_size * (((size_t)layout->height + DXT_BLOCK_WIDTH - 1) / DXT_BLOCK_WIDTH);
	} else {
		if(!(flags & DDS_PIXEL_FLAG_RGB) || (layout->bits != 16 && layout->bits != 24 && layout->bits != 32)) {
			return fail("unsupported DDS pixel format");
		}

		for(size_t i = 0; i < RGB_CHANNEL_COUNT; ++i) {
			layout->masks[i] = u32(dds->data + DDS_RGB_MASKS_OFFSET + i * NATIVE_U32_SIZE);
		}

		if(!is_valid_mask(layout->masks[0], layout->bits) || !is_valid_mask(layout->masks[1], layout->bits) ||
		   !is_valid_mask(layout->masks[2], layout->bits) || (layout->masks[0] & layout->masks[1]) ||
		   (layout->masks[0] & layout->masks[2]) || (layout->masks[1] & layout->masks[2])) {
			return fail("invalid DDS RGB masks");
		}

		layout->row_size	 = (size_t)layout->width * (layout->bits / BYTE_BITS);
		const uint32_t pitch = u32(dds->data + DDS_PITCH_OFFSET);
		if(u32(dds->data + DDS_FLAGS_OFFSET) & DDS_FLAG_PITCH) {
			if(pitch < layout->row_size) {
				return fail("invalid DDS row pitch");
			}

			layout->row_size = pitch;
		}

		if(layout->row_size > SIZE_MAX / layout->height) {
			return fail("DDS payload size overflow");
		}

		payload_size = layout->row_size * layout->height;
	}

	if(payload_size > dds->size - DDS_HEADER_SIZE) {
		return fail("truncated DDS top mip");
	}

	return 1;
}

/*
 */
static void decode_dxt_pixel(const unsigned char *block, const unsigned block_size, const size_t x, const size_t y,
							 unsigned char rgb[RGB_CHANNEL_COUNT]) {
	unsigned char palette[DXT_PALETTE_COLOR_COUNT][RGB_CHANNEL_COUNT] = {{0}};
	const uint16_t first_color										  = u16(block);
	const uint16_t second_color										  = u16(block + DXT_COLOR_ENDPOINT_SIZE);
	const unsigned palette_index =
		(u32(block + DXT_COLOR_INDICES_OFFSET) >>
		 (DXT_SELECTOR_BITS * ((y % DXT_BLOCK_WIDTH) * DXT_BLOCK_WIDTH + x % DXT_BLOCK_WIDTH))) &
		(DXT_PALETTE_COLOR_COUNT - 1);
	
	decode_rgb565(first_color, palette[0]);
	decode_rgb565(second_color, palette[1]);
	
	for(size_t i = 0; i < RGB_CHANNEL_COUNT; ++i) {
		if(first_color > second_color || block_size == DXT_ALPHA_BLOCK_SIZE) {
			palette[2][i] = (unsigned char)((2 * palette[0][i] + palette[1][i]) / 3);
			palette[3][i] = (unsigned char)((palette[0][i] + 2 * palette[1][i]) / 3);
		} else {
			palette[2][i] = (unsigned char)((palette[0][i] + palette[1][i]) / 2);
			palette[3][i] = 0;
		}

		rgb[i] = palette[palette_index][i];
	}
}

/*
 */
static int decode_dds(const Blob *dds, Blob *tga) {
	DdsLayout layout = {0};
	if(!read_dds_layout(dds, &layout)) {
		return 0;
	}

	tga->size = TGA_HEADER_SIZE + (size_t)layout.width * layout.height * RGB_CHANNEL_COUNT;
	tga->data = calloc(1, tga->size);
	if(!tga->data) {
		return fail("out of memory decoding diffuse texture");
	}

	tga->data[TGA_TYPE_OFFSET] = TGA_TYPE_TRUE_COLOR;
	put16(tga->data + TGA_WIDTH_OFFSET, (uint16_t)layout.width);
	put16(tga->data + TGA_HEIGHT_OFFSET, (uint16_t)layout.height);
	tga->data[TGA_BITS_OFFSET]		 = TGA_BITS_RGB;
	tga->data[TGA_DESCRIPTOR_OFFSET] = TGA_ORIGIN_TOP;
	for(size_t y = 0; y < layout.height; ++y) {
		for(size_t x = 0; x < layout.width; ++x) {
			unsigned char rgb[RGB_CHANNEL_COUNT] = {0};
			unsigned char *destination = tga->data + TGA_HEADER_SIZE + (y * layout.width + x) * RGB_CHANNEL_COUNT;
			if(layout.block_size) {
				const unsigned char *block = dds->data + DDS_HEADER_SIZE + (y / DXT_BLOCK_WIDTH) * layout.row_size +
											 (x / DXT_BLOCK_WIDTH) * layout.block_size +
											 (layout.block_size == DXT_ALPHA_BLOCK_SIZE ? DXT_ALPHA_PREFIX_SIZE : 0);
				decode_dxt_pixel(block, layout.block_size, x, y, rgb);
			} else {
				const unsigned char *pixel_bytes =
					dds->data + DDS_HEADER_SIZE + y * layout.row_size + x * (layout.bits / BYTE_BITS);
				uint32_t pixel = 0;
				for(size_t i = 0; i < layout.bits / BYTE_BITS; ++i) {
					pixel |= (uint32_t)pixel_bytes[i] << (i * BYTE_BITS);
				}

				for(size_t i = 0; i < RGB_CHANNEL_COUNT; ++i) {
					rgb[i] = (unsigned char)decode_channel(pixel, layout.masks[i]);
				}
			}

			destination[0] = rgb[2];
			destination[1] = rgb[1];
			destination[2] = rgb[0];
		}
	}

	return 1;
}

/*
 */
static int texture_matches(const Texture *item, const char *reference) {
	if(!has_suffix(item->name, ".dds")) {
		return 0;
	}

	if(!strchr(reference, '/')) {
		return !ci(item->name, reference);
	}

	if(!ci(item->relative, reference)) {
		return 1;
	}

	if(!strncmp(item->relative, "Materials/", 10)) {
		return !ci(item->relative + 10, reference);
	}

	if(!strncmp(item->relative, "3D/Materials/", 13)) {
		return !ci(item->relative + 13, reference);
	}

	return 0;
}

/*
 */
static int texture_rank(const Texture *item, const char *reference) {
	// Preserve the native search order used by viewer/scene/DdsCatalog.cpp.
	static const char *const directories[] = {
		"Materials/Common",	 "Materials/Metal",	  "Materials/Stone",   "Materials/Wood",   "Materials/Organic",
		"Materials/Glass",	 "Materials/Effects", "Materials/Terrain", "Materials/Colors", "Materials/Lights",
		"Materials/Shaders", "Materials",		  "3D/Materials"};
	if(strchr(reference, '/')) {
		return 0;
	}

	char parent[2048] = {0};
	strcpy(parent, item->relative);
	char *separator = strrchr(parent, '/');
	if(separator) {
		*separator = '\0';
	}

	for(size_t i = 0; i < sizeof(directories) / sizeof(*directories); ++i) {
		if(!ci(parent, directories[i])) {
			return (int)i;
		}
	}

	return (int)(sizeof(directories) / sizeof(*directories));
}

/*
 */
static int select_texture_source(const NodMaterials *materials, const char *reference, const char **fallback_reason,
								 Blob *source) {
	int best_rank = INT_MAX;
	for(const Texture *item = materials->textures; !*fallback_reason && item; item = item->next) {
		if(texture_matches(item, reference) && texture_rank(item, reference) < best_rank) {
			best_rank = texture_rank(item, reference);
		}
	}

	for(const Texture *item = materials->textures; !*fallback_reason && item; item = item->next) {
		Blob candidate = {0};
		if(!texture_matches(item, reference) || texture_rank(item, reference) != best_rank) {
			continue;
		}

		if(!load(item->path, &candidate)) {
			return 0;
		}

		if(source->data && (source->size != candidate.size || memcmp(source->data, candidate.data, source->size))) {
			*fallback_reason = "ambiguous-texture";
			free(candidate.data);
			break;
		}

		if(!source->data) {
			*source = candidate; // Transfer the candidate buffer to the caller for cleanup.
		} else {
			free(candidate.data);
		}
	}

	if(!source->data && !*fallback_reason) {
		*fallback_reason = "missing-texture";
	}

	return 1;
}

/*
 */
static int image_rank(const char *path) {
	if(has_suffix(path, ".dds")) {
		return 0;
	}

	if(has_suffix(path, ".tga")) {
		return 1;
	}

	if(has_suffix(path, ".jpg")) {
		return 2;
	}

	return 3;
}

/*
 */
static int same_image_key(const char *left, const char *right) {
	const size_t a = strlen(left);
	const size_t b = strlen(right);
	if(a < 4 || b < 4 || a != b) {
		return 0;
	}

	for(size_t i = 0; i < a - 4; ++i) {
		if(tolower((unsigned char)left[i]) != tolower((unsigned char)right[i])) {
			return 0;
		}
	}

	return 1;
}

/*
 */
static const Texture *editor_texture(const NodMaterials *materials, const char *reference, const char **reason) {
	char prefixed[2048];
	snprintf(prefixed, sizeof(prefixed), "Materials/%s", reference);
	// Radiant tries exact paths, then Materials/, then an unambiguous basename.
	// Unlike native diffuse lookup, duplicate aliases have no directory priority.
	for(int pass = 0; pass < 3; ++pass) {
		const Texture *found = NULL;
		if(pass == 2 && strchr(reference, '/')) {
			break;
		}

		for(const Texture *item = materials->textures; item; item = item->next) {
			const int matches = pass == 2 ? same_image_key(item->name, reference)
										  : same_image_key(item->relative, pass == 0 ? reference : prefixed);
			if(!matches) {
				continue;
			}

			if(found &&
			   (!same_image_key(found->relative, item->relative) ||
				(image_rank(found->name) == image_rank(item->name) && strcmp(found->relative, item->relative)))) {
				*reason = "ambiguous editor image";
				return NULL;
			}

			if(!found || image_rank(item->name) < image_rank(found->name)) {
				found = item;
			}
		}

		if(found) {
			return found;
		}
	}

	*reason = "missing editor image";
	return NULL;
}

/*
 */
static int load_editor_catalog(NodMaterials *materials, const char **reason) {
	if(materials->editor_catalog) {
		return 1;
	}

	NodMaterials *catalog = calloc(1, sizeof(*catalog));
	Blob manifest		  = {0};
	char path[2048]		  = {0};
	int success			  = 0;
	if(!catalog) {
		*reason = "out of memory loading editor materials";
		return 0;
	}

	catalog->editor_mode = 1;
	if(!path_join(path, sizeof(path), materials->root, "Materials/shaderlist.txt") || !load(path, &manifest)) {
		*reason = "cannot read Radiant material shaderlist";
		goto cleanup;
	}

	Lexer lexer = {&manifest, 0, 1, {0}};
	int token;
	while((token = next_token(&lexer)) == 1) {
		char relative[2048];
		for(size_t i = 0; lexer.token[i]; ++i) {
			if(lexer.token[i] == '\\') {
				lexer.token[i] = '/';
			}
		}

		const char *component = lexer.token;
		if(!*component || strchr(component, ':')) {
			*reason = "invalid editor archive path";
			goto cleanup;
		}
		while(component) {
			const char *separator = strchr(component, '/');
			const size_t length	  = separator ? (size_t)(separator - component) : strlen(component);
			if(!length || (length == 1 && *component == '.') || (length == 2 && !strncmp(component, "..", 2))) {
				*reason = "invalid editor archive path";
				goto cleanup;
			}

			component = separator ? separator + 1 : NULL;
		}

		const size_t token_length = strlen(lexer.token);
		const int extension		  = token_length >= 4 && !strcmp(lexer.token + token_length - 4, ".nsa");
		snprintf(relative, sizeof(relative), "Materials/%s%s", lexer.token, extension ? "" : ".nsa");
		const Texture *archive = NULL;
		for(const Texture *item = materials->archives; item; item = item->next) {
			if(ci(item->relative, relative)) {
				continue;
			}

			if(archive) {
				*reason = "ambiguous editor archive path";
				goto cleanup;
			}

			archive = item;
		}

		if(!archive) {
			fprintf(stderr, "NODscene: materials: missing editor archive '%s'\n", relative);
			continue;
		}

		if(!read_archive(catalog, archive->path, archive->name)) {
			*reason = "invalid editor material archive";
			goto cleanup;
		}
	}

	if(token < 0) {
		*reason = "invalid material shaderlist";
		goto cleanup;
	}

	materials->editor_catalog = catalog;
	success					  = 1;
cleanup:
	free(manifest.data);
	if(!success) {
		nod_materials_close(catalog);
	}

	return success;
}

/*
 */
int nod_materials_describe(NodMaterials *materials, const char *shader, NodMaterialInfo *info, char *error,
						   size_t error_capacity) {
	NodMaterialInfo result	   = {0};
	Blob diffuse			   = {0};
	Blob image				   = {0};
	DdsLayout diffuse_layout   = {0};
	DdsLayout image_layout	   = {0};
	const char *reason		   = NULL;
	const char *diffuse_reason = NULL;
	char direct[256]		   = {0};
	char editor_reference[256] = {0};
	const Texture *editor	   = NULL;
	Material *material		   = NULL;
	int success				   = 0;
	if(!materials || !shader || !info || (error_capacity && !error)) {
		reason = "invalid material lookup";
		goto cleanup;
	}

	if(!strncmp(shader, "textures/nod/", 13)) {
		shader += 13;
	} else if(!strncmp(shader, "nod/", 4)) {
		shader += 4;
	}

	if(!*shader || strlen(shader) >= sizeof(result.shader)) {
		reason = "native shader name must be 1-31 bytes";
		goto cleanup;
	}

	for(const unsigned char *c = (const unsigned char *)shader; *c; ++c) {
		if(*c <= 32 || *c > 126) {
			reason = "invalid native shader name";
			goto cleanup;
		}
	}

	material = find_material(materials, shader);
	if(material && (material->ambiguous || material->name_ambiguous)) {
		reason = "ambiguous material";
		goto cleanup;
	}

	if(!load_editor_catalog(materials, &reason)) {
		goto cleanup;
	}

	Material *editor_material = find_material(materials->editor_catalog, shader);
	if(editor_material && (editor_material->ambiguous || editor_material->editor_ambiguous)) {
		reason = "ambiguous editor material";
		goto cleanup;
	}

	if(!normalize_texture(shader, direct)) {
		reason = "invalid texture reference";
		goto cleanup;
	}

	const int archived = material && material->archived;
	if(!archived) {
		char nas_name[256];
		snprintf(nas_name, sizeof(nas_name), "%s.nas", shader);
		for(const Texture *item = materials->archives; item; item = item->next) {
			if(!ci(item->name, nas_name)) {
				reason = "standalone NAS shaders are not supported";
				goto cleanup;
			}
		}
	}

	if(editor_material && !archived) {
		reason = "editor material has no native bank definition";
		goto cleanup;
	}

	if(!archived && (strchr(shader, '/') || strchr(shader, '\\') || strchr(shader, '.'))) {
		reason = "direct DDS shaders require a basename";
		goto cleanup;
	}

	const char *reference = archived ? material->texture : direct;
	const char *editor_name =
		editor_material ? (*editor_material->editor_image
							   ? editor_material->editor_image
							   : (*editor_material->editor_diffuse ? editor_material->editor_diffuse : direct))
						: direct;
	if(!*reference || !ci(reference, "dflt.dds")) {
		reason = "material has no supported diffuse DDS";
		goto cleanup;
	}

	if(!has_suffix(reference, ".dds") || !has_suffix(editor_name, ".dds")) {
		reason = "only DDS-backed materials and editor images are supported";
		goto cleanup;
	}

	strcpy(editor_reference, editor_name);
	editor = editor_texture(materials, editor_reference, &reason);
	if(!editor) {
		goto cleanup;
	}

	if(!has_suffix(editor->name, ".dds")) {
		reason = "editor image is not a supported DDS";
		goto cleanup;
	}

	if(!select_texture_source(materials, reference, &diffuse_reason, &diffuse)) {
		reason = "cannot read diffuse DDS";
		goto cleanup;
	}

	if(diffuse_reason) {
		reason = diffuse_reason;
		goto cleanup;
	}

	if(!read_dds_layout(&diffuse, &diffuse_layout)) {
		reason = "invalid diffuse DDS";
		goto cleanup;
	}

	if(!load(editor->path, &image) || !read_dds_layout(&image, &image_layout)) {
		reason = "invalid editor DDS";
		goto cleanup;
	}

	const char *native_name	 = archived ? material->name : editor->name;
	const size_t name_length = strlen(native_name) - (archived ? 0 : 4);
	if(!name_length || name_length >= sizeof(result.shader)) {
		reason = "native shader name must be 1-31 bytes";
		goto cleanup;
	}

	memcpy(result.shader, native_name, name_length);
	if(archived) {
		strcpy(result.diffuse, reference);
	} else {
		snprintf(result.diffuse, sizeof(result.diffuse), "%s.dds", result.shader);
	}

	strcpy(result.editor_image, editor->relative);
	result.width	= image_layout.width;
	result.height	= image_layout.height;
	result.scripted = (uint8_t)archived;
	*info			= result;
	success			= 1;
cleanup:
	free(diffuse.data);
	free(image.data);
	if(error && error_capacity) {
		snprintf(error, error_capacity, "%s", success ? "" : reason);
	}

	return success ? 1 : fail(reason);
}

/*
 */
static const char *resolve_material(NodMaterials *materials, const char *shader) {
	Material *material			= find_material(materials, shader);
	Blob source					= {0};
	Blob tga					= {0};
	const char *fallback_reason = NULL;
	int is_successful			= 0;
	if(!material) {
		char direct_reference[256] = {0};
		if(!normalize_texture(shader, direct_reference) ||
		   !add_material(materials, shader, direct_reference, 1, NULL, NULL)) {
			return NULL;
		}

		material = find_material(materials, shader);
	}

	if(material->resolved) {
		return material->preview;
	}

	const char *reference = material->texture;
	if(material->ambiguous) {
		fallback_reason = "ambiguous-material";
	} else if(!*reference || !ci(reference, "dflt.dds")) {
		fallback_reason = "no-diffuse-texture";
	}

	if(!select_texture_source(materials, reference, &fallback_reason, &source)) {
		goto cleanup;
	}

	if(fallback_reason) {
		fprintf(stderr, "NODscene: materials: '%s' -> '%s': %s; explicit white preview\n", shader, reference,
				fallback_reason);
		strcpy(material->preview, "textures/nodpreview/white");
		++materials->fallback;
	} else {
		if(!decode_dds(&source, &tga)) {
			fprintf(stderr, "NODscene: materials: failed diffuse '%s' for '%s'\n", reference, shader);
			goto cleanup;
		}

		snprintf(material->preview, sizeof(material->preview), "textures/nodpreview/d1-%08x", (unsigned)crc32(&source));
		char path[2048] = {0};
		if(snprintf(path, sizeof(path), "%s.tga", material->preview) < 0) {
			goto cleanup;
		}

		{
			char full_path[2048] = {0};
			if(!path_join(full_path, sizeof(full_path), materials->preview_root, path) ||
			   !save_generated(full_path, &tga)) {
				goto cleanup;
			}
		}

		++materials->resolved;
	}

	material->resolved = 1;
	is_successful	   = 1;
cleanup:
	free(source.data);
	free(tga.data);
	return is_successful ? material->preview : NULL;
}

/*
 */
const char *nod_materials_use(void *context, const char *shader) {
	NodMaterialOutput *output = context;
	const char *name		  = resolve_material(output->materials, shader);
	if(!name) {
		return NULL;
	}

	for(size_t i = 0; i < output->count; ++i) {
		if(!strcmp(output->used[i], name)) {
			return name;
		}
	}

	if(output->count >= SIZE_MAX / sizeof(*output->used) - 1) {
		fail("material count overflow");
		return NULL;
	}

	const char **list = realloc(output->used, (output->count + 1) * sizeof(*list));
	if(!list) {
		fail("out of memory collecting materials");
		return NULL;
	}

	output->used				  = list;
	output->used[output->count++] = name;
	if(fprintf(output->mtl, "newmtl %s\nKd 1 1 1\nmap_Kd %s.tga\n", name, name) < 0) {
		fail("cannot write preview MTL");
		return NULL;
	}

	return name;
}

/*
 */
void nod_materials_output_close(NodMaterialOutput *output) {
	free(output->used);
}

/*
 */
NodMaterials *nod_materials_open(const char *root, const char *bundle, const char *preview_root) {
	NodMaterials *materials				   = calloc(1, sizeof(*materials));
	char path[2048]						   = {0};
	static const char *const directories[] = {"Materials", "3D/Materials"};
	const size_t bundle_length			   = strlen(bundle);
	if(!materials) {
		fail("out of memory creating material catalog");
		return NULL;
	}

	if(strlen(root) >= sizeof(materials->root) || strlen(preview_root) >= sizeof(materials->preview_root) ||
	   bundle_length >= sizeof(materials->level)) {
		fail("material catalog path is too long");
		goto failed;
	}

	strcpy(materials->root, root);
	strcpy(materials->preview_root, preview_root);
	if(has_suffix(bundle, ".nmb")) {
		strcpy(materials->level, bundle);
		memcpy(materials->level + bundle_length - 3, "nsa", 3);
	}

	for(size_t i = 0; i < sizeof(directories) / sizeof(*directories); ++i) {
		if(!path_join(path, sizeof(path), root, directories[i]) || !visit_files(path, 0, 1, read_archive, materials) ||
		   !visit_files(path, 1, 1, index_texture, materials)) {
			goto failed;
		}
	}

	if(!path_join(path, sizeof(path), root, "Materials/Effects") ||
	   !visit_files(path, 0, 1, read_effect_archive, materials)) {
		goto failed;
	}

	return materials;
failed:
	nod_materials_close(materials);
	return NULL;
}

/*
 */
void nod_materials_close(NodMaterials *materials) {
	if(!materials) {
		return;
	}

	if(materials->resolved || materials->fallback) {
		printf("NODscene: diffuse previews: %zu native shaders textured, %zu explicit white fallbacks\n",
			   materials->resolved, materials->fallback);
	}
	while(materials->materials) {
		Material *material	 = materials->materials;
		materials->materials = material->next;
		free(material);
	}
	while(materials->textures) {
		Texture *texture	= materials->textures;
		materials->textures = texture->next;
		free(texture);
	}
	while(materials->archives) {
		Texture *archive	= materials->archives;
		materials->archives = archive->next;
		free(archive);
	}

	nod_materials_close(materials->editor_catalog);
	free(materials);
}
