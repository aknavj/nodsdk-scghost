/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

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

#include "NOCParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
NOCParser::~NOCParser() {

	free(m.vertex_data);
	free(m.indices);
	free(m.raw_data);
}

/*
 */
int NOCParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data) return 0;

	// minimum header size: 88 bytes (0x58)
	if(file_size < 0x58) {
		free(data);
		return 0;
	}

	m.raw_data		= data;
	m.raw_data_size = file_size;
	m.file_size		= file_size;

	// header (88 bytes)
	m.version = FileUtils::readUInt32(data, 0x00);
	if(m.version != 10) {
		printf("[NOC] %s: unexpected version %u (expected 10)\n", filePath, m.version);
		// continue anyway — best effort
	}

	m.flags[0] = data[4];
	m.flags[1] = data[5];
	m.flags[2] = data[6];
	m.flags[3] = data[7];

	m.bounds_min.x = FileUtils::readFloat(data, 0x0C);
	m.bounds_min.y = FileUtils::readFloat(data, 0x10);
	m.bounds_min.z = FileUtils::readFloat(data, 0x14);
	m.bounds_max.x = FileUtils::readFloat(data, 0x18);
	m.bounds_max.y = FileUtils::readFloat(data, 0x1C);
	m.bounds_max.z = FileUtils::readFloat(data, 0x20);

	m.num_vertices = FileUtils::readUInt32(data, 0x28);
	m.num_indices  = FileUtils::readUInt32(data, 0x44);

	m.shape_count = m.flags[3];
	if(m.shape_count == 0) m.shape_count = 1;
	if(m.shape_count > MAX_SHAPES) m.shape_count = MAX_SHAPES;

	// determine vertex data offset
	// flags[0]=1: first shape descriptor is 100 bytes -> vertex start at 0xBC
	// flags[0]=2: first shape descriptor is 132 bytes -> vertex start at 0xDC
	// footer: 64 + (shape_count-1) * 56 bytes
	uint32_t shapeDescSize = (m.flags[0] == 2) ? 132 : 100;
	size_t vertexStart	   = 0x58 + shapeDescSize;

	size_t vertexDataBytes = (size_t)m.num_vertices * VERTEX_STRIDE;
	size_t indexDataBytes  = (size_t)m.num_indices * 2;

	if(vertexStart + vertexDataBytes + indexDataBytes > file_size) {
		printf("[NOC] %s: data overflow (vertStart=0x%zX verts=%u idx=%u file_size=%zu)\n", filePath, vertexStart,
			   m.num_vertices, m.num_indices, file_size);
		return 0;
	}

	// parse first shape descriptor
	size_t off = 0x58;
	off += 4;

	// shape name (32 bytes)
	memcpy(m.shapes[0].name, data + off, 32);
	m.shapes[0].name[32] = '\0';
	off += 32;

	// if flags[0]=2, second name (collision type, 32 bytes)
	if(m.flags[0] == 2) {
		memcpy(m.shapes[0].collision_type, data + off, 32);
		m.shapes[0].collision_type[32] = '\0';
		off += 32;
	} else {
		// for flags[0]=1, the name IS the collision type
		strncpy(m.shapes[0].collision_type, m.shapes[0].name, 32);
		m.shapes[0].collision_type[32] = '\0';
	}

	off += 12;

	// 3x3 rotation matrix (36 bytes = 9 floats)
	if(off + 36 <= vertexStart) {
		for(int i = 0; i < 9; ++i) m.shapes[0].matrix[i] = FileUtils::readFloat(data, off + i * 4);
		off += 36;
	}

	// position (12 bytes = 3 floats)
	if(off + 12 <= vertexStart) {
		m.shapes[0].position[0] = FileUtils::readFloat(data, off);
		m.shapes[0].position[1] = FileUtils::readFloat(data, off + 4);
		m.shapes[0].position[2] = FileUtils::readFloat(data, off + 8);
		off += 12;
	}

	// color RGBA (4 bytes)
	if(off + 4 <= vertexStart) {
		memcpy(m.shapes[0].color, data + off, 4);
		off += 4;
	}

	// vertex data
	m.vertex_data = (float *)malloc(vertexDataBytes);
	if(!m.vertex_data) return 0;
	memcpy(m.vertex_data, data + vertexStart, vertexDataBytes);

	// index data
	size_t indexStart = vertexStart + vertexDataBytes;
	m.indices		  = (uint16_t *)malloc(indexDataBytes);
	if(!m.indices) return 0;
	memcpy(m.indices, data + indexStart, indexDataBytes);

	// footer: additional shape descriptors (56 bytes each)
	size_t footerStart	 = indexStart + indexDataBytes;
	size_t baseFooter	 = 64;
	size_t extraShapeOff = footerStart + baseFooter;

	for(uint32_t s = 1; s < m.shape_count && s < MAX_SHAPES; ++s) {
		size_t soff = extraShapeOff + (s - 1) * 56;
		if(soff + 56 > file_size) break;

	// extra shape descriptor: 56 bytes
	// layout: name(32B) + matrix subset or position data + color
	// exact layout not fully decoded; read name and position at minimum
		memcpy(m.shapes[s].name, data + soff, 32);
		m.shapes[s].name[32] = '\0';
		strncpy(m.shapes[s].collision_type, "physclip", 32);
		m.shapes[s].collision_type[32] = '\0';

		// read last 12 bytes as position (approximate)
		if(soff + 56 <= file_size) {
			m.shapes[s].position[0] = FileUtils::readFloat(data, soff + 40);
			m.shapes[s].position[1] = FileUtils::readFloat(data, soff + 44);
			m.shapes[s].position[2] = FileUtils::readFloat(data, soff + 48);
			memcpy(m.shapes[s].color, data + soff + 52, 4);
		}

		memset(m.shapes[s].matrix, 0, sizeof(m.shapes[s].matrix));
		m.shapes[s].matrix[0] = 1.0f;
		m.shapes[s].matrix[4] = 1.0f;
		m.shapes[s].matrix[8] = 1.0f;
	}

	printf("[NOC] %s: v%u, flags=[%u,%u,%u,%u], %u shapes, %u verts, %u indices, "
		   "bbox=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)\n",
		   filePath, m.version, m.flags[0], m.flags[1], m.flags[2], m.flags[3], m.shape_count, m.num_vertices,
		   m.num_indices, m.bounds_min.x, m.bounds_min.y, m.bounds_min.z, m.bounds_max.x, m.bounds_max.y,
		   m.bounds_max.z);

	return 1;
}

/*
 */
int NOCParser::serialize(const char *filePath) const {

	if(!m.raw_data || m.raw_data_size == 0) return 0;
	return FileUtils::writeBinaryFile(filePath, m.raw_data, m.raw_data_size);
}
