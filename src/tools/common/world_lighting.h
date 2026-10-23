/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_lighting.h
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

#ifndef __WORLD_LIGHTING_H__
#define __WORLD_LIGHTING_H__
#include "world_render.h"
#include "map_lighting.h"

/*
 */
namespace nod::world {

// Conforming shared-edge splits preserve the closed collision/render contract.
// Both functions preserve output on failure. Ray occluders may be the original mesh.
bool refine_lighting_geometry(const Geometry &input, float spacing, Geometry &output, std::string &error);
bool bake_lighting(const Geometry &occluders, const lighting::Settings &settings, RenderGeometry &render,
				   std::string &error);

} // namespace nod::world
#endif /* __WORLD_LIGHTING_H__ */
