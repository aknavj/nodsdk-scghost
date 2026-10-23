/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: niltomap.c
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
#include "tool_common.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 */
enum {
	NIL_VERSION					   = 35,
	NIL_MAGIC_SIZE				   = 4,
	NIL_HEADER_SIZE				   = 96,
	NIL_VERSION_OFFSET			   = 4,
	NIL_SHADER_COUNT_OFFSET		   = 92,
	NIL_SHADER_NAME_SIZE		   = 32,
	NIL_SHADER_TOKEN_SIZE		   = NIL_SHADER_NAME_SIZE + 1,
	NIL_U16_SIZE				   = 2,
	NIL_U32_SIZE				   = 4,
	NIL_SECTOR_HEADER_SIZE		   = 17,
	NIL_SECTOR_CENTER_X_OFFSET	   = 4,
	NIL_SECTOR_CENTER_Y_OFFSET	   = 8,
	NIL_SECTOR_CENTER_Z_OFFSET	   = 12,
	NIL_SECTOR_PORTAL_COUNT_OFFSET = 16,
	NIL_PORTAL_SIZE				   = 16,
	NIL_VERTEX_HEADER_SIZE		   = 4,
	NIL_VERTEX_SIZE				   = 36,
	NIL_POSITION_Y_OFFSET		   = 4,
	NIL_POSITION_Z_OFFSET		   = 8,
	NIL_NORMAL_X_OFFSET			   = 12,
	NIL_NORMAL_Y_OFFSET			   = 16,
	NIL_NORMAL_Z_OFFSET			   = 20,
	NIL_UV_U_OFFSET				   = 28,
	NIL_UV_V_OFFSET				   = 32,
	NIL_SECONDARY_HEADER_SIZE	   = 5,
	NIL_SECONDARY_COUNT_OFFSET	   = 1,
	NIL_SECONDARY_RECORD_SIZE	   = 72,
	NIL_GROUP_SIZE				   = 47,
	NIL_GROUP_BASE_OFFSET		   = 4,
	NIL_GROUP_STRIP_COUNT_OFFSET   = 12,
	NIL_GROUP_LIST_COUNT_OFFSET	   = 14,
	NIL_GROUP_VERTEX_POOL_OFFSET   = 20,
	NIL_GROUP_SECONDARY_POOL	   = 3,
	NIL_GEOMETRY_NONE			   = 0,
	NIL_GEOMETRY_INDEXED		   = 1,
	NIL_GEOMETRY_SECONDARY		   = 2,
	NIL_VERTEX_SINGLE			   = 0,
	NIL_VERTEX_DUAL				   = 1,
	NIL_VERTEX_EXTENDED			   = 2,
	NIL_SECONDARY_STREAM_TYPE	   = 1,
	NIL_TRAILING_RECORD_SIZE	   = 68,
	NIL_TRAILING_TBN_SIZE		   = 72,
	NIL_TRAILING_NAME_SIZE		   = 32,
	NIL_SECTOR_FOOTER_SIZE		   = 6,
	NIL_TRIANGLE_VERTEX_COUNT	   = 3,
	NIL_TRIANGLE_LAST_VERTEX	   = NIL_TRIANGLE_VERTEX_COUNT - 1,
	NIL_OBJ_NORMAL_X_ATTRIBUTE	   = 0,
	NIL_OBJ_NORMAL_Y_ATTRIBUTE	   = 1,
	NIL_OBJ_NORMAL_Z_ATTRIBUTE	   = 2,
	NIL_OBJ_U_ATTRIBUTE			   = 3,
	NIL_OBJ_V_ATTRIBUTE			   = 4,
	NIL_OBJ_ATTRIBUTE_COUNT		   = 5,
	NIL_OBJ_SHADER_SIZE			   = 128,
	NIL_CLI_PATH_COUNT			   = 2,
	NIL_COPY_BUFFER_SIZE		   = 8192
};

/*
 */
static const double map_rebuild_normal_tolerance	 = 0.01;
static const double map_normal_tolerance			 = 0.001;
static const double map_plane_minimum_span			 = 64;
static const double map_plane_relative_span			 = 1e-6;
static const double triangle_relative_area_tolerance = 1e-12;
static const char texture_prefix[]					 = "textures/";
static const char caulk_shader[]					 = "common/caulk";

/*
 */
typedef struct {
		// Borrowed input storage; all record views remain valid for its lifetime.
		const unsigned char *data;
		size_t size;
		size_t offset;
} Reader;

/*
 */
typedef struct {
		double x;
		double y;
		double z;
} Vec3;

/*
 */
typedef enum { 
	OUTPUT_BRUSHES, 
	OUTPUT_METADATA, 
	OUTPUT_OBJ, 
	OUTPUT_UV_BRUSHES 
} OutputMode;

/*
 */
typedef struct {
		double scale;
		double thickness;
		size_t brushes;
		size_t degenerate;
		FILE *sectors;
		int has_legacy_planes;
		OutputMode output_mode;
		NilMaterialResolver resolver;
		void *material_context;
		char obj_shader[NIL_OBJ_SHADER_SIZE];
		size_t obj_groups;
} Options;

/*
 */
typedef struct {
		// Views into Reader.data, never separately allocated or freed.
		const unsigned char *vertices;
		const unsigned char *groups;
		const unsigned char *indices;
		const unsigned char *secondary;
		uint16_t vertex_count;
		uint16_t group_count;
		uint32_t index_count;
		uint32_t secondary_count;
		uint8_t vertex_type;
} SectorMesh;

/*
 */
