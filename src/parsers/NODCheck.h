/*
===========================================================================
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

#ifndef NOD_CHECK_H
#define NOD_CHECK_H

#include "NODParser.h"
#include <string>
#include <vector>

/*
 */
// The structure checks of a parsed model that NODParser does not make:
// bone parents, material and palette ranges, rigid or skinned vertex sets,
// detail level 0 index range, and the index and vertex sums of the groups.
struct NODFault {
	int group; // -1 for the whole model
	std::string what;
};

// The vertex count a group adds to its vertex set (uint16 at 0x1C).
uint16_t nodGroupVertexCount(const NODParser::ModelData &m, uint32_t group);
// True when the group holds no index and no vertex.
bool nodGroupEmpty(const NODParser::ModelData &m, uint32_t group);
std::vector<NODFault> nodFaults(const NODParser::ModelData &m);

#endif
