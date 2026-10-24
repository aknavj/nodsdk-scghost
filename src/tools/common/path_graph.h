/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: path_graph.h
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

#ifndef __PATH_GRAPH_H__
#define __PATH_GRAPH_H__
#include "scene_bridge.h"
#include <vector>

/*
 */
namespace nod::path {
	
/*
 */
struct Graph {
		std::vector<uint8_t> bytes;
		size_t nodes	  = 0;
		size_t links	  = 0;
		size_t components = 0;
};

/*
 */
bool authoring_class(const nod_scene::Entity &entity);
bool compile(const nod_scene::Map &map, Graph &out, std::string &error);

} // namespace nod::path

#endif /* __PATH_GRAPH_H__ */
