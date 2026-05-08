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

#ifndef __NSD_ENTITY_TYPE_H__
#define __NSD_ENTITY_TYPE_H__

#include <cstdint>

enum NsdEntityType : uint16_t {

	// unknown / fallback
	NSD_TYPE_UNKNOWN = 0,

	NSD_TYPE_GHOST,
	NSD_TYPE_NOVA_CUT,

 	// terran — marines
	NSD_TYPE_MARINE, // generic marine (prefix match)
	NSD_TYPE_MARINE1_121A,
	NSD_TYPE_MARINE2_121A,
	NSD_TYPE_MARINE3_121A,
	NSD_TYPE_MARINE1_131B,
	NSD_TYPE_MARINE3_CUT,
	NSD_TYPE_MARINE_BATTLEFIELD,
	NSD_TYPE_MARINE_CAPTAIN_HAULER,
	NSD_TYPE_MARINE_CAPTAIN,
	NSD_TYPE_MARINE_DEAD,
	NSD_TYPE_MARINE_SIDEKICK_WOUNDED,
	NSD_TYPE_MARINE_SIDEKICK,
	NSD_TYPE_MARINE_CONTAINER,
	NSD_TYPE_MARINE1_HAULER,
	NSD_TYPE_MARINE2_HAULER,
	NSD_TYPE_MARINE_ELITE_MOB,
	NSD_TYPE_MARINE_HAULER_MOB,

 	// terran — support
	NSD_TYPE_FIREBAT,
	NSD_TYPE_MEDIC,

 	// terran — SCVs
	NSD_TYPE_SCV121,
	NSD_TYPE_SCVCOMBO_121A,
	NSD_TYPE_SCVCOMBO_CUT,
	NSD_TYPE_SCVCOMBO,
	NSD_TYPE_SCVDRIVER_CUT,
	NSD_TYPE_SCVDRIVER_DEAD,
	NSD_TYPE_SCVDRIVER_CONTAINER,
	NSD_TYPE_SCVDRIVER,
	NSD_TYPE_SCVBAYSTEAM,
	NSD_TYPE_SCV,

 	// terran — infested
	NSD_TYPE_INFESTEDSCVDRIVER,
	NSD_TYPE_INFESTEDSCVDRIVER_ONFLOOR,

 	// terran — infantry / hauler (lightInf)
	NSD_TYPE_INFANTRY_HAULER_CUT,
	NSD_TYPE_INFANTRY_HAULER,
	NSD_TYPE_INFANTRY_HIB_HAULER,

	NSD_TYPE_HYDRALISK_CUT,
	NSD_TYPE_HYDRALISK_RUSHING,
	NSD_TYPE_HYDRALISK,
	NSD_TYPE_ZERGLING_NORMAL,
	NSD_TYPE_ZERGLING_RUSHING,
	NSD_TYPE_ZERGLING_CUT,
	NSD_TYPE_ZERGLING1_131B,
	NSD_TYPE_ZERGLING,
	NSD_TYPE_MUTALISK,
	NSD_TYPE_LURKER_BOSS_ARCLITE,
	NSD_TYPE_LURKER,
	NSD_TYPE_ULTRALISK,
	NSD_TYPE_OVERLORD,

 	// pickups / powerups
	NSD_TYPE_POWHEALTH,
	NSD_TYPE_POWGAUSS_2X,
	NSD_TYPE_POWGAUSS,
	NSD_TYPE_POWGRENADE,
	NSD_TYPE_POWSNIPE_2X,
	NSD_TYPE_POWSNIPE,
	NSD_TYPE_POWTERRAN,
	NSD_TYPE_POWZERG,
	NSD_TYPE_POWDATA,
	NSD_TYPE_POWENERGY,

	NSD_TYPE_SIEGETANK_121REPAIR,
	NSD_TYPE_SIEGETANK_CUT,
	NSD_TYPE_SIEGETANK_BLUE,
	NSD_TYPE_SIEGETANK_BLACK,
	NSD_TYPE_SIEGETANK,
	NSD_TYPE_VULTURE,
	NSD_TYPE_GOLIATH,

 	// turrets / defenses
	NSD_TYPE_TURRET_PLAYER,
	NSD_TYPE_TURRET,
	NSD_TYPE_BUNKERGUN,
	NSD_TYPE_ANTIAIRGUN,
	NSD_TYPE_FLOORGUNALARM,
	NSD_TYPE_MISSILETURRET_SPIN,
	NSD_TYPE_SPORECOLONY_ARCLITE,