// Return a borrowed record view without transferring ownership.
static const unsigned char *read_records(Reader *reader, const size_t count, const size_t stride) {
	if(count > (reader->size - reader->offset) / stride) {
		fprintf(stderr, "NILtoMAP: truncated data at byte %zu (need %zu records of %zu bytes)\n", reader->offset, count,
				stride);
		return NULL;
	}

	const unsigned char *const records = reader->data + reader->offset;
	reader->offset += count * stride;
	return records;
}

/*
 */
static Vec3 subtract_vector(const Vec3 first, const Vec3 second) {
	const Vec3 difference = {first.x - second.x, first.y - second.y, first.z - second.z};
	return difference;
}

/*
 */
static Vec3 cross_product(const Vec3 first, const Vec3 second) {
	const Vec3 product = {first.y * second.z - first.z * second.y, first.z * second.x - first.x * second.z,
						  first.x * second.y - first.y * second.x};
	return product;
}

/*
 */
static double vector_length(const Vec3 vector) {
	return sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

/*
 */
static Vec3 map_normal(const Vec3 first, const Vec3 second, const Vec3 third) {
	// Preserve GtkRadiant's binary32 subtraction and cross-product rounding.
	const Vec3 first_edge  = {(float)((float)first.x - (float)second.x), (float)((float)first.y - (float)second.y),
							  (float)((float)first.z - (float)second.z)};
	const Vec3 second_edge = {(float)((float)third.x - (float)second.x), (float)((float)third.y - (float)second.y),
							  (float)((float)third.z - (float)second.z)};
	const Vec3 normal	   = {(float)((float)(first_edge.y * second_edge.z) - (float)(first_edge.z * second_edge.y)),
							  (float)((float)(first_edge.z * second_edge.x) - (float)(first_edge.x * second_edge.z)),
							  (float)((float)(first_edge.x * second_edge.y) - (float)(first_edge.y * second_edge.x))};
	return normal;
}

/*
 */
static int texture_matrix(const Vec3 normal, const Vec3 positions[3], const double uv[3][2], double matrix[2][3]) {
	Vec3 n				= normal;
	const double length = vector_length(n);
	if(!(length > 0) || !isfinite(length)) {
		fprintf(stderr, "NILtoMAP: invalid texture projection plane\n");
		return 0;
	}

	n.x = (float)(n.x / length);
	n.y = (float)(n.y / length);
	n.z = (float)(n.z / length);
	if(fabs(n.x) < 1e-6) {
		n.x = 0;
	}

	if(fabs(n.y) < 1e-6) {
		n.y = 0;
	}

	if(fabs(n.z) < 1e-6) {
		n.z = 0;
	}

	// Match Radiant's ComputeAxisBase, including its binary32 basis.
	const float horizontal_squared = (float)((float)(n.y * n.y) + (float)(n.x * n.x));
	const float ry				   = (float)-atan2(n.z, sqrt(horizontal_squared));
	const float rz				   = (float)atan2(n.y, n.x);
	const Vec3 s				   = {(float)-sin(rz), (float)cos(rz), 0};
	const Vec3 t				   = {(float)(-sin(ry) * cos(rz)), (float)(-sin(ry) * sin(rz)), (float)-cos(ry)};
	double projected[3][2];
	for(size_t i = 0; i < 3; ++i) {
		projected[i][0] = positions[i].x * s.x + positions[i].y * s.y + positions[i].z * s.z;
		projected[i][1] = positions[i].x * t.x + positions[i].y * t.y + positions[i].z * t.z;
	}

	const double x1			 = projected[1][0] - projected[0][0];
	const double y1			 = projected[1][1] - projected[0][1];
	const double x2			 = projected[2][0] - projected[0][0];
	const double y2			 = projected[2][1] - projected[0][1];
	const double determinant = x1 * y2 - x2 * y1;
	if(!isfinite(determinant) || determinant == 0) {
		fprintf(stderr, "NILtoMAP: degenerate UV projection geometry\n");
		return 0;
	}

	for(size_t axis = 0; axis < 2; ++axis) {
		const double d1 = uv[1][axis] - uv[0][axis];
		const double d2 = uv[2][axis] - uv[0][axis];
		matrix[axis][0] = (d1 * y2 - d2 * y1) / determinant;
		matrix[axis][1] = (x1 * d2 - x2 * d1) / determinant;
		matrix[axis][2] = uv[0][axis] - matrix[axis][0] * projected[0][0] - matrix[axis][1] * projected[0][1];
		for(size_t field = 0; field < 3; ++field) {
			if(!isfinite(matrix[axis][field]) || fabs(matrix[axis][field]) > FLT_MAX) {
				fprintf(stderr, "NILtoMAP: non-finite or out-of-range texture matrix\n");
				return 0;
			}
		}
	}

	double maximum_error = 0;
	for(size_t i = 0; i < 3; ++i) {
		const float x = (float)((float)((float)positions[i].x * (float)s.x + (float)positions[i].y * (float)s.y) +
								(float)positions[i].z * (float)s.z);
		const float y = (float)((float)((float)positions[i].x * (float)t.x + (float)positions[i].y * (float)t.y) +
								(float)positions[i].z * (float)t.z);
		for(size_t axis = 0; axis < 2; ++axis) {
			const float value =
				(float)((float)((float)matrix[axis][0] * x + (float)matrix[axis][1] * y) + (float)matrix[axis][2]);
			maximum_error = fmax(maximum_error, fabs(value - uv[i][axis]));
		}
	}

	if(maximum_error > 0.001) {
		fprintf(stderr,
				"NILtoMAP: warning: MAP binary32 UV precision loss (maximum %.9g); "
				"near-degenerate or ill-conditioned triangle\n",
				maximum_error);
	}

	return 1;
}

/*
 */
static int write_plane(FILE *const output, Vec3 first, const Vec3 second, Vec3 third, const char *const shader,
					   const int has_legacy_planes, int primitives, const Vec3 positions[3], const double uv[3][2]) {
	if(!has_legacy_planes) {
		Vec3 stored_normal = map_normal(first, second, third);
		if(fmax(fabs(stored_normal.x), fmax(fabs(stored_normal.y), fabs(stored_normal.z))) <=
		   map_rebuild_normal_tolerance) {
			Vec3 normal		 = cross_product(subtract_vector(first, second), subtract_vector(third, second));
			double magnitude = vector_length(normal);
			if(!(magnitude > 0) || !isfinite(magnitude)) {
				fprintf(stderr, "NILtoMAP: cannot construct a stable MAP plane\n");
				return 0;
			}

			normal.x /= magnitude;
			normal.y /= magnitude;
			normal.z /= magnitude;
			Vec3 axis = {0, 0, 0};
			if(fabs(normal.x) <= fabs(normal.y) && fabs(normal.x) <= fabs(normal.z)) {
				axis.x = 1;
			} else if(fabs(normal.y) <= fabs(normal.z)) {
				axis.y = 1;
			} else {
				axis.z = 1;
			}

			Vec3 first_tangent = cross_product(axis, normal);
			magnitude		   = vector_length(first_tangent);
			first_tangent.x /= magnitude;
			first_tangent.y /= magnitude;
			first_tangent.z /= magnitude;
			const Vec3 second_tangent = cross_product(normal, first_tangent);
			const double span =
				fmax(map_plane_minimum_span,
					 fmax(fabs(second.x), fmax(fabs(second.y), fabs(second.z))) * map_plane_relative_span);
			first.x		  = second.x + first_tangent.x * span;
			first.y		  = second.y + first_tangent.y * span;
			first.z		  = second.z + first_tangent.z * span;
			third.x		  = second.x + second_tangent.x * span;
			third.y		  = second.y + second_tangent.y * span;
			third.z		  = second.z + second_tangent.z * span;
			stored_normal = map_normal(first, second, third);
		}

		if(!isfinite(stored_normal.x) || !isfinite(stored_normal.y) || !isfinite(stored_normal.z) ||
		   fmax(fabs(stored_normal.x), fmax(fabs(stored_normal.y), fabs(stored_normal.z))) <= map_normal_tolerance) {
			fprintf(stderr, "NILtoMAP: plane cannot clear GtkRadiant's binary32 normal tolerance\n");
			return 0;
		}
	}

	if(fprintf(output, "( %.17g %.17g %.17g ) ( %.17g %.17g %.17g ) ( %.17g %.17g %.17g ) ", first.x, first.y, first.z,
			   second.x, second.y, second.z, third.x, third.y, third.z) < 0) {
		return 0;
	}

	if(primitives) {
		double matrix[2][3] = {{1, 0, 0}, {0, 1, 0}};
		if(uv && !texture_matrix(map_normal(first, second, third), positions, uv, matrix)) {
			return 0;
		}

		return fprintf(output, "( ( %.17g %.17g %.17g ) ( %.17g %.17g %.17g ) ) %s 0 0 0\n", matrix[0][0], matrix[0][1],
					   matrix[0][2], matrix[1][0], matrix[1][1], matrix[1][2], shader) >= 0;
	}

	return fprintf(output, "%s 0 0 0 1 1 0 0 0\n", shader) >= 0;
}

/*
 */
static int read_triangle_positions(Vec3 positions[NIL_TRIANGLE_VERTEX_COUNT],
								   Vec3 stored_positions[NIL_TRIANGLE_VERTEX_COUNT],
								   const unsigned char *const vertices,
								   const uint16_t vertex_indices[NIL_TRIANGLE_VERTEX_COUNT], const double scale) {
	for(size_t vertex_index = 0; vertex_index < NIL_TRIANGLE_VERTEX_COUNT; ++vertex_index) {
		const unsigned char *const vertex = vertices + (size_t)vertex_indices[vertex_index] * NIL_VERTEX_SIZE;
		positions[vertex_index].x		  = f32(vertex) * scale;
		positions[vertex_index].y		  = f32(vertex + NIL_POSITION_Y_OFFSET) * scale;
		positions[vertex_index].z		  = f32(vertex + NIL_POSITION_Z_OFFSET) * scale;
		if(!isfinite(positions[vertex_index].x) || !isfinite(positions[vertex_index].y) ||
		   !isfinite(positions[vertex_index].z) || fabs(positions[vertex_index].x) > FLT_MAX ||
		   fabs(positions[vertex_index].y) > FLT_MAX || fabs(positions[vertex_index].z) > FLT_MAX) {
			fprintf(stderr, "NILtoMAP: non-finite or out-of-range vertex position\n");
			return 0;
		}

		stored_positions[vertex_index].x = (float)positions[vertex_index].x;
		stored_positions[vertex_index].y = (float)positions[vertex_index].y;
		stored_positions[vertex_index].z = (float)positions[vertex_index].z;
	}

	return 1;
}

/*
 */
static int write_obj_triangle(FILE *const output, const unsigned char *const vertices,
							  const uint16_t vertex_indices[NIL_TRIANGLE_VERTEX_COUNT],
							  const Vec3 positions[NIL_TRIANGLE_VERTEX_COUNT], const char *const shader,
							  Options *const options) {
	if(options->brushes > (size_t)(INT_MAX - NIL_TRIANGLE_VERTEX_COUNT) / NIL_TRIANGLE_VERTEX_COUNT) {
		fprintf(stderr, "NILtoMAP: OBJ vertex count exceeds editor index range\n");
		return 0;
	}

	const size_t base_index = options->brushes * NIL_TRIANGLE_VERTEX_COUNT + 1;
	if(options->resolver && strcmp(options->obj_shader, shader)) {
		const char *const material_name = options->resolver(options->material_context, shader);
		if(!material_name ||
		   fprintf(output, "g nil_surface_%zu\nusemtl %s\n", options->obj_groups++, material_name) < 0) {
			return 0;
		}

		strcpy(options->obj_shader, shader);
	}

	if(fprintf(output, "# native_shader %s\n", shader) < 0) {
		return 0;
	}

	for(size_t vertex_index = 0; vertex_index < NIL_TRIANGLE_VERTEX_COUNT; ++vertex_index) {
		const unsigned char *const vertex = vertices + (size_t)vertex_indices[vertex_index] * NIL_VERTEX_SIZE;
		const float attributes[NIL_OBJ_ATTRIBUTE_COUNT] = {
			f32(vertex + NIL_NORMAL_X_OFFSET), f32(vertex + NIL_NORMAL_Y_OFFSET), f32(vertex + NIL_NORMAL_Z_OFFSET),
			f32(vertex + NIL_UV_U_OFFSET), f32(vertex + NIL_UV_V_OFFSET)};
		for(size_t attribute_index = 0; attribute_index < NIL_OBJ_ATTRIBUTE_COUNT; ++attribute_index) {
			if(!isfinite(attributes[attribute_index])) {
				fprintf(stderr, "NILtoMAP: non-finite OBJ normal or UV attribute\n");
				return 0;
			}
		}

		const double texture_v = options->resolver && attributes[NIL_OBJ_V_ATTRIBUTE]
									 ? -(double)attributes[NIL_OBJ_V_ATTRIBUTE]
									 : (double)attributes[NIL_OBJ_V_ATTRIBUTE];
		if(fprintf(output, "v %.9g %.9g %.9g\nvt %.9g %.9g\nvn %.9g %.9g %.9g\n", positions[vertex_index].x,
				   positions[vertex_index].y, positions[vertex_index].z, (double)attributes[NIL_OBJ_U_ATTRIBUTE],
				   texture_v, (double)attributes[NIL_OBJ_NORMAL_X_ATTRIBUTE],
				   (double)attributes[NIL_OBJ_NORMAL_Y_ATTRIBUTE],
				   (double)attributes[NIL_OBJ_NORMAL_Z_ATTRIBUTE]) < 0) {
			return 0;
		}
	}

	return fprintf(output, "f %zu/%zu/%zu %zu/%zu/%zu %zu/%zu/%zu\n", base_index, base_index, base_index,
				   base_index + 1, base_index + 1, base_index + 1, base_index + NIL_TRIANGLE_LAST_VERTEX,
				   base_index + NIL_TRIANGLE_LAST_VERTEX, base_index + NIL_TRIANGLE_LAST_VERTEX) >= 0;
}

/*
 */
static int extrude_triangle(Vec3 back_positions[NIL_TRIANGLE_VERTEX_COUNT],
							const Vec3 positions[NIL_TRIANGLE_VERTEX_COUNT], const Vec3 normal) {
	for(size_t vertex_index = 0; vertex_index < NIL_TRIANGLE_VERTEX_COUNT; ++vertex_index) {
		back_positions[vertex_index] = subtract_vector(positions[vertex_index], normal);
		if(fabs(back_positions[vertex_index].x) > FLT_MAX || fabs(back_positions[vertex_index].y) > FLT_MAX ||
		   fabs(back_positions[vertex_index].z) > FLT_MAX) {
			fprintf(stderr, "NILtoMAP: extruded position outside MAP float range\n");
			return 0;
		}

		const Vec3 stored_delta = {
			(double)(float)positions[vertex_index].x - (double)(float)back_positions[vertex_index].x,
			(double)(float)positions[vertex_index].y - (double)(float)back_positions[vertex_index].y,
			(double)(float)positions[vertex_index].z - (double)(float)back_positions[vertex_index].z};
		if(stored_delta.x * normal.x + stored_delta.y * normal.y + stored_delta.z * normal.z <= 0) {
			fprintf(stderr,
					"NILtoMAP: extrusion collapses at MAP float precision; increase thickness or adjust scale\n");
			return 0;
		}
	}

	return 1;
}

/*
 */
static int write_triangle_brush(FILE *const output, const Vec3 positions[NIL_TRIANGLE_VERTEX_COUNT],
								const Vec3 stored_positions[NIL_TRIANGLE_VERTEX_COUNT], Vec3 normal,
								const double magnitude, const char *const shader, const Options *const options,
								const double uv[3][2]) {
	const Vec3 stored_normal =
		cross_product(subtract_vector(stored_positions[1], stored_positions[0]),
					  subtract_vector(stored_positions[NIL_TRIANGLE_LAST_VERTEX], stored_positions[0]));
	if(vector_length(stored_normal) <=
	   vector_length(subtract_vector(stored_positions[1], stored_positions[0])) *
		   vector_length(subtract_vector(stored_positions[NIL_TRIANGLE_LAST_VERTEX], stored_positions[0])) *
		   triangle_relative_area_tolerance) {
		fprintf(stderr, "NILtoMAP: triangle collapses at MAP float precision; adjust scale\n");
		return 0;
	}

	normal.x									   = normal.x / magnitude * options->thickness;
	normal.y									   = normal.y / magnitude * options->thickness;
	normal.z									   = normal.z / magnitude * options->thickness;
	Vec3 back_positions[NIL_TRIANGLE_VERTEX_COUNT] = {{0}};
	if(!extrude_triangle(back_positions, positions, normal)) {
		return 0;
	}

	// MAP plane normals use (a - b) cross (c - b), opposite to mesh winding.
	const int primitives = options->output_mode == OUTPUT_UV_BRUSHES;
	if(fputs(primitives ? "{\nbrushDef\n{\n" : "{\n", output) == EOF ||
	   !write_plane(output, positions[0], positions[NIL_TRIANGLE_LAST_VERTEX], positions[1], shader,
					options->has_legacy_planes, primitives, stored_positions, uv) ||
	   !write_plane(output, back_positions[0], back_positions[1], back_positions[NIL_TRIANGLE_LAST_VERTEX],
					caulk_shader, options->has_legacy_planes, primitives, NULL, NULL)) {
		return 0;
	}

	for(size_t vertex_index = 0; vertex_index < NIL_TRIANGLE_VERTEX_COUNT; ++vertex_index) {
		const size_t next_index = (vertex_index + 1) % NIL_TRIANGLE_VERTEX_COUNT;
		if(!write_plane(output, positions[vertex_index], positions[next_index], back_positions[next_index],
						caulk_shader, options->has_legacy_planes, primitives, NULL, NULL)) {
			return 0;
		}
	}

	return fputs(primitives ? "}\n}\n" : "}\n", output) != EOF;
}

/*
 */
static int write_triangle(FILE *const output, const unsigned char *const vertices, const uint32_t vertex_count,
						  const uint16_t first_index, const uint16_t second_index, const uint16_t third_index,
						  const char *const shader, Options *const options) {
	if(first_index >= vertex_count || second_index >= vertex_count || third_index >= vertex_count) {
		fprintf(stderr, "NILtoMAP: triangle index outside vertex buffer (%u, %u, %u; %u vertices)\n",
				(unsigned)first_index, (unsigned)second_index, (unsigned)third_index, (unsigned)vertex_count);
		return 0;
	}

	const uint16_t vertex_indices[NIL_TRIANGLE_VERTEX_COUNT] = {first_index, second_index, third_index};
	Vec3 positions[NIL_TRIANGLE_VERTEX_COUNT]				 = {{0}};
	Vec3 stored_positions[NIL_TRIANGLE_VERTEX_COUNT]		 = {{0}};
	if(!read_triangle_positions(positions, stored_positions, vertices, vertex_indices, options->scale)) {
		return 0;
	}

	const Vec3 normal		  = cross_product(subtract_vector(positions[1], positions[0]),
											  subtract_vector(positions[NIL_TRIANGLE_LAST_VERTEX], positions[0]));
	const double magnitude	  = vector_length(normal);
	const double edge_product = vector_length(subtract_vector(positions[1], positions[0])) *
								vector_length(subtract_vector(positions[NIL_TRIANGLE_LAST_VERTEX], positions[0]));
	if(magnitude <= edge_product * triangle_relative_area_tolerance) {
		++options->degenerate;
		return 1;
	}

	if(options->output_mode == OUTPUT_OBJ) {
		if(!write_obj_triangle(output, vertices, vertex_indices, positions, shader, options)) {
			return 0;
		}
	} else if(options->output_mode == OUTPUT_BRUSHES || options->output_mode == OUTPUT_UV_BRUSHES) {
		double uv[3][2] = {{0}};
		if(options->output_mode == OUTPUT_UV_BRUSHES) {
			for(size_t i = 0; i < 3; ++i) {
				const unsigned char *vertex = vertices + (size_t)vertex_indices[i] * NIL_VERTEX_SIZE;
				uv[i][0]					= f32(vertex + NIL_UV_U_OFFSET);
				uv[i][1]					= f32(vertex + NIL_UV_V_OFFSET);
			}
		}

		if(!write_triangle_brush(output, positions, stored_positions, normal, magnitude, shader, options,
								 options->output_mode == OUTPUT_UV_BRUSHES ? uv : NULL)) {
			return 0;
		}
	}

	++options->brushes;
	return 1;
}

/*
 */
static int read_material_name(const unsigned char *const name, char shader[NIL_SHADER_TOKEN_SIZE]) {
	size_t name_length = 0;
	for(; name_length < NIL_SHADER_NAME_SIZE && name[name_length]; ++name_length) {
		const unsigned char character = name[name_length];
		if(!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			 (character >= '0' && character <= '9') || character == '_' || character == '-' || character == '.' ||
			 character == '/' || character == '\\')) {
			fprintf(stderr, "NILtoMAP: material contains an unsupported MAP token byte (0x%02x)\n",
					(unsigned)character);
			return 0;
		}

		shader[name_length] = character == '\\' ? '/' : (char)character;
	}

	shader[name_length]		   = '\0';
	const size_t prefix_length = sizeof(texture_prefix) - 1;
	if(strncmp(shader, texture_prefix, prefix_length) == 0) {
		memmove(shader, shader + prefix_length, name_length - prefix_length + 1);
		name_length -= prefix_length;
	}

	if(name_length == 0) {
		fprintf(stderr, "NILtoMAP: empty material name\n");
		return 0;
	}

	if(strstr(shader, "//")) {
		fprintf(stderr, "NILtoMAP: material name contains a MAP comment delimiter\n");
		return 0;
	}

	return 1;
}

