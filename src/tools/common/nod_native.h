/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_native.h
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

#ifndef __NOD_NATIVE_H__
#define __NOD_NATIVE_H__

#include "nod_assets.h"

/*
 */
typedef struct NodNativeAssets NodNativeAssets;
typedef struct NodTemplates NodTemplates;

/*
 */
#ifdef __cplusplus
extern "C" {
#endif

/*
 */
// Owns parser buffers and indexes; release with nod_native_close, do not share concurrently.
NodNativeAssets *nod_native_open(const char *root, const char *bundle);
void nod_native_close(NodNativeAssets *assets);
int nod_native_resolve(NodNativeAssets *assets, NodBinding *binding);
NodTemplates *nod_templates_open(const char *root, const char *const *archives, size_t count);
void nod_templates_close(NodTemplates *templates);
// A non-null modifiers buffer contains count validated 36-byte disk records.
int nod_templates_model(const NodTemplates *templates, const char *type, const unsigned char *modifiers, size_t count,
						NodBinding *binding);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_NATIVE_H__ */
