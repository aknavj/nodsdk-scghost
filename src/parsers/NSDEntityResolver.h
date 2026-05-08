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

#ifndef __NSD_ENTITY_RESOLVER_H__
#define __NSD_ENTITY_RESOLVER_H__

#include "NSDEntityType.h"
#include <cstdint>

/*
 */
struct NSDEntityRef {
	char model_name[64];
	char type_name[64];
	NsdEntityType type; // integer type ID
	float position[3];
	float rotation[3];
};

/*
 */
struct NSDRegionRef {
	float position[3];
	float extentMin[3];
	float extentMax[3];
	char className[48];
	uint8_t isCameraRegion;
};

/*
 */
class NSDEntityResolver {

	public:
		// classify NSD type name -> GameEntity archetype name.
		// returns empty string if the type should be skipped.
		static const char *resolveArchetype(const char *nsdTypeName);

		// resolve NSD type name -> model filename (.nod).
		// returns nullptr if no mapping exists.
		static const char *resolveModelFile(const char *nsdTypeName);

		// check if an NSD type name is a region type (trigger/camera).
		static bool isRegionType(const char *nsdTypeName);

		// check if an NSD type name should be skipped (non-visual).
		static bool isSkipType(const char *nsdTypeName);

		// extract all entity placements from NSD binary file.
		// fixed-offset block parsing (tag1=1@+32, tag2=2@+68); caller frees.
		static void extractFromNSD(const char *nsdPath, NSDEntityRef **outEntities, uint32_t *outEntityCount,
								NSDRegionRef **outRegions, uint32_t *outRegionCount, float outGhostPos[3] = nullptr,
								float outGhostRot[3] = nullptr, bool *outHasGhost = nullptr,
								char outSkyModelName[64] = nullptr);

		// extract light entities (cLight*) from NSD; classified as NSD_FLAG_SCENE.
		// outputs malloc'd array; caller frees.
		struct NSDLightRef {
			float position[3];
			float rotation[3];
			char className[48];
		};
		static void extractLightsFromNSD(const char *nsdPath, NSDLightRef **outLights, uint32_t *outLightCount);
};

#endif // __NSD_ENTITY_RESOLVER_H__