/*
 */
static int write_sector_metadata(FILE *const sectors, const unsigned char *const header, const uint32_t sector_index) {
	if(!sectors) {
		return 1;
	}

	if(!isfinite(f32(header + NIL_SECTOR_CENTER_X_OFFSET)) || !isfinite(f32(header + NIL_SECTOR_CENTER_Y_OFFSET)) ||
	   !isfinite(f32(header + NIL_SECTOR_CENTER_Z_OFFSET))) {
		fprintf(stderr, "NILtoMAP: non-finite sector center in sector %u\n", (unsigned)sector_index);
		return 0;
	}

	return fprintf(sectors,
				   "{\n\"classname\" \"nod_nil_sector\"\n\"nod_id\" \"sector:%u\"\n"
				   "\"origin\" \"%.9g %.9g %.9g\"\n\"nod_flags\" \"%u\"\n"
				   "\"nod_portals\" \"%u\"\n}\n",
				   (unsigned)sector_index, f32(header + NIL_SECTOR_CENTER_X_OFFSET),
				   f32(header + NIL_SECTOR_CENTER_Y_OFFSET), f32(header + NIL_SECTOR_CENTER_Z_OFFSET),
				   (unsigned)u32(header), (unsigned)header[NIL_SECTOR_PORTAL_COUNT_OFFSET]) >= 0;
}

