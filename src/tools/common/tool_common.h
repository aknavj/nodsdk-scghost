/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: tool_common.h
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

#ifndef __TOOL_COMMON_H__
#define __TOOL_COMMON_H__

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 */
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
	#error NOD tools require IEEE-754 binary32 floats
#endif

/*
 */
typedef char Binary32Required[sizeof(float) == 4 ? 1 : -1];

/*
 */
typedef struct {
		unsigned char *data;
		size_t size;
} Blob;

/*
 */
static inline uint16_t u16(const unsigned char *data) {
	return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

/*
 */
static inline uint32_t u32(const unsigned char *data) {
	return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/*
 */
static inline void put16(unsigned char *data, uint16_t value) {
	data[0] = (unsigned char)value;
	data[1] = (unsigned char)(value >> 8);
}

/*
 */
static inline void put32(unsigned char *data, uint32_t value) {
	data[0] = (unsigned char)value;
	data[1] = (unsigned char)(value >> 8);
	data[2] = (unsigned char)(value >> 16);
	data[3] = (unsigned char)(value >> 24);
}

/*
 */
static inline float f32(const unsigned char *data) {
	const uint32_t bits = u32(data);
	float value			= 0.0f;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

/*
 */
static inline void put_float(unsigned char *data, float value) {
	uint32_t bits = 0;
	memcpy(&bits, &value, sizeof(bits));
	put32(data, bits);
}

#ifdef __cplusplus
extern "C" {
#endif

/*
 */
uint32_t crc32(const Blob *blob);

/*
 */
// The caller releases blob->data, including after a failed read/load.
int read_stream(FILE *file, Blob *blob);
int load(const char *path, Blob *blob);
int save(const char *path, const Blob *blob);
int save_generated(const char *path, const Blob *blob);
int path_join(char *output, size_t capacity, const char *base_path, const char *relative_path);
int ci(const char *left, const char *right);

typedef int (*FileVisitor)(void *context, const char *path, const char *name);
int visit_files(const char *directory, int recursive, int optional, FileVisitor visitor, void *context);

#ifdef __cplusplus
}
#endif

#endif /* __TOOL_COMMON_H__ */
