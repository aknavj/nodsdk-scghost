/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nmb_compiler.h
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

#ifndef __NMB_COMPILER_H__
#define __NMB_COMPILER_H__

#include <cstdint>
#include <string>
#include <vector>

/*
 */
namespace nod::bundle {

/*
 */
struct Input {
		std::string name;
		std::vector<uint8_t> bytes;
};

/*
 */
// Input order is member/frame order. No files are read or written here.
bool compile(const std::vector<Input> &inputs, std::vector<uint8_t> &output, std::string &error);

} // namespace nod::bundle

#endif /* __NMB_COMPILER_H__ */