/*
 */
static int read_vertex_streams(Reader *const reader, SectorMesh *const mesh, const uint8_t geometry_type,
							   const uint32_t sector_index) {
	if(geometry_type != NIL_GEOMETRY_NONE) {
		const unsigned char *const vertex_type_record = read_records(reader, 1, 1);
		if(!vertex_type_record) {
			return 0;
		}

		mesh->vertex_type = vertex_type_record[0];
		if(mesh->vertex_type > NIL_VERTEX_EXTENDED) {
			fprintf(stderr, "NILtoMAP: unsupported vertex type %u in sector %u\n", (unsigned)mesh->vertex_type,
					(unsigned)sector_index);
			return 0;
		}
	}

	const unsigned char *const vertex_header = read_records(reader, 1, NIL_VERTEX_HEADER_SIZE);
	if(!vertex_header) {
		return 0;
	}

	mesh->vertex_count = u16(vertex_header);
	mesh->vertices	   = read_records(reader, mesh->vertex_count, NIL_VERTEX_SIZE);
	if(!mesh->vertices ||
	   (mesh->vertex_type == NIL_VERTEX_DUAL && !read_records(reader, mesh->vertex_count, NIL_VERTEX_SIZE))) {
		return 0;
	}

	if(geometry_type == NIL_GEOMETRY_SECONDARY) {
		const unsigned char *const secondary_header = read_records(reader, 1, NIL_SECONDARY_HEADER_SIZE);
		if(!secondary_header) {
			return 0;
		}

		if(secondary_header[0] != NIL_SECONDARY_STREAM_TYPE) {
			fprintf(stderr, "NILtoMAP: unsupported secondary stream type %u in sector %u\n",
					(unsigned)secondary_header[0], (unsigned)sector_index);
			return 0;
		}

		mesh->secondary_count = u32(secondary_header + NIL_SECONDARY_COUNT_OFFSET);
		// Secondary positions are a 36-byte stream followed by 36-byte TBNs.
		mesh->secondary = read_records(reader, mesh->secondary_count, NIL_SECONDARY_RECORD_SIZE);
		if(!mesh->secondary) {
			return 0;
		}
	}

	return 1;
}

