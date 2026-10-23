/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_image.h
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

#ifndef __WORLD_IMAGE_H__
#define __WORLD_IMAGE_H__

#include "world_sector.h"

/*
 */
namespace nod::world {

/*
 */
struct WorldImage {
		std::vector<uint8_t> bytes;
		std::vector<size_t> excluded_entities;
};

// In-memory NIL v35: one unbounded sector, one sector-owned collision body.
// Optional global/ambient lighting, sector point lights and baked diffuse.
// No terrain, environment maps or instances.
// Bounds are min xyz/max xyz and must strictly enclose geometry. No guessed margin.
// Revalidates the mesh/MOPP pairing. Failure preserves output; no file publication.
bool build_world_image(const Geometry &geometry, const RenderGeometry &render,
					   const std::array<float, 6> &physics_bounds, WorldImage &image, std::string &error);

} // namespace nod::world

#endif /* __WORLD_IMAGE_H__ */
