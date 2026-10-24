/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_materials.h
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

#ifndef __NOD_MATERIALS_H__
#define __NOD_MATERIALS_H__

#include "tool_common.h"

// Mutable material catalogs must not be shared concurrently.
typedef struct NodMaterials NodMaterials;

/*
 */
typedef struct {
		char shader[32];
		char diffuse[256];
		char editor_image[2048];
		uint32_t width;
		uint32_t height;
		uint8_t scripted; // Native archive definition, rather than a direct DDS shader.
} NodMaterialInfo;

/*
 */
// Caller-owned output contexts and their streams must not be shared concurrently.
typedef struct {
		// Borrowed catalog and stream; output_close() releases only the used array.
		// Keep both alive while using this output.
		NodMaterials *materials;
		FILE *mtl;
		// Owns the array, but its strings belong to materials.
		const char **used;
		size_t count;
} NodMaterialOutput;

// The caller owns the returned catalog and releases it with nod_materials_close().
#ifdef __cplusplus
extern "C" {
#endif

NodMaterials *nod_materials_open(const char *root, const char *bundle, const char *preview_root);
void nod_materials_close(NodMaterials *materials);
// Read-only DDS-backed lookup. Editor dimensions follow Materials/shaderlist.txt,
// independently of native level-archive overrides.
// No preview fallback or files are generated; failure preserves info.
int nod_materials_describe(NodMaterials *materials, const char *shader, NodMaterialInfo *info, char *error,
						   size_t error_capacity);
// context points to a NodMaterialOutput. Returns a borrowed preview name/path,
// valid until its material catalog is closed, even after output_close().
const char *nod_materials_use(void *context, const char *shader);
void nod_materials_output_close(NodMaterialOutput *output);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_MATERIALS_H__ */