/*
 */
static int read_sector_mesh(Reader *const reader, SectorMesh *const mesh, FILE *const sectors,
							const uint32_t sector_index) {
	const unsigned char *const header = read_records(reader, 1, NIL_SECTOR_HEADER_SIZE);
	if(!header || !write_sector_metadata(sectors, header, sector_index)) {
		return 0;
	}

	if(!read_records(reader, header[NIL_SECTOR_PORTAL_COUNT_OFFSET], NIL_PORTAL_SIZE)) {
		return 0;
	}

	const unsigned char *const geometry_record = read_records(reader, 1, 1);
	if(!geometry_record) {
		return 0;
	}

	const uint8_t geometry_type = geometry_record[0];
	if(geometry_type > NIL_GEOMETRY_SECONDARY) {
		fprintf(stderr, "NILtoMAP: unsupported geometry type %u in sector %u\n", (unsigned)geometry_type,
				(unsigned)sector_index);
		return 0;
	}

	if(!read_vertex_streams(reader, mesh, geometry_type, sector_index)) {
		return 0;
	}

	const unsigned char *const group_count_record = read_records(reader, 1, NIL_U16_SIZE);
	if(!group_count_record) {
		return 0;
	}

	mesh->group_count = u16(group_count_record);
	mesh->groups	  = read_records(reader, mesh->group_count, NIL_GROUP_SIZE);
	if(!mesh->groups) {
		return 0;
	}

	if(geometry_type != NIL_GEOMETRY_NONE) {
		const unsigned char *const index_count_record = read_records(reader, 1, NIL_U32_SIZE);
		if(!index_count_record) {
			return 0;
		}

		mesh->index_count = u32(index_count_record);
	}

	mesh->indices = read_records(reader, mesh->index_count, NIL_U16_SIZE);
	return mesh->indices != NULL;
}

