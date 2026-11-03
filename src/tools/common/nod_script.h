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
#include "NOTParser.h"
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
const std::vector<std::string> &handlers();
Fields defaults(const std::string &handler, uint32_t owner_type = 3);
Fields behavior(const std::string &handler);
Fields field_info(const std::string &handler, const std::string &key);
bool compatible(const std::string &handler, const std::vector<NOTParser::ResolvedProperty> &properties);
bool spawnable(const std::vector<NOTParser::ResolvedProperty> &properties);
bool actor_template(const NOTDatabase &templates, const std::string &name, std::string &error);
bool cutscene_reference(const std::string &value);
bool cue_reference(const std::string &value);
bool reverb_parameter(const std::string &value, uint32_t &number);
bool save_marker_parameter(const std::string &value, uint32_t &number);
bool fog_distances(const nod_scene::Entity &entity, std::string &error);
bool control_handler(const std::string &handler);
bool control_parameter(const std::string &handler, unsigned slot, const std::string &value, std::string &error);
bool control_body(const nod_scene::Entity &entity, std::string &error);
bool body(const nod_scene::Entity &entity, const std::map<std::string, uint32_t> &targets,
		  std::vector<uint8_t> &bytes, std::string &error, const NOTDatabase &templates);
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
			 std::vector<std::vector<uint8_t>> &records, std::string &error, const NOTDatabase *templates);

} // namespace nod::script

#endif /* __NOD_SCRIPT_H__ */
