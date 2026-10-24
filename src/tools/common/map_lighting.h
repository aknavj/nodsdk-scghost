/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: map_lighting.h
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

#ifndef __MAP_LIGHTING_H__
#define __MAP_LIGHTING_H__
#include <array>
#include <string>
#include <vector>
#include <cstdint>

// forward declarations
namespace nod_scene {
	struct Map;
	struct Entity;
} // namespace nod_scene

/*
 */
namespace nod::lighting {
	
	/*
	 */
	struct Point {
			std::array<float, 3> position{}, color{1, 1, 1};
			float intensity = 8;
			float radius	= 10;
			float falloff	= 0;
	};

	/*
	 */
	struct Settings {
			float spacing = 4;
			std::vector<Point> points;
			bool enabled			= true;
			bool force				= false;
			bool shadows			= true;
			bool sun				= false;
			bool ambient			= false;
			unsigned samples		= 1;
			float phong				= 0;
			float sun_intensity		= 1;
			float ambient_intensity = 0.35f;
			std::array<float, 3> sun_color{1, 1, 1}, sun_direction{-1, 2, -4}, ambient_color{1, 1, 1};
			bool active() const { return enabled && (force || !points.empty() || sun || ambient); }
	};

	/*
	 */
	int option_arity(const std::string &name);
	bool apply_options(const std::vector<std::string> &arguments, Settings &settings, std::string &error);
	bool authoring_class(const nod_scene::Entity &entity);
	bool parse(const nod_scene::Map &map, Settings &settings, std::string &error);
	std::vector<uint8_t> records(const Settings &settings);

} // namespace nod::lighting

#endif /* __MAP_LIGHTING_H__ */