/*
 */
static int write_surface_group(FILE *const output, const SectorMesh *const mesh, const unsigned char *const shaders,
							   const uint32_t shader_count, const uint16_t group_index, const uint32_t sector_index,
							   Options *const options) {
	const unsigned char *const group = mesh->groups + (size_t)group_index * NIL_GROUP_SIZE;
	const uint32_t shader_index		 = u32(group);
	const uint32_t base_index		 = u32(group + NIL_GROUP_BASE_OFFSET);
	const uint32_t strip_count		 = u16(group + NIL_GROUP_STRIP_COUNT_OFFSET);
	const uint32_t list_count		 = u16(group + NIL_GROUP_LIST_COUNT_OFFSET);
	if(shader_index >= shader_count || base_index > mesh->index_count ||
	   strip_count + list_count > mesh->index_count - base_index || list_count % NIL_TRIANGLE_VERTEX_COUNT) {
		fprintf(stderr, "NILtoMAP: invalid surface group %u in sector %u\n", (unsigned)group_index,
				(unsigned)sector_index);
		return 0;
	}

	char shader[NIL_SHADER_TOKEN_SIZE] = {0};
	if(!read_material_name(shaders + (size_t)shader_index * NIL_SHADER_NAME_SIZE, shader)) {
		return 0;
	}

	const unsigned char *group_vertices = mesh->vertices;
	uint32_t group_vertex_count			= mesh->vertex_count;
	if(group[NIL_GROUP_VERTEX_POOL_OFFSET] == NIL_GROUP_SECONDARY_POOL && mesh->vertex_type != NIL_VERTEX_DUAL) {
		if(!mesh->secondary) {
			fprintf(stderr, "NILtoMAP: bumped group has no matching vertex pool in sector %u\n",
					(unsigned)sector_index);
			return 0;
		}

		group_vertices	   = mesh->secondary;
		group_vertex_count = mesh->secondary_count;
	}

	uint32_t strip_run		 = 0;
	uint16_t previous_first	 = 0;
	uint16_t previous_second = 0;
	for(uint32_t index_offset = 0; index_offset < strip_count; ++index_offset) {
		const uint16_t current_index = u16(mesh->indices + ((size_t)base_index + index_offset) * NIL_U16_SIZE);
		if(current_index == UINT16_MAX) {
			strip_run = 0;
			continue;
		}

		if(current_index >= group_vertex_count) {
			fprintf(stderr, "NILtoMAP: strip index %u outside %u vertices in sector %u\n", (unsigned)current_index,
					(unsigned)group_vertex_count, (unsigned)sector_index);
			return 0;
		}

		if(strip_run >= NIL_TRIANGLE_LAST_VERTEX &&
		   !write_triangle(output, group_vertices, group_vertex_count,
						   (strip_run & 1) ? previous_second : previous_first,
						   (strip_run & 1) ? previous_first : previous_second, current_index, shader, options)) {
			return 0;
		}

		previous_first	= previous_second;
		previous_second = current_index;
		++strip_run;
	}

	for(uint32_t index_offset = 0; index_offset < list_count; index_offset += NIL_TRIANGLE_VERTEX_COUNT) {
		const unsigned char *const triangle_indices =
			mesh->indices + ((size_t)base_index + strip_count + index_offset) * NIL_U16_SIZE;
		if(!write_triangle(output, group_vertices, group_vertex_count, u16(triangle_indices),
						   u16(triangle_indices + NIL_U16_SIZE),
						   u16(triangle_indices + NIL_TRIANGLE_LAST_VERTEX * NIL_U16_SIZE), shader, options)) {
			return 0;
		}
	}

	return 1;
}

