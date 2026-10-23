/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_render.h
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

#ifndef __WORLD_RENDER_H__
#define __WORLD_RENDER_H__

#include "world_geometry.h"
#include "nod_materials.h"

/*
 */
namespace nod::world {

/*
 */
using TextureProjection = std::array<std::array<float, 4>, 2>;

/*
 */
struct RenderVertex {
		std::array<float, 3> position{}, normal{};
		std::array<float, 2> uv{};
		uint32_t diffuse = 0xffffffff;
};

/*
 */
struct RenderSurface {
		uint32_t material	 = 0;
		uint32_t first_index = 0;
		uint32_t index_count = 0;
};

/*
 */
struct RenderGeometry {
		std::vector<RenderVertex> vertices;
		std::vector<uint16_t> indices;
		std::vector<RenderSurface> surfaces;
		std::vector<NodMaterialInfo> materials;
		std::vector<uint8_t> static_lights;
		std::array<uint8_t, 72> global_light{};
		std::array<float, 3> ambient_light{};
};

// GtkRadiant classic projection, using actual editor image dimensions.
// Both builders preserve output on failure. Neither publishes NIL.
bool build_texture_projection(const Surface &surface, uint32_t width, uint32_t height, TextureProjection &projection,
							  std::string &error);
// Surface/triangle order matches collision geometry; corners retain UV/normal seams.
bool build_render_geometry(const Geometry &geometry, NodMaterials *materials, RenderGeometry &render,
						   std::string &error);

} // namespace nod::world

#endif /* __WORLD_RENDER_H__ */
