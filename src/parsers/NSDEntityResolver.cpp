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

#include "NSDEntityResolver.h"
#include "NSDEntityType.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>

/*
 */
static int stricmp_local(const char *a, const char *b) {
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
const char *NSDEntityResolver::resolveArchetype(const char *nsdTypeName) {

	if(!nsdTypeName || nsdTypeName[0] == '\0') return "";

	NsdEntityType type = nsdTypeFromString(nsdTypeName);
	const char *arch   = nsdTypeArchetype(type);
	return arch ? arch : "";
}

/*
 */
const char *NSDEntityResolver::resolveModelFile(const char *nsdTypeName) {

	if(!nsdTypeName || nsdTypeName[0] == '\0') return nullptr;

	NsdEntityType type = nsdTypeFromString(nsdTypeName);
	return nsdTypeModelFile(type);
}

/*
 */
bool NSDEntityResolver::isRegionType(const char *nsdTypeName) {

	if(!nsdTypeName) return false;
	NsdEntityType type = nsdTypeFromString(nsdTypeName);
	return nsdTypeIsRegion(type);
}

/*
 */
bool NSDEntityResolver::isSkipType(const char *nsdTypeName) {

	if(!nsdTypeName) return false;
	NsdEntityType type = nsdTypeFromString(nsdTypeName);
	return nsdTypeIsSkip(type) || nsdTypeIsScene(type) || nsdTypeIsEffect(type);
}

/*
 */
void NSDEntityResolver::extractFromNSD(const char *nsdPath, NSDEntityRef **outEntities, uint32_t *outEntityCount,
									   NSDRegionRef **outRegions, uint32_t *outRegionCount, float outGhostPos[3],
									   float outGhostRot[3], bool *outHasGhost, char outSkyModelName[64]) {

	*outEntities	= nullptr;
	*outEntityCount = 0;
	*outRegions		= nullptr;
	*outRegionCount = 0;
	if(outHasGhost) *outHasGhost = false;
	if(outSkyModelName) outSkyModelName[0] = '\0';

	FILE *fp = fopen(nsdPath, "rb");
	if(!fp) {
		printf("[NSDEntityResolver] Cannot open NSD: %s\n", nsdPath);
		return;
	}
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(sz < 200) {
		fclose(fp);
		return;
	}

	uint8_t *data = (uint8_t *)malloc((size_t)sz);
	fread(data, 1, (size_t)sz, fp);
	fclose(fp);

	uint32_t entCap = 512, entCount = 0;
	NSDEntityRef *ents = (NSDEntityRef *)malloc(entCap * sizeof(NSDEntityRef));
	uint32_t regCap = 256, regCount = 0;
	NSDRegionRef *regs = (NSDRegionRef *)malloc(regCap * sizeof(NSDRegionRef));

	long pos = 0;
	while(pos + 144 < sz) {

		// find printable null-terminated string (4..31 chars)
		int slen = 0;
		while(slen < 32 && pos + slen < sz && data[pos + slen] >= 32 && data[pos + slen] <= 126) slen++;
		if(slen < 4 || slen >= 32 || pos + slen >= sz || data[pos + slen] != 0) {
			pos++;
			continue;
		}

		long T = pos;
		if(T + 84 > sz) {
			pos++;
			continue;
		}

		// verify tag1=1 at T+32, tag2=2 at T+68
		uint32_t tag1, tag2;
		memcpy(&tag1, data + T + 32, 4);
		memcpy(&tag2, data + T + 68, 4);
		if(tag1 != 1 || tag2 != 2) {
			pos++;
			continue;
		}

		char type_name[32];
		memcpy(type_name, data + T, (size_t)slen);
		type_name[slen] = '\0';

		// resolve type enum ONCE — all decisions use integer from here
		NsdEntityType eType = nsdTypeFromString(type_name);

		// read position from T+72 (3 floats)
		float px, py, pz;
		memcpy(&px, data + T + 72, 4);
		memcpy(&py, data + T + 76, 4);
		memcpy(&pz, data + T + 80, 4);

		if(fabsf(px) > 100000.0f || fabsf(py) > 100000.0f || fabsf(pz) > 100000.0f) {
			pos++;
			continue;
		}

		// region types (integer check)
		if(nsdTypeIsRegion(eType)) {
			char className[48] = {};
			uint32_t tag15;
			memcpy(&tag15, data + T + 104, 4);
			if(tag15 == 15 && T + 108 < sz) {
				const char *cn = (const char *)(data + T + 108);
				size_t clen	   = strnlen(cn, 40);
				if(clen > 0 && clen < 40) strncpy(className, cn, 47);
			}

			float eMin[3] = {-2.0f, -2.0f, -2.0f};
			float eMax[3] = {2.0f, 2.0f, 2.0f};
			for(long scan = T + 140; scan + 28 < sz && scan < T + 400; scan += 4) {
				uint32_t marker;
				memcpy(&marker, data + scan, 4);
				if(marker == 2) {
					float f6[6];
					memcpy(f6, data + scan + 4, 24);
					if(f6[0] <= f6[3] && f6[1] <= f6[4] && f6[2] <= f6[5] && fabsf(f6[0]) < 500.0f &&
					   fabsf(f6[3]) < 500.0f && fabsf(f6[1]) < 500.0f && fabsf(f6[4]) < 500.0f &&
					   fabsf(f6[2]) < 500.0f && fabsf(f6[5]) < 500.0f &&
					   (fabsf(f6[0]) > 0.01f || fabsf(f6[3]) > 0.01f)) {
						eMin[0] = f6[0];
						eMin[1] = f6[1];
						eMin[2] = f6[2];
						eMax[0] = f6[3];
						eMax[1] = f6[4];
						eMax[2] = f6[5];
						break;
					}
				}
			}

			if(regCount >= regCap) {
				regCap *= 2;
				regs = (NSDRegionRef *)realloc(regs, regCap * sizeof(NSDRegionRef));
			}
			NSDRegionRef &reg = regs[regCount];
			memset(&reg, 0, sizeof(reg));
			reg.position[0]	 = px;
			reg.position[1]	 = py;
			reg.position[2]	 = pz;
			reg.extentMin[0] = eMin[0];
			reg.extentMin[1] = eMin[1];
			reg.extentMin[2] = eMin[2];
			reg.extentMax[0] = eMax[0];
			reg.extentMax[1] = eMax[1];
			reg.extentMax[2] = eMax[2];
			strncpy(reg.className, className, 47);
			reg.isCameraRegion = (eType == NSD_TYPE_CAMERA_REGION) ? 1 : 0;
			regCount++;

			pos = T + slen + 32;
			continue;
		}

		// skip types (integer check)
		if(nsdTypeIsSkip(eType) || nsdTypeIsScene(eType) || nsdTypeIsEffect(eType)) {
			// extract ghost spawn before skipping
			if(eType == NSD_TYPE_GHOST && outHasGhost && !(*outHasGhost) && outGhostPos && outGhostRot) {
				outGhostPos[0] = px;
				outGhostPos[1] = py;
				outGhostPos[2] = pz;
				float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
				memcpy(&rotX, data + T + 36, 4);
				memcpy(&rotY, data + T + 40, 4);
				memcpy(&rotZ, data + T + 44, 4);
				outGhostRot[0] = rotX;
				outGhostRot[1] = rotY;
				outGhostRot[2] = rotZ;
				*outHasGhost   = true;
				printf("[NSDEntityResolver] Ghost spawn: (%.1f, %.1f, %.1f) rot (%.1f, %.1f, %.1f)\n", px, py, pz, rotX,
					   rotY, rotZ);
			}
			pos = T + slen + 32;
			continue;
		}

		// model entity
		float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		memcpy(&rotX, data + T + 36, 4);
		memcpy(&rotY, data + T + 40, 4);
		memcpy(&rotZ, data + T + 44, 4);

		// scan for tag4 (model name) starting at T+140, in 36-byte blocks
		const char *foundModel = nullptr;
		size_t foundLen		   = 0;
		for(long blk = T + 140; blk + 36 <= sz && blk < T + 140 + 36 * 4; blk += 36) {
			uint32_t btag;
			memcpy(&btag, data + blk, 4);
			if(btag == 4 && blk + 4 + 2 < sz) {
				const char *str = (const char *)(data + blk + 4);
				size_t sl		= strnlen(str, 60);
				if(sl >= 2 && sl < 60) {
					bool printable = true;
					for(size_t c = 0; c < sl; c++) {
						if(str[c] < 32 || str[c] > 126) {
							printable = false;
							break;
						}
					}
					if(printable) {
						foundModel = str;
						foundLen   = sl;
					}
				}
				break;
			}
		}

		// filter out Maya leftover geometry names embedded in tag4.
		// these are internal 3D editor names, not valid model references.
		if(foundModel) {
			if(prefixMatch_ci(foundModel, "polySurface") || prefixMatch_ci(foundModel, "pCube") ||
			   prefixMatch_ci(foundModel, "pSphere") || prefixMatch_ci(foundModel, "pCylinder") ||
			   prefixMatch_ci(foundModel, "pPlane") || prefixMatch_ci(foundModel, "pTorus")) {
				foundModel = nullptr;
				foundLen   = 0;
			}
		}

		// if no tag4, use entity class model file from enum table
		char modelBuf[64];
		if(!foundModel) {
			const char *alias = nsdTypeModelFile(eType);
			if(alias) {
				snprintf(modelBuf, sizeof(modelBuf), "%s", alias);
				foundModel = modelBuf;
				foundLen   = strlen(modelBuf);
			} else {
				snprintf(modelBuf, sizeof(modelBuf), "%s", type_name);
				foundModel = modelBuf;
				foundLen   = strlen(modelBuf);
			}
		}

		if(foundModel && foundLen >= 2) {
			if(entCount >= entCap) {
				entCap *= 2;
				ents = (NSDEntityRef *)realloc(ents, entCap * sizeof(NSDEntityRef));
			}
			NSDEntityRef &ref = ents[entCount];
			memset(&ref, 0, sizeof(ref));

			if(foundLen > 4 && stricmp_local(foundModel + foundLen - 4, ".nod") == 0)
				strncpy(ref.model_name, foundModel, 63);
			else
				snprintf(ref.model_name, 63, "%s.nod", foundModel);
			ref.model_name[63] = '\0';

			strncpy(ref.type_name, type_name, 63);
			ref.type_name[63] = '\0';
			ref.type		  = eType;

			ref.position[0] = px;
			ref.position[1] = py;
			ref.position[2] = pz;
			ref.rotation[0] = rotX;
			ref.rotation[1] = rotY;
			ref.rotation[2] = rotZ;
			entCount++;
		}

		pos = T + slen + 32;
	}

	// sky model scan
	// sky dome is stored as a special non-entity metadata block in the NSD.
	// format at offset P:
	// P+ 0: model base name (null-terminated, e.g. "V5_Sky_Funnel")
	// P+N+1: extension string "nod\0" (immediately after the null terminator)
	// ...padding to 32 bytes...
	// P+32: same base name again
	// P+32+N+1: extension string "nad\0"
	// the name contains "sky" (case-insensitive).
	if(outSkyModelName) {
		for(long sp = 0; sp + 51 <= sz; sp++) {
			// look for printable string starting here
			int sl2 = 0;
			while(sl2 < 28 && sp + sl2 < sz && data[sp + sl2] >= 32 && data[sp + sl2] <= 126) sl2++;
			if(sl2 < 4 || sl2 >= 28 || sp + sl2 >= sz || data[sp + sl2] != 0) continue;

			const char *s = (const char *)(data + sp);

			// must contain "sky" (case-insensitive)
			bool hasSky = false;
			for(int ci = 0; ci <= sl2 - 3; ci++) {
				if(tolower((unsigned char)s[ci]) == 's' && tolower((unsigned char)s[ci + 1]) == 'k' &&
				   tolower((unsigned char)s[ci + 2]) == 'y') {
					hasSky = true;
					break;
				}
			}
			if(!hasSky) continue;

			// check for "nod" extension immediately after null terminator
			long extOff = sp + sl2 + 1;
			if(extOff + 3 > sz) continue;
			if(data[extOff] != 'n' || data[extOff + 1] != 'o' || data[extOff + 2] != 'd') continue;

			// verify: same name at sp+32 with "nad" after its null terminator
			if(sp + 32 + sl2 + 4 > sz) continue;
			const char *s2 = (const char *)(data + sp + 32);
			if(strncmp(s, s2, (size_t)sl2) != 0) continue;
			long extOff2 = sp + 32 + sl2 + 1;
			if(data[extOff2] != 'n' || data[extOff2 + 1] != 'a' || data[extOff2 + 2] != 'd') continue;

			snprintf(outSkyModelName, 64, "%s.nod", s);
			printf("[NSDEntityResolver] Sky model found: %s\n", outSkyModelName);
			break;
		}
	}

	free(data);

	*outEntities	= ents;
	*outEntityCount = entCount;
	*outRegions		= regs;
	*outRegionCount = regCount;

	if(entCount > 0 || regCount > 0) {
		uint32_t trigCount = 0, camCount = 0;
		for(uint32_t i = 0; i < regCount; i++) {
			if(regs[i].isCameraRegion)
				camCount++;
			else
				trigCount++;
		}
		printf("[NSDEntityResolver] NSD: %u model refs, %u triggers, %u cameras in %s\n", entCount, trigCount, camCount,
			   nsdPath);
	}
}

/*
 */
void NSDEntityResolver::extractLightsFromNSD(const char *nsdPath, NSDLightRef **outLights, uint32_t *outLightCount) {
	*outLights	   = nullptr;
	*outLightCount = 0;

	FILE *fp = fopen(nsdPath, "rb");
	if(!fp) return;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(sz < 200) {
		fclose(fp);
		return;
	}

	uint8_t *data = (uint8_t *)malloc((size_t)sz);
	fread(data, 1, (size_t)sz, fp);
	fclose(fp);

	uint32_t cap = 128, count = 0;
	NSDLightRef *lights = (NSDLightRef *)malloc(cap * sizeof(NSDLightRef));

	int totalA1Blocks = 0;
	long pos		  = 4; // start at 4 so we can read T-4

	while(pos + 144 < sz) {

		// check for 0xa1 marker at pos-4
		uint32_t marker;
		memcpy(&marker, data + pos - 4, 4);
		if(marker != 0xa1) {
			pos++;
			continue;
		}

		// pos is the start of what should be a type name string
		long T = pos;

		// verify printable null-terminated string at T (4..48 chars)
		int slen = 0;
		while(slen < 48 && T + slen < sz && data[T + slen] >= 32 && data[T + slen] <= 126) slen++;
		if(slen < 4 || slen >= 48 || T + slen >= sz || data[T + slen] != 0) {
			pos++;
			continue;
		}

		// need at least T+144 bytes for the full block
		if(T + 144 > sz) {
			pos++;
			continue;
		}

		// verify TAG_2=2 at T+128 (position block marker)
		uint32_t tag2;
		memcpy(&tag2, data + T + 128, 4);
		if(tag2 != 2) {
			pos++;
			continue;
		}

		totalA1Blocks++;

		char type_name[48];
		memcpy(type_name, data + T, (size_t)slen);
		type_name[slen] = '\0';

		// check if this is a light-related entity
		bool isLight = false;
		if(strstr(type_name, "lightCone") || strstr(type_name, "LightCone") || strstr(type_name, "lightHalo") ||
		   strstr(type_name, "LightHalo") || strstr(type_name, "lvlLight") || strstr(type_name, "lvllight") ||
		   strstr(type_name, "cLight") || strstr(type_name, "CLight")) {
			isLight = true;
		}

		if(!isLight) {
			pos = T + slen + 4;
			continue;
		}

		// read position from T+132 (3 floats after TAG_2)
		float px, py, pz;
		memcpy(&px, data + T + 132, 4);
		memcpy(&py, data + T + 136, 4);
		memcpy(&pz, data + T + 140, 4);

		if(fabsf(px) > 100000.0f || fabsf(py) > 100000.0f || fabsf(pz) > 100000.0f) {
			pos = T + slen + 4;
			continue;
		}

		// read rotation from T+96 (3 floats after TAG_1 at T+92)
		float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		uint32_t tag1;
		memcpy(&tag1, data + T + 92, 4);
		if(tag1 == 1) {
			memcpy(&rotX, data + T + 96, 4);
			memcpy(&rotY, data + T + 100, 4);
			memcpy(&rotZ, data + T + 104, 4);
		}

		if(count >= cap) {
			cap *= 2;
			lights = (NSDLightRef *)realloc(lights, cap * sizeof(NSDLightRef));
		}

		NSDLightRef &lr = lights[count];
		lr.position[0]	= px;
		lr.position[1]	= py;
		lr.position[2]	= pz;
		lr.rotation[0]	= rotX;
		lr.rotation[1]	= rotY;
		lr.rotation[2]	= rotZ;
		strncpy(lr.className, type_name, 47);
		lr.className[47] = '\0';
		count++;

		// advance past this block (each 0xa1 block is ~204 bytes)
		pos = T + 164;
	}
	// pass 2: Standard entity blocks (tag1=1 at T+32, tag2=2 at T+68)
	// some light sources use the standard entity block format rather
	// than the 0xa1 format.  Known types:
	// halo_white_dim       -- ambient glow halos at scaffold lights
	// sparkfountain_test   -- spark effect emitters (emit light)
	// *Light / *light      -- any explicitly named light entity
	// position is at T+72 (3 floats), rotation at T+36.
	int stdLightCount = 0;
	pos				  = 0;
	while(pos + 84 < sz) {
		int slen = 0;
		while(slen < 32 && pos + slen < sz && data[pos + slen] >= 32 && data[pos + slen] <= 126) slen++;
		if(slen < 4 || slen >= 32 || pos + slen >= sz || data[pos + slen] != 0) {
			pos++;
			continue;
		}
		long T = pos;
		if(T + 84 > sz) {
			pos++;
			continue;
		}

		uint32_t t1, t2;
		memcpy(&t1, data + T + 32, 4);
		memcpy(&t2, data + T + 68, 4);
		if(t1 != 1 || t2 != 2) {
			pos++;
			continue;
		}

		char type_name[32];
		memcpy(type_name, data + T, (size_t)slen);
		type_name[slen] = '\0';

		// match known light-emitting standard entity types
		bool isStdLight = false;
		if(strcmp(type_name, "halo_white_dim") == 0 || strstr(type_name, "sparkfountain") ||
		   strstr(type_name, "nydusExpLight") || strstr(type_name, "cLight") || strstr(type_name, "CLight")) {
			isStdLight = true;
		}

		if(!isStdLight) {
			pos = T + slen + 32;
			continue;
		}

		float px, py, pz;
		memcpy(&px, data + T + 72, 4);
		memcpy(&py, data + T + 76, 4);
		memcpy(&pz, data + T + 80, 4);

		if(fabsf(px) > 100000.0f || fabsf(py) > 100000.0f || fabsf(pz) > 100000.0f) {
			pos = T + slen + 32;
			continue;
		}

		float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		memcpy(&rotX, data + T + 36, 4);
		memcpy(&rotY, data + T + 40, 4);
		memcpy(&rotZ, data + T + 44, 4);

		if(count >= cap) {
			cap *= 2;
			lights = (NSDLightRef *)realloc(lights, cap * sizeof(NSDLightRef));
		}

		NSDLightRef &lr = lights[count];
		lr.position[0]	= px;
		lr.position[1]	= py;
		lr.position[2]	= pz;
		lr.rotation[0]	= rotX;
		lr.rotation[1]	= rotY;
		lr.rotation[2]	= rotZ;
		strncpy(lr.className, type_name, 47);
		lr.className[47] = '\0';
		count++;
		stdLightCount++;

		pos = T + slen + 32;
	}

	free(data);

	printf("[NSDLightScan] Total: %d 0xa1 lights + %d std lights = %u total\n", totalA1Blocks, stdLightCount, count);

	*outLights	   = lights;
	*outLightCount = count;

	if(count > 0) {
		printf("[NSDEntityResolver] NSD: %u light entities extracted from %s\n", count, nsdPath);
	}
}
