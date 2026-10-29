/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_binary.h
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

#ifndef __WORLD_BINARY_H__
#define __WORLD_BINARY_H__

#include "tool_common.h"
#include "NILCollision.h"
#include <array>
#include <vector>

/*
 */
namespace nod::world::detail {

/*
 */
class Writer {
	public:
		std::vector<uint8_t> bytes;
			
		/*
		 */
		void byte(uint8_t value) { 
			bytes.push_back(value); 
		}
			
		/*
		 */
		void half(uint16_t value) {
			const auto offset = bytes.size();
			bytes.resize(offset + 2);
			put16(bytes.data() + offset, value);
		}

		/*
		 */
		void word(uint32_t value) {
			const auto offset = bytes.size();
			bytes.resize(offset + 4);
			put32(bytes.data() + offset, value);
		}
			
		/*
		 */
		void real(float value) {
			const auto offset = bytes.size();
			bytes.resize(offset + 4);
			put_float(bytes.data() + offset, value);
		}

		/*
		 */
		void vector(const std::array<float, 3> &value) {
			for(float component : value) {
				real(component);
			}
		}
	
		/*
		 */
		void append(const std::vector<uint8_t> &value) { 
			bytes.insert(bytes.end(), value.begin(), value.end()); 
		}

		/*
		 */
		void collision_mapping(const nil::CollisionSector &sector) {
			word(sector.present);
			for(const auto &group : sector.groups) {
				word(group.present);
				if(group.present) {
					word(group.offset);
				}
			}
		}
};

} // namespace nod::world::detail

#endif /* __WORLD_BINARY_H__ */