/*
 */
static int skip_sector_trailer(Reader *const reader) {
	const unsigned char *const record_count = read_records(reader, 1, 1);
	if(!record_count || !read_records(reader, record_count[0], NIL_TRAILING_RECORD_SIZE)) {
		return 0;
	}

	const unsigned char *const tbn_count = read_records(reader, 1, NIL_U16_SIZE);
	if(!tbn_count || !read_records(reader, u16(tbn_count), NIL_TRAILING_TBN_SIZE)) {
		return 0;
	}

	const unsigned char *const name_count = read_records(reader, 1, NIL_U16_SIZE);
	return name_count && read_records(reader, u16(name_count), NIL_TRAILING_NAME_SIZE) &&
		   read_records(reader, 1, NIL_SECTOR_FOOTER_SIZE);
}

/*
 */
static int decompile(Reader *const reader, FILE *const output, Options *const options) {
	const unsigned char *const header = read_records(reader, 1, NIL_HEADER_SIZE);
	if(!header) {
		return 0;
	}

	if(memcmp(header, "NIL\x10", NIL_MAGIC_SIZE) != 0 || u32(header + NIL_VERSION_OFFSET) != NIL_VERSION) {
		fprintf(stderr, "NILtoMAP: expected NIL magic 0x104c494e and version 35\n");
		return 0;
	}

	const uint32_t shader_count		   = u32(header + NIL_SHADER_COUNT_OFFSET);
	const unsigned char *const shaders = read_records(reader, shader_count, NIL_SHADER_NAME_SIZE);
	if(!shaders) {
		return 0;
	}

	const unsigned char *const extra_shader_count = read_records(reader, 1, NIL_U16_SIZE);
	if(!extra_shader_count || !read_records(reader, u16(extra_shader_count), NIL_SHADER_NAME_SIZE)) {
		return 0;
	}

	const unsigned char *const sector_count_record = read_records(reader, 1, NIL_U32_SIZE);
	if(!sector_count_record) {
		return 0;
	}

	const uint32_t sector_count = u32(sector_count_record);
	if(fputs(options->output_mode == OUTPUT_OBJ
				 ? "# NIL native XYZ render mesh\n"
				 : "// NILtoMAP: source-preserving render preview\n{\n\"classname\" \"worldspawn\"\n",
			 output) == EOF) {
		return 0;
	}

	if(options->output_mode == OUTPUT_UV_BRUSHES && fputs("\"nod_uv_mapping\" \"brush-primitives\"\n", output) == EOF) {
		return 0;
	}

	if(options->output_mode == OUTPUT_OBJ && !options->resolver &&
	   fputs("g nil_world\nusemtl textures/nodpreview/white\n", output) == EOF) {
		return 0;
	}

	for(uint32_t sector_index = 0; sector_index < sector_count; ++sector_index) {
		SectorMesh mesh = {0};
		if(!read_sector_mesh(reader, &mesh, options->sectors, sector_index)) {
			return 0;
		}

		for(uint16_t group_index = 0; group_index < mesh.group_count; ++group_index) {
			if(!write_surface_group(output, &mesh, shaders, shader_count, group_index, sector_index, options)) {
				return 0;
			}
		}

		if(!skip_sector_trailer(reader)) {
			return 0;
		}
	}

	if(reader->offset != reader->size) {
		fprintf(stderr, "NILtoMAP: warning: ignoring %zu bytes of undecoded post-sector data\n",
				reader->size - reader->offset);
	}

	return options->output_mode == OUTPUT_OBJ || fputs("}\n", output) != EOF;
}

/*
 */
