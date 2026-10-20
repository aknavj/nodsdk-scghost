/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
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

#ifndef NOD_PROPERTIES_H
#define NOD_PROPERTIES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/*
 */
struct NODModifier {
	uint32_t prop_id;
	uint8_t value[32];
	uint32_t value_size;
	size_t value_offset;
};

/*
 */
struct NODPropertyInfo {
	const char *name;
	uint32_t handler;
	uint32_t native_type;
};

/*
 */
// Verified engine table: name[32], handler, native type; 177 entries.
const NODPropertyInfo *nodPropertyInfo(uint32_t id);
bool nodPropertyMapEditable(uint32_t id, bool preview);

/*
 */
bool nodModifierString(const NODModifier &modifier, std::string &value);
inline size_t nodPlacementPrefix(const std::vector<NODModifier> &modifiers) {
	return modifiers.size() > 3 && (modifiers[3].prop_id == 3 || modifiers[3].prop_id == 15) ? 4 : 3;
}

#endif
