/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_sector.cpp
Version: 0.6.8

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

#include "world_sector.h"
#include "world_binary.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

/*
 */
namespace nod::world {
namespace {
	
/*
 */
bool reject(std::string &error, const std::string &reason) {
	error = reason;
	return false;
}

/*
 */
bool finite(const std::array<float, 3> &value) {
	return std::all_of(value.begin(), value.end(), [](float x) { return std::isfinite(x); });
}
} // namespace

/*
 */
bool build_sector(const Geometry &geometry, const RenderGeometry &render, SectorData &sector, std::string &error) {
	error.clear();
	const auto &mesh = geometry.mesh;
	if(render.vertices.empty() || render.vertices.size() > 65535 || render.indices.empty() ||
	   render.indices.size() % 3 || render.indices.size() / 3 > 4096 || render.indices.size() != mesh.indices.size() ||
	   mesh.vertices.empty() || mesh.vertices.size() > 65535 || render.surfaces.empty() ||
	   render.surfaces.size() != geometry.surfaces.size() || render.surfaces.size() > 4096 ||
	   render.materials.empty() || render.materials.size() > 4096 || render.static_lights.size() % 72 ||
	   render.static_lights.size() / 72 > 64) {
		return reject(error, "Invalid sector geometry limits");
	}

	std::set<std::string> names;
	for(const auto &material : render.materials) {
		const char *end = static_cast<const char *>(std::memchr(material.shader, 0, sizeof(material.shader)));
		if(!end || end == material.shader) {
			return reject(error, "Invalid native shader name");
		}

		std::string key;
		for(const char *c = material.shader; c != end; ++c) {
			const unsigned char value = static_cast<unsigned char>(*c);
			if(value <= 32 || value > 126) {
				return reject(error, "Invalid native shader name");
			}

			key += static_cast<char>(value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value);
		}

		if(!names.insert(key).second) {
			return reject(error, "Duplicate native shader binding");
		}

	}

	SectorData result;
	result.low = result.high = render.vertices.front().position;
	for(const auto &vertex : render.vertices) {
		if(!finite(vertex.position) || !finite(vertex.normal) || !std::isfinite(vertex.uv[0]) ||
		   !std::isfinite(vertex.uv[1])) {
			return reject(error, "Nonfinite sector vertex");
		}

		double length = 0;
		for(float component : vertex.normal) {
			length += double(component) * component;
		}

		if(std::abs(length - 1) > 0.0001) {
			return reject(error, "Invalid sector normal");
		}

		for(size_t axis = 0; axis < 3; ++axis) {
			result.low[axis]  = std::min(result.low[axis], vertex.position[axis]);
			result.high[axis] = std::max(result.high[axis], vertex.position[axis]);
		}
	}

	if(result.low != geometry.low || result.high != geometry.high) {
		return reject(error, "Render/collision bounds differ");
	}

	for(size_t axis = 0; axis < 3; ++axis) {
		result.center[axis] = static_cast<float>((double(result.low[axis]) + result.high[axis]) / 2);
	}

	size_t next = 0;
	for(size_t i = 0; i < render.surfaces.size(); ++i) {
		const auto &surface		  = render.surfaces[i];
		const auto &source		  = geometry.surfaces[i];
		const std::string context = "Surface " + std::to_string(i) + ": ";
		if(source.flags != std::array<uint32_t, 3>{}) {
			return reject(error, context + "Authoring flags are not supported yet");
		}

		if(surface.material >= render.materials.size() || surface.first_index != next || surface.index_count == 0 ||
		   surface.index_count % 3 || next > render.indices.size() ||
		   surface.index_count > render.indices.size() - next || source.first_triangle != next / 3 ||
		   source.triangle_count != surface.index_count / 3) {
			return reject(error, context + "Invalid surface span or material");
		}

		if(result.groups.empty() || result.groups.back().material != surface.material) {
			result.groups.push_back({surface.material, surface.first_index, 0, {}, {}});
			result.collision_mapping.groups.push_back({1, static_cast<uint32_t>(next / 3)});
		}

		auto &group = result.groups.back();
		for(size_t j = next; j < next + surface.index_count; ++j) {
			const uint16_t render_key	 = render.indices[j];
			const uint16_t collision_key = mesh.indices[j];
			if(render_key >= render.vertices.size() || collision_key >= mesh.vertices.size()) {
				return reject(error, context + "Index outside vertex pool");
			}

			const auto &position = render.vertices[render_key].position;
			if(position != mesh.vertices[collision_key]) {
				return reject(error, context + "Render/collision triangles differ");
			}

			if(!group.index_count && j == next) {
				group.low = group.high = position;
			}

			for(size_t axis = 0; axis < 3; ++axis) {
				group.low[axis]	 = std::min(group.low[axis], position[axis]);
				group.high[axis] = std::max(group.high[axis], position[axis]);
			}
		}

		group.index_count = static_cast<uint16_t>(group.index_count + surface.index_count);
		next += surface.index_count;
	}

	if(next != render.indices.size()) {
		return reject(error, "Unmapped sector indices");
	}

	result.collision_mapping.present = 1;
	detail::Writer writer;
	// Observed baseline: standard sector flags 5, opaque tail (0, 1).
	writer.word(render.global_light[4] == 2 ? 5 | 0x20 : 5);
	writer.vector(result.center);
	writer.byte(0); // No boundary planes: one unbounded sector.
	writer.byte(1); // One primary vertex pool.
	writer.byte(0);
	writer.word(static_cast<uint32_t>(render.vertices.size()));
	for(const auto &vertex : render.vertices) {
		writer.vector(vertex.position);
		writer.vector(vertex.normal);
		writer.word(vertex.diffuse);
		for(float value : vertex.uv) {
			writer.real(value);
		}
	}

	writer.half(static_cast<uint16_t>(result.groups.size()));
	for(const auto &group : result.groups) {
		writer.word(group.material);
		writer.word(group.first_index);
		writer.word(0);
		writer.half(0);
		writer.half(group.index_count);
		writer.word(0);
		writer.half(0); // Render flags, not a vertex-format field.
		writer.byte(0); // Primary vertex pool.
		writer.vector(group.low);
		writer.vector(group.high);
	}

	writer.word(static_cast<uint32_t>(render.indices.size()));
	for(uint16_t index : render.indices) {
		writer.half(index);
	}

	writer.byte(0);
	writer.half(static_cast<uint16_t>(render.static_lights.size() / 72));
	writer.append(render.static_lights);
	writer.half(0);
	writer.half(0);
	writer.word(1);
	result.bytes = std::move(writer.bytes);
	sector		 = std::move(result);
	return true;
}

} // namespace nod::world
