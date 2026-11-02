/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_imported.h
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
#ifndef __NOD_IMPORTED_H__
#define __NOD_IMPORTED_H__

#include "scene_bridge.h"
#include "NSDParser.h"
#include <map>
#include <vector>

/*
 */
namespace nod::imported {

/*
 */
using Fields = std::map<std::string, std::string>;
struct Splice {
	size_t offset;
	size_t size;
	std::vector<uint8_t> bytes;
};

/*
 */
bool script_fields(const NSDParser &parser, const NSDParser::SceneEntity &entity, Fields &fields);
bool environment_fields(const NSDParser &parser, Fields &fields);
bool record_insertion_offset(const NSDParser &parser, size_t &offset);
bool optional_key(const std::string &key);
bool patch_script(const NSDParser &parser, const NSDParser::SceneEntity &entity,
				  const nod_scene::Entity &actual, const Fields &baseline,
				  const std::map<std::string, uint32_t> &targets, std::vector<uint8_t> &bytes,
				  std::vector<Splice> &splices, std::string &error);
bool patch_environment(const NSDParser &parser, const nod_scene::Entity &actual, const Fields &baseline,
					   std::vector<uint8_t> &bytes, std::string &error);
bool rebuild_navigation(const nod_scene::Map &actual, const nod_scene::Map &expected,
						const Blob &source, std::vector<uint8_t> &bytes, std::string &error);

} // namespace nod::imported

#endif /* __NOD_IMPORTED_H__ */
