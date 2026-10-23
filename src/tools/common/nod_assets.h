/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_assets.h
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

#ifndef __NOD_ASSETS_H__
#define __NOD_ASSETS_H__

#include "tool_common.h"

// Mutable asset indexes must not be shared concurrently.
typedef struct NodAssets NodAssets;

/*
 */
typedef struct {
		char reference[256];
		char asset[256];
		char member[33];
		char provenance[128];
		char preview[256];
		char status[64];
		uint32_t checksum;
		uint32_t frame;
		float model_scale;
		float model_offset[3];
} NodBinding;

#ifdef __cplusplus
extern "C" {
#endif

// The caller owns the returned index and releases it with nod_assets_close().
NodAssets *nod_assets_open(const char *root, const char *bundle, const char *preview_root, const char *const *archives,
						   size_t count);
NodAssets *nod_assets_open_native(const char *root, const char *bundle, const char *preview_root,
								  const char *const *archives, size_t count);
void nod_assets_close(NodAssets *assets);
int nod_assets_resolve(NodAssets *assets, const char *type, const char *reference, const unsigned char *modifiers,
					   size_t count, NodBinding *binding);
int nod_assets_nil_preview(NodAssets *assets, const Blob *nil, NodBinding *binding);
void nod_angles_to_editor(const float native[3], float editor[3]);
void nod_angles_from_editor(const float editor[3], float native[3]);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_ASSETS_H__ */
