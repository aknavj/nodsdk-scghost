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

#include "NSDEntityType.h"

#include <cstring>
#include <cctype>
#include <cstdio>

/*
 */
static const NsdTypeInfo s_typeInfo[] = {

	{NSD_TYPE_UNKNOWN, "", "", nullptr, NSD_FLAG_NONE, false},

	{NSD_TYPE_GHOST, "ghost", "Player", nullptr, NSD_FLAG_PLAYER | NSD_FLAG_SKIP, false},
	{NSD_TYPE_NOVA_CUT, "nova_cut", "Creature", nullptr, NSD_FLAG_SKIP, false},
 
	// terran — Marines (specific before generic prefix)
	{NSD_TYPE_MARINE1_121A, "marine1_121a", "cTerranMarine1", "marine1Tex.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE2_121A, "marine2_121a", "cTerranMarine2", "marine2Joe.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE3_121A, "marine3_121a", "cTerranMarine3", "marine3Sacker.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE1_131B, "marine1_131b", "cTerranMarine1", "marine1Tex.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE3_CUT, "marine3_cut", "cTerranMarine3", "marine3Sacker.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE_BATTLEFIELD, "marineBattlefield", "cTerranMarineBattlefield", "marineBattlefield.nod",
	 NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE_CAPTAIN_HAULER, "marine_captain_hauler", "cTerranMarineCaptainHauler", "marineCaptain_hauler.nod",
	 NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE_CAPTAIN, "marine_captain", "cTerranMarineCaptain", "marineCaptain.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE_DEAD, "marine_dead", "cTerranMarineDead", "marine1Tex.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE_SIDEKICK_WOUNDED, "marinesidekick_wounded", "cTerranMarineSidekickWounded", "marine1Tex.nod",
	 NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE_SIDEKICK, "marinesidekick", "cTerranMarineSidekick", "marine1Tex.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE_CONTAINER, "marine_container", "cTerranMarine", "marine1Tex.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE1_HAULER, "marine1_hauler", "cTerranMarine1", "marine1_hauler.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE2_HAULER, "marine2_hauler", "cTerranMarine2", "marine2_hauler.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MARINE_ELITE_MOB, "marine_elite_mob", "cTerranMarine", "marine_elite_mob.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_MARINE_HAULER_MOB, "marine_hauler_mob", "cTerranMarine", "marine_hauler_mob.nod", NSD_FLAG_CREATURE,
	 false},
	{NSD_TYPE_MARINE, "marine", "cTerranMarine", "marine1Tex.nod", NSD_FLAG_CREATURE, true},
 	
	// terran — support
	{NSD_TYPE_FIREBAT, "firebat", "cTerranFirebat", "firebat.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MEDIC, "medic", "cTerranMedic", "medic.nod", NSD_FLAG_CREATURE, true},
 	
	// terran — SCVs (specific before generic prefix)
	{NSD_TYPE_SCV121, "scv121", "cTerranSCV121", "scvCombo121.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_SCVCOMBO_121A, "scvCombo_121a", "cTerranSCV121", "scvCombo121.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_SCVCOMBO_CUT, "scvcombo_cut", "cTerranSCV", "scvCombo.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_SCVCOMBO, "scvcombo", "cTerranSCV", "scvCombo.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_SCVDRIVER_CUT, "scvdriver_cut", "cTerranSCVDriver", "scvDriver.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_SCVDRIVER_DEAD, "scvdriver_dead", "cTerranSCVDriver", "scvDriver.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_SCVDRIVER_CONTAINER, "scvdriver_container", "cTerranSCVDriver", "scvDriver.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_SCVDRIVER, "scvdriver", "cTerranSCVDriver", "scvDriver.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_SCVBAYSTEAM, "scvbaySteam", "cTerranSCV", "scvCombo.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_SCV, "scv", "cTerranSCV", "scvCombo.nod", NSD_FLAG_CREATURE, true},
 	
	// terran — Infested (missing .nod)
	{NSD_TYPE_INFESTEDSCVDRIVER_ONFLOOR, "infestedscvdriver_onfloor", "Prop", "infestedSCV.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_INFESTEDSCVDRIVER, "infestedscvdriver", "Prop", "infestedSCV.nod", NSD_FLAG_PROP, true},
 	
	// terran — Infantry / Hauler (missing .nod -> lightInf)
	{NSD_TYPE_INFANTRY_HAULER_CUT, "infantry_hauler_cut", "Prop", "lightInf_hauler.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_INFANTRY_HIB_HAULER, "infantry_hib_hauler", "Prop", "lightInf_hauler.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_INFANTRY_HAULER, "infantry_hauler", "Prop", "lightInf_hauler.nod", NSD_FLAG_PROP, true},

	{NSD_TYPE_HYDRALISK_RUSHING, "hydraliskrushing", "cZergHydraliskRushing", "hydraliskRUSH.nod", NSD_FLAG_CREATURE,
	 true},
	{NSD_TYPE_HYDRALISK_CUT, "hydralisk_cut", "cZergHydralisk", "hydralisk.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_HYDRALISK, "hydralisk", "cZergHydralisk", "hydralisk.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_ZERGLING_NORMAL, "zerglingnormal", "cZergZergling", "zergling.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_ZERGLING_RUSHING, "zerglingrushing", "cZergZergling", "zergling.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_ZERGLING_CUT, "zergling_cut", "cZergZergling", "zergling.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_ZERGLING1_131B, "zergling1_131b", "cZergZergling", "zergling.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_ZERGLING, "zergling", "cZergZergling", "zergling.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_MUTALISK, "mutalisk", "cZergMutalisk", "mutalisk.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_LURKER_BOSS_ARCLITE, "lurker_boss_arclite", "cZergLurker", "hydralisk.nod", NSD_FLAG_CREATURE, false},
	{NSD_TYPE_LURKER, "lurker", "cZergLurker", "lurker.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_ULTRALISK, "ultralisk", "cZergUltralisk", "ultralisk.nod", NSD_FLAG_CREATURE, true},
	{NSD_TYPE_OVERLORD, "overlord", "cZergOverlord", "overlord.nod", NSD_FLAG_CREATURE, false},

	{NSD_TYPE_POWHEALTH, "powhealth", "cPickupHealth", "PU_Health.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWGAUSS_2X, "powgauss_2x", "cPickupGauss2x", "PU_Ammo_Bullet_sixpack.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWGAUSS, "powgauss", "cPickupGauss", "PU_Ammo_Bullet.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWGRENADE, "powgrenade", "cPickupGrenade", "PU_Grenade.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWSNIPE_2X, "powsnipe_2x", "cPickupSniper2x", "PU_Ammo_Sniper_fourpack.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWSNIPE, "powsnipe", "cPickupSniper", "PU_Ammo_Sniper.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWTERRAN, "powterran", "cPickupTerran", "PU_Ammo_01.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWZERG, "powzerg", "cPickupEnergy", "PU_Energy.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWDATA, "powdata", "cPickupData", "PU_Data.nod", NSD_FLAG_PICKUP, false},
	{NSD_TYPE_POWENERGY, "powenergy", "cPickupEnergy", "PU_Energy.nod", NSD_FLAG_PICKUP, false},

	{NSD_TYPE_SIEGETANK_121REPAIR, "siegetank_121repair", "SiegeTank", "siegeTank.nod", NSD_FLAG_VEHICLE, false},
	{NSD_TYPE_SIEGETANK_CUT, "siegetank_cut", "SiegeTank", "siegeTank.nod", NSD_FLAG_VEHICLE, false},
	{NSD_TYPE_SIEGETANK_BLUE, "siegetank_blue", "SiegeTank", "siegeTank_blue.nod", NSD_FLAG_VEHICLE, false},
	{NSD_TYPE_SIEGETANK_BLACK, "siegetank_black", "SiegeTank", "siegeTank_black.nod", NSD_FLAG_VEHICLE, false},
	{NSD_TYPE_SIEGETANK, "siegetank", "SiegeTank", "siegeTank.nod", NSD_FLAG_VEHICLE, true},
	{NSD_TYPE_VULTURE, "vulture", "VultureBike", nullptr, NSD_FLAG_VEHICLE, true},
	{NSD_TYPE_GOLIATH, "goliath", "GoliathMech", nullptr, NSD_FLAG_VEHICLE, true},
 	
	// turrets / defenses
	{NSD_TYPE_TURRET_PLAYER, "turret_player", "Prop", "gunTurret.nod", NSD_FLAG_TURRET, false},
	{NSD_TYPE_TURRET, "turret", "Prop", "gunTurret.nod", NSD_FLAG_TURRET, true},
	{NSD_TYPE_BUNKERGUN, "bunkergun", "Prop", "gunTurret_marine.nod", NSD_FLAG_TURRET, false},
	{NSD_TYPE_ANTIAIRGUN, "antiairgun", "Prop", nullptr, NSD_FLAG_TURRET, true},
	{NSD_TYPE_FLOORGUNALARM, "floorgunalarm", "Prop", "floorGun.nod", NSD_FLAG_TURRET, false},
	{NSD_TYPE_MISSILETURRET_SPIN, "missileturret_spin", "Prop", "MissileTurret_01.nod", NSD_FLAG_TURRET, false},
	{NSD_TYPE_SPORECOLONY_ARCLITE, "sporecolony_arclite", "Prop", "sporeColony.nod", NSD_FLAG_TURRET, false},
 	
	// interactive objects
	{NSD_TYPE_HACKCONSOLE_FLOOR_ACTIVATE, "hackconsole_floor_activate", "Prop", "hackConsole_floor.nod", NSD_FLAG_PROP,
	 false},
	{NSD_TYPE_HACKCONSOLE_FLOOR, "hackconsole_floor", "Prop", "hackConsole_floor.nod", NSD_FLAG_PROP, true},
	{NSD_TYPE_HACKCONSOLE_WALL, "hackconsole_wall", "Prop", "hackConsole_wall.nod", NSD_FLAG_PROP, true},
	{NSD_TYPE_WIRE_DYNAMIC, "wire_dynamic", "Prop", "wire_test_12m.nod", NSD_FLAG_PROP, true},
	{NSD_TYPE_LADDER_DYNAMIC, "ladder_dynamic", "Prop", "GE_ladder1Mx_01.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_LADDER_WALL, "ladder_wall", "Prop", "GE_ladder1Mx_01.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_MS_RADARTOWER, "ms_radartower", "Prop", "MS_RadarTower_01.nod", NSD_FLAG_PROP, false},
 	
	// props — specific named (with fallback model mappings)
	{NSD_TYPE_BATTLECRUISER_MARSARA, "battlecruiser_marsara", "Prop", "1_2_1_battlecruiser.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_BARREL_EXPLODE_PHYS, "barrel_explode_phys", "Prop", "barrel_explode.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_GE_FOOTLOCKER_GRN_GAUSS, "ge_footlocker_grn_gauss", "Prop", "GE_footLocker_grn.nod", NSD_FLAG_PROP,
	 false},
	{NSD_TYPE_GE_FOOTLOCKER_GRN_SNIPE, "ge_footlocker_grn_snipe", "Prop", "GE_footLocker_grn.nod", NSD_FLAG_PROP,
	 false},
	{NSD_TYPE_GE_FOOTLOCKER_GRN_HEALTH, "ge_footlocker_grn_health", "Prop", "GE_footLocker_grn.nod", NSD_FLAG_PROP,
	 false},
	{NSD_TYPE_GE_FOOTLOCKER_GRN_SPIDER, "ge_footlocker_grn_spider", "Prop", "GE_footLocker_grn.nod", NSD_FLAG_PROP,
	 false},
	{NSD_TYPE_DROPSHIPINTERIOR_CUT_222, "dropshipinterior_cut_222", "Prop", "dropshipInterior_cut.nod", NSD_FLAG_PROP,
	 false},
	{NSD_TYPE_MENU_GHOSTLOGO, "menu_ghostlogo", "Prop", "ghostLogo.nod", NSD_FLAG_PROP, false},
	{NSD_TYPE_BUTTON_WALL_ACTIVATE, "button_wall_activate", "Prop", nullptr, NSD_FLAG_SKIP, false},
 	
	// props — generic prefix-matched
	{NSD_TYPE_DOOR, "cDoor", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_MESH, "cMesh", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_MOVETRACK, "movetrack", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_MOVENONE, "movenone", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_MOVEPHYS, "movephys", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_INSTANCE, "instance_", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_HALO, "halo_", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_MS_PROP, "ms_", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_GE_PROP, "ge_", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_LADDER, "ladder", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_WIRE, "wire_", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_HACK, "hack", "Prop", nullptr, NSD_FLAG_PROP, true},
	{NSD_TYPE_NYDUS, "nydus", "Prop", nullptr, NSD_FLAG_PROP, true},

	{NSD_TYPE_TRIGGER_REGION, "trigger_region", nullptr, nullptr, NSD_FLAG_REGION, false},
	{NSD_TYPE_CAMERA_REGION, "camera_region", nullptr, nullptr, NSD_FLAG_REGION, false},
	{NSD_TYPE_ALARM_REGION, "alarm_region", nullptr, nullptr, NSD_FLAG_REGION, false},
	{NSD_TYPE_NORAIN_REGION, "norain_region", nullptr, nullptr, NSD_FLAG_REGION, false},
	{NSD_TYPE_WATER_REGION, "water_region", nullptr, nullptr, NSD_FLAG_REGION, false},
	{NSD_TYPE_BODYTOSS_REGION, "bodytoss_region", nullptr, nullptr, NSD_FLAG_REGION, false},
 	
	// skip types — non-visual entities
	{NSD_TYPE_SCRIPT_OBJECT, "script_object", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_SCRIPT_OBJECT_TRACK, "script_object_track", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_SCRIPT_TARGET, "script_target", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_SCRIPT_TARGET_TRACK, "script_target_track", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_HALO_WHITE_DIM, "halo_white_dim", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_HALO_BLUE_BRIGHT, "halo_blue_bright", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_CONE_WHITE_DIM_LARGE, "cone_white_dim_large", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_GUARDSPOT, "guardspot", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_IONSOUND, "ionSound", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_SPARKFOUNTAIN, "sparkfountain", nullptr, nullptr, NSD_FLAG_SKIP, true},
	{NSD_TYPE_BLACKSMOKE, "blacksmoke", nullptr, nullptr, NSD_FLAG_SKIP, true},
	{NSD_TYPE_FIREHUGE, "firehuge", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_FIRELARGE, "firelarge", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_SMOKEY_GREY, "smokeGrey", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_PLAYERCLIP, "playerclip", nullptr, nullptr, NSD_FLAG_SKIP, true},
	{NSD_TYPE_LVL_LIGHT_CONE, "lvlLightCone", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_AILOOK_CORPSE_PROP, "ailook_corpse_prop", nullptr, nullptr, NSD_FLAG_SKIP, false},

	// additional skip types (AI/logic invisible)
	{NSD_TYPE_AIDONOTENTER_PROP_HULL, "aidonotenter_prop_hull", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_AIDONOTENTER, "aidonotenter", nullptr, nullptr, NSD_FLAG_SKIP, true},
	{NSD_TYPE_AILOOK_MACHINERY_PROP, "ailook_machinery_prop", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_CONSTRAINT_HULL, "constraint_hull", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_MOVETRACK_NOCOLLIDE, "movetrack_nocollide", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_MOVENONE_NOCOLLIDE_ACTIVATE, "movenone_nocollide_activate", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_INSTANCE_NOCOLLIDE, "instance_nocollide", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_INSTANCE_NOCFILE, "instance_nocfile", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_MOVETRACK_HULL, "movetrack_hull", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_BREAKLIGHT_PROP, "breaklight_prop", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_MOVEPHYS_HULL, "movephys_hull", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_MOVENONE_BBOX, "movenone_bbox", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_MOVENONE_NOCOLLIDE, "movenone_nocollide", "Prop", nullptr, NSD_FLAG_PROP, false},
	{NSD_TYPE_GHOST_DUMMYCONTAINER, "ghost_dummycontainer", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_FLOORGUN_NOLIFE, "floorgun_nolife", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_CAM_ANTIZERGHUMAN, "cam_antizerghuman", nullptr, nullptr, NSD_FLAG_SKIP, false},
	{NSD_TYPE_POLYSURFACE, "polySurface", nullptr, nullptr, NSD_FLAG_SKIP, true},
	{NSD_TYPE_PCUBE, "pCube", nullptr, nullptr, NSD_FLAG_SKIP, true},
	{NSD_TYPE_NTOGGLE, "nToggle", nullptr, nullptr, NSD_FLAG_SKIP, true},

	// particle effects (no mesh model)
	{NSD_TYPE_FIREMEDIUM, "firemedium", nullptr, nullptr, NSD_FLAG_EFFECT, false},
	{NSD_TYPE_FIRESMALL, "firesmall", nullptr, nullptr, NSD_FLAG_EFFECT, false},
	{NSD_TYPE_STEAMDOORFALL, "steamdoorfall", nullptr, nullptr, NSD_FLAG_EFFECT, false},
	{NSD_TYPE_STEAMFALLBLUE, "steamfallblue", nullptr, nullptr, NSD_FLAG_EFFECT, false},
	{NSD_TYPE_ZERGMIST, "zergmist", nullptr, nullptr, NSD_FLAG_EFFECT, false},
	{NSD_TYPE_MENUFOG, "menufog", nullptr, nullptr, NSD_FLAG_EFFECT, false},
	{NSD_TYPE_STEAM, "steam", nullptr, nullptr, NSD_FLAG_EFFECT, true},
	{NSD_TYPE_TERRANVIDEOGAME_ZAP, "terranvideogame_zap", "Prop", "TerranVideogame_01.nod", NSD_FLAG_PROP, false},
 // scene-graph-only (non-visual)
	{NSD_TYPE_LIGHT, "cLight", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_SOUND, "cSound", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_PARTICLE, "particle", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_CAMERA, "Camera", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_TERRAIN, "terrain", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_CUTSCENE_PLAY, "cCutscenePlay", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_OBJECT_REMOVE, "cObjectRemove", nullptr, nullptr, NSD_FLAG_SCENE, true},
	{NSD_TYPE_EFFECT_ADD, "cEffectAdd", nullptr, nullptr, NSD_FLAG_SCENE, true},
};

static const int s_typeInfoCount = (int)(sizeof(s_typeInfo) / sizeof(s_typeInfo[0]));

/*
 */
static const NsdTypeInfo *s_infoByEnum[NUM_NSD_TYPES];
static bool s_initialized = false;

#define HASH_TABLE_SIZE 512 // power of 2, >> entry count (~130)

/*
 */
struct HashEntry {
	const NsdTypeInfo *info; // nullptr = empty slot
};

static HashEntry s_hashTable[HASH_TABLE_SIZE];

/*
 */
static uint32_t fnv1a_lower(const char *str, int len) {
	uint32_t hash = 2166136261u;
	for(int i = 0; i < len; i++) {
		hash ^= (uint32_t)tolower((unsigned char)str[i]);
		hash *= 16777619u;
	}
	return hash;
}

/*
 */
static void initTables() {
	if(s_initialized) return;

	memset(s_infoByEnum, 0, sizeof(s_infoByEnum));
	memset(s_hashTable, 0, sizeof(s_hashTable));

	for(int i = 0; i < s_typeInfoCount; i++) {
		const NsdTypeInfo *info = &s_typeInfo[i];

		// populate enum -> info lookup
		s_infoByEnum[info->type] = info;

		// only hash non-prefix, non-empty entries for exact lookup
		if(!info->prefix && info->nsdName[0] != '\0') {
			uint32_t h	 = fnv1a_lower(info->nsdName, (int)strlen(info->nsdName));
			uint32_t idx = h & (HASH_TABLE_SIZE - 1);
			while(s_hashTable[idx].info != nullptr) {
				idx = (idx + 1) & (HASH_TABLE_SIZE - 1);
			}
			s_hashTable[idx].info = info;
		}
	}

	s_initialized = true;
}

/*
 */
static int stricmp_ci(const char *a, const char *b) {
	while(*a && *b) {
		int ca = tolower((unsigned char)*a);
		int cb = tolower((unsigned char)*b);
		if(ca != cb) return ca - cb;
		a++;
		b++;
	}
	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/*
 */
static bool prefixMatch_ci(const char *str, const char *prefix) {
	while(*prefix) {
		if(tolower((unsigned char)*str) != tolower((unsigned char)*prefix)) return false;
		str++;
		prefix++;
	}
	return true;
}

/*
 */
NsdEntityType nsdTypeFromString(const char *nsdTypeName) {

	if(!nsdTypeName || nsdTypeName[0] == '\0') return NSD_TYPE_UNKNOWN;

	initTables();

	int len = (int)strlen(nsdTypeName);

	// exact hash lookup
	uint32_t h	 = fnv1a_lower(nsdTypeName, len);
	uint32_t idx = h & (HASH_TABLE_SIZE - 1);
	int probes	 = 0;
	while(s_hashTable[idx].info != nullptr && probes < 32) {
		if(stricmp_ci(s_hashTable[idx].info->nsdName, nsdTypeName) == 0) return s_hashTable[idx].info->type;
		idx = (idx + 1) & (HASH_TABLE_SIZE - 1);
		probes++;
	}

	// prefix matching — scan prefix entries only
	// order in s_typeInfo matters: specific before generic
	for(int i = 0; i < s_typeInfoCount; i++) {
		if(!s_typeInfo[i].prefix) continue;
		if(prefixMatch_ci(nsdTypeName, s_typeInfo[i].nsdName)) return s_typeInfo[i].type;
	}

	// c-class pattern: starts with 'c' + uppercase -> generic Prop
	if(nsdTypeName[0] == 'c' && len > 1 && nsdTypeName[1] >= 'A' && nsdTypeName[1] <= 'Z') {

		// but check for scene-graph types first
		if(prefixMatch_ci(nsdTypeName, "cLight")) return NSD_TYPE_LIGHT;
		if(prefixMatch_ci(nsdTypeName, "cSound")) return NSD_TYPE_SOUND;
		if(prefixMatch_ci(nsdTypeName, "cCutscenePlay")) return NSD_TYPE_CUTSCENE_PLAY;
		if(prefixMatch_ci(nsdTypeName, "cObjectRemove")) return NSD_TYPE_OBJECT_REMOVE;
		if(prefixMatch_ci(nsdTypeName, "cEffectAdd")) return NSD_TYPE_EFFECT_ADD;
		if(prefixMatch_ci(nsdTypeName, "cRegion")) return NSD_TYPE_TRIGGER_REGION;
		if(prefixMatch_ci(nsdTypeName, "cTriggerRelay")) return NSD_TYPE_TRIGGER_REGION;
		if(prefixMatch_ci(nsdTypeName, "cDoor")) return NSD_TYPE_DOOR;
		if(prefixMatch_ci(nsdTypeName, "cMesh")) return NSD_TYPE_MESH;

		return NSD_TYPE_C_CLASS_PROP;
	}

	// additional substring checks for common patterns
	if(strstr(nsdTypeName, "Stinger")) return NSD_TYPE_SOUND;

	return NSD_TYPE_UNKNOWN;
}

/*
 */
const NsdTypeInfo *nsdTypeGetInfo(NsdEntityType type) {
	initTables();
	if(type >= NUM_NSD_TYPES) return &s_typeInfo[0];
	const NsdTypeInfo *info = s_infoByEnum[type];
	return info ? info : &s_typeInfo[0];
}

/*
 */
const char *nsdTypeToString(NsdEntityType type) {
	return nsdTypeGetInfo(type)->nsdName;
}

/*
 */
const char *nsdTypeArchetype(NsdEntityType type) {
	return nsdTypeGetInfo(type)->archetype;
}

/*
 */
const char *nsdTypeModelFile(NsdEntityType type) {
	return nsdTypeGetInfo(type)->modelFile;
}

/*
 */
uint16_t nsdTypeGetFlags(NsdEntityType type) {
	return nsdTypeGetInfo(type)->flags;
}
