/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: niltomap.h
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

#ifndef __NILTOMAP_H__
#define __NILTOMAP_H__

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write triangular MAP brushes, returning 1 on success or 0 on failure.
 *
 * All writers borrow data for the call and never modify or free it. The caller
 * keeps ownership of output/sectors and must close them. Output must be a valid
 * writable stream; sectors may be NULL to omit separately emitted metadata.
 * Data must address data_size bytes for the duration of the call. Scale and
 * thickness must be finite positive values appropriate for MAP float precision.
 * Failed writes may leave partial output; callers handle staging when needed.
 */
int niltomap_write(const unsigned char *data, size_t data_size, FILE *output, double scale, double thickness,
				   FILE *sectors);

/** Same writer contract, optionally reporting processed brushes and skipped triangles. */
int niltomap_write_stats(const unsigned char *data, size_t data_size, FILE *output, double scale, double thickness,
						 FILE *sectors, size_t *brushes, size_t *degenerate);

/** Write brush primitives retaining normalized native UVs, including shear and constant UVs. */
int niltomap_write_uv_stats(const unsigned char *data, size_t data_size, FILE *output, double scale, double thickness,
							FILE *sectors, size_t *brushes, size_t *degenerate);

/** Regenerate unchanged version-1/2 scene geometry for existing sidecars. */
int niltomap_write_legacy(const unsigned char *data, size_t data_size, FILE *output, double scale, double thickness,
						  FILE *sectors);

/**
 * Write worldspawn/sector metadata without brushes, using the same borrowed
 * buffer and stream contract. If non-NULL, triangles receives the number of
 * nondegenerate triangles processed, including before a failure.
 */
int niltomap_write_metadata(const unsigned char *data, size_t data_size, FILE *output, FILE *sectors,
							size_t *triangles);

/**
 * Resolve a material from a borrowed shader token and caller-owned context.
 * Return a borrowed NUL-terminated name valid until the writer consumes it
 * immediately, or NULL to fail. The writer never frees the returned name.
 */
typedef const char *(*NilMaterialResolver)(void *context, const char *shader);

/**
 * Write native XYZ OBJ geometry using the same borrowed buffer/stream contract.
 * Triangles follows the metadata writer's count contract. Resolver may be NULL
 * to use the preview material; context is borrowed and only passed to resolver.
 */
int niltomap_write_obj(const unsigned char *data, size_t data_size, FILE *output, size_t *triangles,
					   NilMaterialResolver resolver, void *context);

/** Write one native/world-space triangle prism without an entity wrapper.
 * Degenerate triangles succeed with written=0. Other successful writes set it to 1.
 */
int niltomap_write_triangle(FILE *output, const double positions[3][3], const char *shader, double thickness,
							int *written);

/** Same triangle contract, preserving normalized UVs on its front face. */
int niltomap_write_triangle_uv(FILE *output, const double positions[3][3], const double uv[3][2], const char *shader,
							   double thickness, int *written);

#ifdef __cplusplus
}
#endif

#endif /* __NILTOMAP_H__ */
