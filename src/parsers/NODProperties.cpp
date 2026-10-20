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

#include "NODProperties.h"

/*
 */
const NODPropertyInfo *nodPropertyInfo(uint32_t id) {
	// Star_d memory export: records begin at 0xA05250, dispatch words at 0xA05270.
	static const NODPropertyInfo properties[] = {
		{"basedon", 0, 0}, {"orientation", 0, 4}, {"position", 0, 4}, {"sector", 0, 3},
		{"model", 15, 0}, {"editorcategory", 0, 3}, {"editorsizemins", 0, 4},
		{"editorsizemaxs", 0, 4}, {"editorcolor", 0, 4}, {"editor_subcategory", 0, 0},
		{"foley", 15, 0}, {"thingflags", 0, 3}, {"renderflags", 0, 3}, {"motionset", 15, 0},
		{"modelflags", 15, 3}, {"script", 2, 0}, {"collide", 0, 3}, {"radius", 0, 2},
		{"height", 0, 2}, {"movespeed", 0, 2}, {"type", 0, 3}, {"item_type", 99, 3},
		{"item_flags", 99, 3}, {"item_amount", 99, 2}, {"ITEM_ID", 99, 3},
		{"STAT_ENERGY", 4, 3}, {"STAT_ENERGYPOOL", 4, 3}, {"WEAPON_FLAGS", 99, 3},
		{"WEAPON_TYPE1", 99, 3}, {"WEAPON_TYPE2", 99, 3}, {"WEAPON_DEFENSETYPE", 99, 3},
		{"WEAPON_DAMAGE1", 99, 3}, {"WEAPON_DAMAGE2", 99, 3}, {"WEAPON_STRIKETIME1", 99, 2},
		{"WEAPON_STRIKETIME2", 99, 2}, {"WEAPON_RELOADTIME", 99, 2},
		{"WEAPON_MINDISTANCE", 99, 2}, {"WEAPON_MAXDISTANCE", 99, 2},
		{"WEAPON_PROJECTILE", 99, 0}, {"WEAPON_MODEL", 99, 0}, {"ACTOR_HEALTH", 4, 3},
		{"ACTOR_WALKSPEED", 4, 2}, {"ACTOR_RUNSPEED", 4, 2}, {"ACTOR_AI", 4, 0},
		{"ACTOR_MAXHEALTH", 4, 3}, {"TEMPLATE_MOVETYPE", 0, 3}, {"CASTID", 7, 0},
		{"DESCRIPTIONID", 0, 0}, {"TIMER", 0, 3}, {"PROJECT_FLAGS", 9, 3},
		{"PROJECT_DAMAGE", 9, 3}, {"PROJECT_DAMAGETYPE", 9, 3}, {"PHYSICS_FLAGS", 8, 3},
		{"PHYSICS_VELOCITY", 8, 4}, {"PHYSICS_ROTVEL", 8, 4}, {"REGION_FLAGS", 0, 3},
		{"ACTOR_WEAPON", 4, 0}, {"ACTOR_FIREPOS", 4, 4}, {"ITEM_FIREPOS", 99, 4},
		{"HALO_MATERIAL", 0, 0}, {"HALO_SIZE", 0, 2}, {"HALO_COLOR", 0, 4},
		{"SOUND_TYPE", 15, 3}, {"ACTOR_FLAGS", 4, 3}, {"DROPITEM", 0, 0},
		{"CONTAINERHEALTH", 11, 3}, {"DESTROYTEMPLATE", 15, 0}, {"SKELETON_ID", 15, 3},
		{"PARTICLENAME", 0, 68}, {"DYNA_INTENSITY", 0, 2}, {"DYNA_MATERIAL", 0, 0},
		{"DYNA_COLOR", 0, 4}, {"DYNA_BONE", 0, 3}, {"DYNA_FLAGS", 0, 3}, {"DYNA_STYLE", 0, 3},
		{"DYNA_RADIUS", 0, 2}, {"EXPLOSION", 9, 0}, {"EXPLOSION_NAME", 99, 0},
		{"EXPLOSION_FLAGS", 99, 3}, {"WEAPON_DAMAGETYPE1", 99, 3},
		{"WEAPON_DAMAGETYPE2", 99, 3}, {"WEAPON_MUZZLEFLASH1", 99, 0},
		{"WEAPON_MUZZLEFLASH2", 99, 0}, {"DYNA_OFFSET", 0, 4}, {"PROJECT_MAXDIST", 9, 2},
		{"ACTOR_TYPE", 4, 3}, {"WEAPON_AMMOTYPE1", 99, 3}, {"WEAPON_AMMOTYPE2", 99, 3},
		{"WEAPON_MAXAMMO1", 99, 3}, {"WEAPON_MAXAMMO2", 99, 3}, {"WEAPON_RELOADTIME2", 99, 2},
		{"WEAPON_PROJECTILE2", 99, 0}, {"WEAPON_MINDIST2", 99, 2}, {"WEAPON_MAXDIST2", 99, 2},
		{"ITEM_EFFECTNAME0", 99, 0}, {"ITEM_EFFECTLEVEL0", 99, 3}, {"ITEM_EFFECTVALUE0", 99, 2}, {"ITEM_EFFECTTIME0", 99, 3},
		{"ITEM_EFFECTNAME1", 99, 0}, {"ITEM_EFFECTLEVEL1", 99, 3}, {"ITEM_EFFECTVALUE1", 99, 2}, {"ITEM_EFFECTTIME1", 99, 3},
		{"ITEM_EFFECTNAME2", 99, 0}, {"ITEM_EFFECTLEVEL2", 99, 3}, {"ITEM_EFFECTVALUE2", 99, 2}, {"ITEM_EFFECTTIME2", 99, 3},
		{"ITEM_EFFECTNAME3", 99, 0}, {"ITEM_EFFECTLEVEL3", 99, 3}, {"ITEM_EFFECTVALUE3", 99, 2}, {"ITEM_EFFECTTIME3", 99, 3},
		{"ITEM_EFFECTNAME4", 99, 0}, {"ITEM_EFFECTLEVEL4", 99, 3}, {"ITEM_EFFECTVALUE4", 99, 2}, {"ITEM_EFFECTTIME4", 99, 3},
		{"ITEM_EFFECTNAME5", 99, 0}, {"ITEM_EFFECTLEVEL5", 99, 3}, {"ITEM_EFFECTVALUE5", 99, 2}, {"ITEM_EFFECTTIME5", 99, 3},
		{"ITEM_EFFECTNAME6", 99, 0}, {"ITEM_EFFECTLEVEL6", 99, 3}, {"ITEM_EFFECTVALUE6", 99, 2}, {"ITEM_EFFECTTIME6", 99, 3},
		{"ITEM_EFFECTNAME7", 99, 0}, {"ITEM_EFFECTLEVEL7", 99, 3}, {"ITEM_EFFECTVALUE7", 99, 2}, {"ITEM_EFFECTTIME7", 99, 3},
		{"ITEM_EFFECTNAME8", 99, 0}, {"ITEM_EFFECTLEVEL8", 99, 3}, {"ITEM_EFFECTVALUE8", 99, 2}, {"ITEM_EFFECTTIME8", 99, 3},
		{"ITEM_EFFECTNAME9", 99, 0}, {"ITEM_EFFECTLEVEL9", 99, 3}, {"ITEM_EFFECTVALUE9", 99, 2}, {"ITEM_EFFECTTIME9", 99, 3},
		{"WEAPON_MELEETYPE2", 99, 3}, {"WEAPON_AMMOUSE1", 99, 3}, {"WEAPON_AMMOUSE2", 99, 3},
		{"PHYSICS_VELDRAG", 8, 2}, {"PHYSICS_ROTDRAG", 8, 2}, {"PHYSICS_MASS", 8, 2},
		{"PHYSICS_ELASTICITY", 8, 2}, {"PHYSICS_THRUST", 8, 4}, {"PROXRADIUS", 0, 2},
		{"MODELSCALE", 15, 2}, {"WEAPON_ACCURACY1", 99, 2}, {"WEAPON_ACCURACY2", 99, 2},
		{"WEAPON_FLAGS1", 99, 3}, {"WEAPON_FLAGS2", 99, 3}, {"WEAPON_BLADESTART", 99, 4},
		{"WEAPON_BLADEEND", 99, 4}, {"WEAPON_BLADETRAIL", 99, 0}, {"PHYSICS_GRAVITY", 8, 2},
		{"WALLMARK", 0, 0}, {"WEAPON_THROWTYPE", 99, 3}, {"ACTOR_GIBCLASS", 4, 3},
		{"WEAPON_THROWDIST", 99, 2}, {"WEAPON_THROWCHANCE", 99, 2},
		{"EMITTER1", 0, 0}, {"EMITTER2", 0, 0}, {"EMITTER3", 0, 0}, {"EMITTER4", 0, 0},
		{"EFFECT1", 0, 0}, {"EFFECT2", 0, 0}, {"EFFECT3", 0, 0}, {"EFFECT4", 0, 0},
		{"SENSOR_FLAGS", 14, 3}, {"PHYSMODEL", 0, 0}, {"VEHICLEDESC", 4, 0},
		{"ALARM_GROUP", 0, 3}, {"PROP_FLAGS", 13, 3}, {"RETICULE_SCALE", 4, 2},
		{"CONTAINER_FLAGS", 11, 3}, {"CONTAINER_HAVOKBREAK_MODEL", 11, 0},
		{"CONTAINER_HAVOKBREAK_TEMPLATE", 11, 0}, {"MODEL_LODSCALE", 15, 2},
		{"MODELOFFSET", 15, 4}, {"SCORE_REWARD", 15, 3}
	};
	static_assert(sizeof(properties) / sizeof(properties[0]) == 177);
	return id < 177 ? &properties[id] : nullptr;
}

/*
 */
bool nodPropertyMapEditable(uint32_t id, bool preview) {
	const auto *info = nodPropertyInfo(id);
	if(!info || id <= 3 || id == 20 ||
	   (info->native_type != 0 && info->native_type != 2 && info->native_type != 3 && info->native_type != 4))
		return false;
	return !preview || (id != 4 && id != 12 && id != 15 && id != 143 && id != 175);
}

/*
 */
bool nodModifierString(const NODModifier &modifier, std::string &value) {
	value.clear();
	if(modifier.value_size != sizeof(modifier.value)) return false;
	size_t length = 0;
	while(length < sizeof(modifier.value) && modifier.value[length]) {
		if(modifier.value[length] < 32 || modifier.value[length] > 126) return false;
		++length;
	}
	value.assign(reinterpret_cast<const char *>(modifier.value), length);
	return true;
}
