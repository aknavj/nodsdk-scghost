/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: mopp_compiler.h
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

#ifndef __MOPP_COMPILER_H__
#define __MOPP_COMPILER_H__

#include <array>
#include <cstdint>
#include <string>
#include <vector>

/*
 */
namespace nod::mopp {

/*
 */
struct Mesh {
		std::vector<std::array<float, 3>> vertices;
		std::vector<uint16_t> indices;
};

/*
 */
struct Code {
		// XYZ offset and world-to-16.16 scale, followed by the raw MOPP program.
		std::array<float, 4> info{};
		std::vector<uint8_t> bytes;
};

// Single-subpart meshes, at most 65535 vertices and 4096 triangles. Primitive
// keys retain input triangle order; spatial sorting changes bytecode order only.
// On failure, code is unchanged and error describes the rejected input.
bool compile(const Mesh &mesh, Code &code, std::string &error);

} // namespace nod::mopp

#endif /* __MOPP_COMPILER_H__ */
