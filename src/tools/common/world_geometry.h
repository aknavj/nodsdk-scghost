/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_geometry.h
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

#ifndef __WORLD_GEOMETRY_H__
#define __WORLD_GEOMETRY_H__

#include "mopp_compiler.h"
#include <cstddef>

// forward declaration
namespace nod_scene {
	struct Map;
}

/*
 */
namespace nod::world {

/*
 */
struct Surface {
		size_t brush = 0;
		std::string material;
		std::array<float, 3> normal{};
		// Source plane evaluated with Radiant's float math, for UV-axis selection.
		std::array<float, 3> texture_normal{};
		std::array<float, 5> texture{};
		std::array<uint32_t, 3> flags{};
		std::vector<uint16_t> winding;
		uint32_t first_triangle = 0;
		uint32_t triangle_count = 0;
};

/*
 */
struct Geometry {
		mopp::Mesh mesh;
		mopp::Code collision_code;
		std::vector<Surface> surfaces;
		std::vector<size_t> excluded_entities;
		std::array<float, 3> low{}, high{};
		size_t brush_count	  = 0;
		bool lighting_refined = false;
};

// Classic convex brushes are unioned into exterior polygons. Coplanar overlaps
// retain the earliest source brush's material/texture fields. Render preparation
// is separate (world_render.h). No NIL publication. Non-world entities are
// explicitly listed, not compiled.
// Limits: 64 planes/brush, 65535 vertices and 4096 triangles before/after CSG.
// Failure preserves geometry and reports the brush/reason in error.
bool build_geometry(const nod_scene::Map &map, Geometry &geometry, std::string &error);

} // namespace nod::world
#endif /* __WORLD_GEOMETRY_H__ */