 	// interactive objects
	NSD_TYPE_HACKCONSOLE_FLOOR_ACTIVATE,
	NSD_TYPE_HACKCONSOLE_FLOOR,
	NSD_TYPE_HACKCONSOLE_WALL,
	NSD_TYPE_WIRE_DYNAMIC,
	NSD_TYPE_LADDER_DYNAMIC,
	NSD_TYPE_LADDER_WALL,
	NSD_TYPE_MS_RADARTOWER,

 	// props — specific named
	NSD_TYPE_BATTLECRUISER_MARSARA,
	NSD_TYPE_BARREL_EXPLODE_PHYS,

	NSD_TYPE_GE_FOOTLOCKER_GRN_GAUSS,
	NSD_TYPE_GE_FOOTLOCKER_GRN_SNIPE,
	NSD_TYPE_GE_FOOTLOCKER_GRN_HEALTH,
	NSD_TYPE_GE_FOOTLOCKER_GRN_SPIDER,

	NSD_TYPE_DROPSHIPINTERIOR_CUT_222,

	NSD_TYPE_MENU_GHOSTLOGO,

 	// props — generic prefix-matched (cDoor, cMesh, instance_, etc.)
 	NSD_TYPE_DOOR, // cDoorMove, cDoor prefix
 	NSD_TYPE_MESH, // cMesh prefix
 	NSD_TYPE_MOVETRACK, // movetrack prefix
	NSD_TYPE_MOVENONE, // movenone prefix
	NSD_TYPE_MOVEPHYS, // movephys prefix
	NSD_TYPE_INSTANCE, // instance_ prefix
	NSD_TYPE_HALO, // halo_ prefix
	NSD_TYPE_MS_PROP, // ms_ prefix
	NSD_TYPE_GE_PROP, // ge_ prefix
	NSD_TYPE_LADDER, // ladder prefix
	NSD_TYPE_WIRE, // wire_ prefix
	NSD_TYPE_HACK, // hack prefix
	NSD_TYPE_NYDUS, // nydus prefix
	NSD_TYPE_C_CLASS_PROP, // unknown cXxxx class -> Prop
	NSD_TYPE_BUTTON_WALL_ACTIVATE,

 	// regions (trigger/camera)
 	NSD_TYPE_TRIGGER_REGION,
 	NSD_TYPE_CAMERA_REGION,
 	NSD_TYPE_ALARM_REGION,
 	NSD_TYPE_NORAIN_REGION,
 	NSD_TYPE_WATER_REGION,
 	NSD_TYPE_BODYTOSS_REGION,

 	// skip types — non-visual entities
	NSD_TYPE_SCRIPT_OBJECT,
	NSD_TYPE_SCRIPT_OBJECT_TRACK,
	NSD_TYPE_SCRIPT_TARGET,
	NSD_TYPE_SCRIPT_TARGET_TRACK,
	NSD_TYPE_HALO_WHITE_DIM,
	NSD_TYPE_HALO_BLUE_BRIGHT,
	NSD_TYPE_CONE_WHITE_DIM_LARGE,
	NSD_TYPE_GUARDSPOT,
	NSD_TYPE_IONSOUND,
	NSD_TYPE_SPARKFOUNTAIN,
	NSD_TYPE_BLACKSMOKE,
	NSD_TYPE_FIREHUGE,
	NSD_TYPE_FIRELARGE,
	NSD_TYPE_SMOKEY_GREY,
	NSD_TYPE_PLAYERCLIP,
	NSD_TYPE_PLAYERCLIP_ONESHOT,
	NSD_TYPE_PLAYERCLIP_NOCOLLIDE,
	NSD_TYPE_LVL_LIGHT_CONE,
	NSD_TYPE_AILOOK_CORPSE_PROP,

	// additional skip types (AI/logic invisible)
	NSD_TYPE_AIDONOTENTER_PROP_HULL,
	NSD_TYPE_AIDONOTENTER, // aidonotenter prefix
	NSD_TYPE_AILOOK_MACHINERY_PROP,
	NSD_TYPE_CONSTRAINT_HULL,
	NSD_TYPE_MOVETRACK_NOCOLLIDE,
	NSD_TYPE_MOVENONE_NOCOLLIDE_ACTIVATE,
	NSD_TYPE_INSTANCE_NOCOLLIDE,
	NSD_TYPE_INSTANCE_NOCFILE,
	NSD_TYPE_MOVETRACK_HULL,
	NSD_TYPE_BREAKLIGHT_PROP,
	NSD_TYPE_MOVEPHYS_HULL,
	NSD_TYPE_MOVENONE_BBOX,
	NSD_TYPE_MOVENONE_NOCOLLIDE,
	NSD_TYPE_GHOST_DUMMYCONTAINER,
	NSD_TYPE_FLOORGUN_NOLIFE,
	NSD_TYPE_CAM_ANTIZERGHUMAN,
	NSD_TYPE_POLYSURFACE, // polySurface prefix — Maya leftover
	NSD_TYPE_PCUBE, // pCube prefix — Maya leftover
	NSD_TYPE_NTOGGLE, // nToggle — logic switch

