/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_sector.h
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

#ifndef __WORLD_SECTOR_H__
#define __WORLD_SECTOR_H__

#include "world_render.h"
#include "NILCollision.h"

/*
 */
namespace nod::world {

/*
 */
struct SectorGroup {
		uint32_t material	 = 0;
		uint32_t first_index = 0;
		uint16_t index_count = 0;
		std::array<float, 3> low{}, high{};
};

/*
 */
struct SectorData {
		std::array<float, 3> low{}, high{}, center{};
		std::vector<SectorGroup> groups;
		nil::CollisionSector collision_mapping;
		// One native sector record, not a NIL file or a publishable level.
		std::vector<uint8_t> bytes;
};

// One primary vertex pool; DDS-backed direct/NSA materials and zero authoring flags only.
// Adjacent material runs are merged without reordering render/collision triangles.
// Group shader indices address render.materials unchanged.
// Carries baked diffuse and native static lights when supplied by the CPU bake.
// No boundary planes, instances or file publication. Failure preserves output.
bool build_sector(const Geometry &geometry, const RenderGeometry &render, SectorData &sector, std::string &error);

} // namespace nod::world

#endif /* __WORLD_SECTOR_H__ */
