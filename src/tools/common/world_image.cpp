/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_image.cpp
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

#include "world_image.h"
#include "world_binary.h"

#include <cmath>
#include <limits>

/*
 */
namespace nod::world {

/*
 */
bool build_world_image(const Geometry &geometry, const RenderGeometry &render,
					   const std::array<float, 6> &physics_bounds, WorldImage &image, std::string &error) {
	error.clear();
	if(render.materials.size() > 256) {
		error = "World image supports at most 256 shaders";
		return false;
	}

	SectorData sector;
	if(!build_sector(geometry, render, sector, error)) {
		return false;
	}

	for(size_t axis = 0; axis < 3; ++axis) {
		const float low	 = physics_bounds[axis];
		const float high = physics_bounds[axis + 3];
		if(!std::isfinite(low) || !std::isfinite(high) || !(low < sector.low[axis] && high > sector.high[axis]) ||
		   double(high) - low > std::numeric_limits<float>::max()) {
			error = "Physics bounds must finitely enclose the world";
			return false;
		}
	}

	for(const auto &vertex : geometry.mesh.vertices) {
		for(size_t axis = 0; axis < 3; ++axis) {
			if(!std::isfinite(vertex[axis]) || vertex[axis] < sector.low[axis] || vertex[axis] > sector.high[axis]) {
				error = "Collision vertex outside world bounds";
				return false;
			}
		}
	}

	mopp::Code verified;
	if(!mopp::compile(geometry.mesh, verified, error)) {
		return false;
	}

	if(verified.info != geometry.collision_code.info || verified.bytes != geometry.collision_code.bytes) {
		error = "Collision program does not match geometry";
		return false;
	}

	detail::Writer writer;
	writer.word(0x104c494e);
	writer.word(35);
	for(uint8_t byte : render.global_light) {
		writer.byte(byte);
	}

	for(float component : render.ambient_light) {
		if(!std::isfinite(component) || component < 0) {
			error = "Invalid world ambient lighting";
			return false;
		}

		writer.real(component);
	}

	writer.word(static_cast<uint32_t>(render.materials.size()));
	for(const auto &material : render.materials) {
		const size_t start = writer.bytes.size();
		for(const char *c = material.shader; *c; ++c) {
			writer.byte(static_cast<uint8_t>(*c));
		}

		writer.bytes.resize(start + 32);
	}

	writer.half(0);
	writer.word(1);
	writer.append(sector.bytes);
	writer.half(0);
	writer.half(0);
	for(float bound : physics_bounds) {
		writer.real(bound);
	}

	writer.half(1);
	writer.byte(1);
	writer.half(static_cast<uint16_t>(geometry.mesh.vertices.size()));
	for(const auto &vertex : geometry.mesh.vertices) {
		writer.vector(vertex);
	}

	writer.word(static_cast<uint32_t>(geometry.mesh.indices.size()));
	for(uint16_t index : geometry.mesh.indices) {
		writer.half(index);
	}

	writer.half(0); // No custom sound-material ranges.
	for(float value : verified.info) {
		writer.real(value);
	}

	writer.word(static_cast<uint32_t>(verified.bytes.size()));
	writer.append(verified.bytes);
	writer.half(1); // Sector-owned rigid body.
	writer.half(0);
	writer.byte(0);
	writer.word(sector.collision_mapping.present);
	for(const auto &group : sector.collision_mapping.groups) {
		writer.word(group.present);
		writer.word(group.offset);
	}

	WorldImage result;
	result.bytes			 = std::move(writer.bytes);
	result.excluded_entities = geometry.excluded_entities;
	image					 = std::move(result);
	return true;
}

} // namespace nod::world