	// particle effects (no mesh model)
	NSD_TYPE_FIREMEDIUM,
	NSD_TYPE_FIRESMALL,
	NSD_TYPE_STEAMDOORFALL,
	NSD_TYPE_STEAMFALLBLUE,
	NSD_TYPE_ZERGMIST,
	NSD_TYPE_MENUFOG,
	NSD_TYPE_STEAM, // steam prefix (steamjet, steamfall, etc.)
	NSD_TYPE_TERRANVIDEOGAME_ZAP, // effect -> TerranVideogame_01.nod fallback

 	// scene-graph-only (non-visual, skipped by archetype resolver)
	NSD_TYPE_LIGHT, // cLight prefix
	NSD_TYPE_SOUND, // cSound prefix
	NSD_TYPE_PARTICLE, // particle prefix
	NSD_TYPE_CAMERA, // Camera prefix
	NSD_TYPE_TERRAIN, // Terrain prefix
	NSD_TYPE_CUTSCENE_PLAY, // cCutscenePlay
	NSD_TYPE_OBJECT_REMOVE, // cObjectRemove
	NSD_TYPE_EFFECT_ADD, // cEffectAdd

	NUM_NSD_TYPES
};

/*
 */
enum NsdTypeFlags : uint16_t {
	NSD_FLAG_NONE	  = 0,
	NSD_FLAG_SKIP	  = (1 << 0), // non-visual, skip entirely
	NSD_FLAG_REGION	  = (1 << 1), // trigger/camera region
	NSD_FLAG_PLAYER	  = (1 << 2), // player spawn
	NSD_FLAG_CREATURE = (1 << 3), // AI creature
	NSD_FLAG_PICKUP	  = (1 << 4), // pickup / powerup
	NSD_FLAG_VEHICLE  = (1 << 5), // drivable vehicle
	NSD_FLAG_PROP	  = (1 << 6), // static/dynamic prop
	NSD_FLAG_EFFECT	  = (1 << 7), // particle effect (no mesh)
	NSD_FLAG_TURRET	  = (1 << 8), // turret / defense
	NSD_FLAG_SCENE	  = (1 << 9), // scene-graph-only (light/sound/camera)
};

/*
 */
struct NsdTypeInfo {
	NsdEntityType type;
	const char *nsdName; // NSD type name string (for exact match)
	const char *archetype; // EntityFactory archetype name
	const char *modelFile; // .nod model filename (nullptr = use tag4/typename)
	uint16_t flags; // NsdTypeFlags bitmask
	bool prefix; // true = prefix match this nsdName
};

// convert NSD type name string -> enum (O(1) hash lookup, done once at parse time)
NsdEntityType nsdTypeFromString(const char *nsdTypeName);

// get type info struct for a given enum value
const NsdTypeInfo *nsdTypeGetInfo(NsdEntityType type);
const char *nsdTypeToString(NsdEntityType type);
const char *nsdTypeArchetype(NsdEntityType type);
const char *nsdTypeModelFile(NsdEntityType type);
uint16_t nsdTypeGetFlags(NsdEntityType type);

/*
 */
inline bool nsdTypeIsSkip(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_SKIP) != 0;
}

/*
 */
inline bool nsdTypeIsRegion(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_REGION) != 0;
}

/*
 */
inline bool nsdTypeIsPlayer(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_PLAYER) != 0;
}

/*
 */
inline bool nsdTypeIsCreature(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_CREATURE) != 0;
}

/*
 */
inline bool nsdTypeIsPickup(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_PICKUP) != 0;
}

/*
 */
inline bool nsdTypeIsVehicle(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_VEHICLE) != 0;
}

/*
 */
inline bool nsdTypeIsProp(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_PROP) != 0;
}

/*
 */
inline bool nsdTypeIsEffect(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_EFFECT) != 0;
}

/*
 */
inline bool nsdTypeIsTurret(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_TURRET) != 0;
}

/*
 */
inline bool nsdTypeIsScene(NsdEntityType t) {
	return (nsdTypeGetFlags(t) & NSD_FLAG_SCENE) != 0;
}

/*
 */
inline bool nsdTypeIsVisual(NsdEntityType t) {
	uint16_t f = nsdTypeGetFlags(t);
	return !(f & (NSD_FLAG_SKIP | NSD_FLAG_REGION | NSD_FLAG_SCENE));
}

#endif // __NSD_ENTITY_TYPE_H__