int niltomap_write(const unsigned char *const data, const size_t data_size, FILE *const output, const double scale,
				   const double thickness, FILE *const sectors) {
	return niltomap_write_stats(data, data_size, output, scale, thickness, sectors, NULL, NULL);
}

/*
 */
static int write_stats_mode(const unsigned char *const data, const size_t data_size, FILE *const output,
							const double scale, const double thickness, FILE *const sectors, size_t *const brushes,
							size_t *const degenerate, OutputMode mode) {
	Reader reader			= {data, data_size, 0};
	Options options			= {.scale = scale, .thickness = thickness, .sectors = sectors, .output_mode = mode};
	const int is_successful = decompile(&reader, output, &options);
	if(brushes) {
		*brushes = options.brushes;
	}

	if(degenerate) {
		*degenerate = options.degenerate;
	}

	return is_successful;
}

/*
 */
int niltomap_write_stats(const unsigned char *data, size_t size, FILE *output, double scale, double thickness,
						 FILE *sectors, size_t *brushes, size_t *degenerate) {
	return write_stats_mode(data, size, output, scale, thickness, sectors, brushes, degenerate, OUTPUT_BRUSHES);
}

/*
 */
int niltomap_write_uv_stats(const unsigned char *data, size_t size, FILE *output, double scale, double thickness,
							FILE *sectors, size_t *brushes, size_t *degenerate) {
	return write_stats_mode(data, size, output, scale, thickness, sectors, brushes, degenerate, OUTPUT_UV_BRUSHES);
}

/*
 */
int niltomap_write_legacy(const unsigned char *const data, const size_t data_size, FILE *const output,
						  const double scale, const double thickness, FILE *const sectors) {
	Reader reader	= {data, data_size, 0};
	Options options = {.scale			  = scale,
					   .thickness		  = thickness,
					   .sectors			  = sectors,
					   .has_legacy_planes = 1,
					   .output_mode		  = OUTPUT_BRUSHES};
	return decompile(&reader, output, &options);
}

/*
 */
int niltomap_write_metadata(const unsigned char *const data, const size_t data_size, FILE *const output,
							FILE *const sectors, size_t *const triangles) {
	Reader reader			= {data, data_size, 0};
	Options options			= {.scale = 1, .thickness = 1, .sectors = sectors, .output_mode = OUTPUT_METADATA};
	const int is_successful = decompile(&reader, output, &options);
	if(triangles) {
		*triangles = options.brushes;
	}

	return is_successful;
}

/*
 */
int niltomap_write_obj(const unsigned char *const data, const size_t data_size, FILE *const output,
					   size_t *const triangles, const NilMaterialResolver resolver, void *const context) {
	Reader reader	= {data, data_size, 0};
	Options options = {
		.scale = 1, .thickness = 1, .output_mode = OUTPUT_OBJ, .resolver = resolver, .material_context = context};
	const int is_successful = decompile(&reader, output, &options);
	if(triangles) {
		*triangles = options.brushes;
	}

	return is_successful;
}

/*
 */
static int write_triangle_with_uv(FILE *output, const double coordinates[3][3], const double uv[3][2],
								  const char *shader, double thickness, int *written) {
	Vec3 positions[3];
	Vec3 stored[3];
	Options options = {.scale = 1, .thickness = thickness, .output_mode = uv ? OUTPUT_UV_BRUSHES : OUTPUT_BRUSHES};
	*written		= 0;
	if(!isfinite(thickness) || thickness <= 0 || !*shader || strstr(shader, "//")) {
		fprintf(stderr, "NILtoMAP: invalid triangle thickness or material\n");
		return 0;
	}

	for(const unsigned char *p = (const unsigned char *)shader; *p; ++p) {
		if(*p <= 32 || *p >= 127 || strchr("(){}\"", *p)) {
			fprintf(stderr, "NILtoMAP: invalid triangle material token\n");
			return 0;
		}
	}

	for(size_t i = 0; i < 3; ++i) {
		for(size_t axis = 0; axis < 3; ++axis) {
			if(!isfinite(coordinates[i][axis]) || fabs(coordinates[i][axis]) > FLT_MAX) {
				fprintf(stderr, "NILtoMAP: invalid triangle position\n");
				return 0;
			}
		}

		positions[i] = (Vec3){coordinates[i][0], coordinates[i][1], coordinates[i][2]};
		stored[i]	 = (Vec3){(float)positions[i].x, (float)positions[i].y, (float)positions[i].z};
	}

	const Vec3 first	   = subtract_vector(positions[1], positions[0]);
	const Vec3 second	   = subtract_vector(positions[2], positions[0]);
	const Vec3 normal	   = cross_product(first, second);
	const double magnitude = vector_length(normal);
	if(magnitude <= vector_length(first) * vector_length(second) * triangle_relative_area_tolerance) {
		return 1;
	}

	if(!write_triangle_brush(output, positions, stored, normal, magnitude,
							 !strncmp(shader, texture_prefix, strlen(texture_prefix)) ? shader + strlen(texture_prefix)
																					  : shader,
							 &options, uv)) {
		return 0;
	}

	*written = 1;
	return 1;
}

/*
 */
int niltomap_write_triangle(FILE *output, const double coordinates[3][3], const char *shader, double thickness,
							int *written) {
	return write_triangle_with_uv(output, coordinates, NULL, shader, thickness, written);
}

/*
 */
int niltomap_write_triangle_uv(FILE *output, const double coordinates[3][3], const double uv[3][2], const char *shader,
							   double thickness, int *written) {
	if(!uv) {
		*written = 0;
		fprintf(stderr, "NILtoMAP: missing triangle UV coordinates\n");
		return 0;
	}

	return write_triangle_with_uv(output, coordinates, uv, shader, thickness, written);
}
