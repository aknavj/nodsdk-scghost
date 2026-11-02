/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nod_script.h
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

#ifndef __NOD_SCRIPT_H__
#define __NOD_SCRIPT_H__

#include "scene_bridge.h"
#include <map>
#include <vector>

/*
 */
class NOTDatabase;

/*
 */
namespace nod::script {

/*
 */
bool authoring_class(const nod_scene::Entity &entity);
bool template_class(const nod_scene::Entity &entity);
bool body_key(const std::string &key);

/*
 */
using Fields = std::map<std::string, std::string>;
// Empty means the handler/owner combination has not been verified.
Fields defaults(const std::string &owner, const std::string &handler, bool modeled_prop = false);
bool body(const nod_scene::Entity &entity, const std::map<std::string, uint32_t> &targets,
		  std::vector<uint8_t> &bytes, std::string &error, bool modeled_prop = false);
bool targets(const nod_scene::Map &map, uint32_t first_index,
			 std::map<std::string, uint32_t> &aliases, std::string &error);
bool placement(const nod_scene::Entity &entity, const NOTDatabase &templates,
			   const std::map<std::string, uint32_t> &targets,
			   std::vector<uint8_t> &bytes, std::string &error);

/*
 */
int maptonsd_main(int argc, char **argv);

// Existing targets are serialized indices below first_index, not native GUIDs.
// On failure records is unchanged. Each spawn's targetname aliases its hidden relay.
bool compile(const nod_scene::Map &map, uint32_t first_index,
			 const std::map<std::string, uint32_t> &existing_targets,
			 std::vector<std::vector<uint8_t>> &records, std::string &error);

} // namespace nod::script

#endif /* __NOD_SCRIPT_H__ */
