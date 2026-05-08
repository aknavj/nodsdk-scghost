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

#include "NFXParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

/*
 */
NFXParser::~NFXParser() {

	if(m.blocks) {
		free(m.blocks);
		m.blocks = nullptr;
	}
	if(m.raw_text) {
		free(m.raw_text);
		m.raw_text = nullptr;
	}
}

namespace {

	// known block-start keywords. Any unrecognized first word on a line
	// that is followed by a name and eventually "end" also works, but we
	// specifically recognise these to disambiguate property lines.
	int is_block_keyword(const char *word) {

		static const char *KEYWORDS[] = {
			"controller",	"smoke",		   "sparks",		"light",	  "shockwave",	   "glow",
			"muzzle_flash", "trail",		   "decal",			"model",	  "energybeam",	   "rain",
			"heatshimmer",	"mushroom",		   "nukemushroom",	"bubbles",	  "volumefog",	   "psishield",
			"lightningarc", "lensFlare",	   "melee",			"spikes",	  "shells",		   "gun",
			"billboard",	"dirtTrail",	   "dustoff",		"dustshape",  "glass",		   "hydraspikes",
			"flashtime",	"lightningattack", "lasersight",	"laserfan",	  "lasertripwire", "damage",
			"ctrldamage",	"radiuseffect",	   "spawner",		"teleport",	  "purifierbeam",  "chargeup",
			"partcylinder", "parttube",		   "partplane",		"project",	  "projectcube",   "boxproject",
			"force",		"motionBlur",	   "meshTypeFlash", "linelerp",	  "gibomatic2k",   "actorburn",
			"actorglow",	"actorirradiate",  "ambient",		"shaderctrl", "siegetankFX",   "goliathFX",
			"vultureFX",	"exp_fire",		   "hellfire",		"fxmodel",	  nullptr};
		
		for(int i = 0; KEYWORDS[i]; ++i) {
			if(strcmp(word, KEYWORDS[i]) == 0) return 1;
		}

		return 0;
	}

} // namespace

/*
 */
int NFXParser::parse(const char *filePath) {

	FILE *fp = fopen(filePath, "rb");
	if(!fp) return 0;

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(fsize <= 0) {
		fclose(fp);
		return 0;
	}

	m.raw_text = (char *)malloc((size_t)fsize + 1);
	if(!m.raw_text) {
		fclose(fp);
		return 0;
	}
	if((long)fread(m.raw_text, 1, (size_t)fsize, fp) != fsize) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	m.raw_text[fsize] = '\0';
	m.raw_text_len	  = (size_t)fsize;
	m.file_size		  = (size_t)fsize;

	uint32_t capacity = 256;
	m.blocks		  = (Block *)calloc(capacity, sizeof(Block));
	if(!m.blocks) return 0;
	m.block_count = 0;

	char *line = m.raw_text;
	Block *cur = nullptr;

	while(line && *line) {
		// find end of line
		char *eol = line;
		while(*eol && *eol != '\n' && *eol != '\r') ++eol;
		char saved = *eol;
		*eol	   = '\0';

		char *p = line;
		while(*p == ' ' || *p == '\t') ++p;

		// skip blank lines and comments
		if(*p == '\0' || *p == ';') {
			*eol = saved;
			line = eol;
			if(*line == '\r') ++line;
			if(*line == '\n') ++line;
			continue;
		}

		char firstWord[64];
		int fi	 = 0;
		char *wp = p;
		while(*wp && *wp != ' ' && *wp != '\t' && fi < 63) {
			firstWord[fi++] = *wp++;
		}
		firstWord[fi] = '\0';

		if(strcmp(firstWord, "end") == 0) {
			cur	 = nullptr;
			*eol = saved;
			line = eol;
			if(*line == '\r') ++line;
			if(*line == '\n') ++line;
			continue;
		}

		// check for block start keyword
		if(is_block_keyword(firstWord)) {
			while(*wp == ' ' || *wp == '\t') ++wp;
			char blockName[64];
			int ni = 0;
			while(*wp && *wp != ' ' && *wp != '\t' && ni < 63) {
				blockName[ni++] = *wp++;
			}
			blockName[ni] = '\0';

			if(m.block_count >= capacity) {
				capacity *= 2;
				Block *nb = (Block *)realloc(m.blocks, capacity * sizeof(Block));
				if(!nb) break;
				m.blocks = nb;
				memset(m.blocks + m.block_count, 0, (capacity - m.block_count) * sizeof(Block));
			}

			cur = &m.blocks[m.block_count];
			memset(cur, 0, sizeof(Block));
			strncpy(cur->type, firstWord, sizeof(cur->type) - 1);
			strncpy(cur->name, blockName, sizeof(cur->name) - 1);
			m.block_count++;
		} else if(cur && cur->property_count < MAX_PROPERTIES) {
			Property &prop = cur->properties[cur->property_count];
			strncpy(prop.key, firstWord, sizeof(prop.key) - 1);
			while(*wp == ' ' || *wp == '\t') ++wp;
			if(*wp) {
				strncpy(prop.value, wp, sizeof(prop.value) - 1);
			}
			cur->property_count++;
		}

		// advance to next line
		*eol = saved;
		line = eol;
		if(*line == '\r') ++line;
		if(*line == '\n') ++line;
	}

	// count controller vs effect blocks
	m.controller_count = 0;
	m.effect_count	   = 0;
	for(uint32_t i = 0; i < m.block_count; ++i) {
		if(strcmp(m.blocks[i].type, "controller") == 0)
			m.controller_count++;
		else
			m.effect_count++;
	}

	return m.block_count > 0;
}

/*
 */
int NFXParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}

/*
 */
const NFXParser::Block *NFXParser::findBlock(const char *name) const {

	if(!m.blocks || !name) return nullptr;
	for(uint32_t i = 0; i < m.block_count; ++i) {
#ifdef _WIN32
		if(_stricmp(m.blocks[i].name, name) == 0)
#else
		if(strcasecmp(m.blocks[i].name, name) == 0)
#endif
			return &m.blocks[i];
	}
	return nullptr;
}

/*
 */
uint32_t NFXParser::countBlockType(const char *type) const {

	if(!m.blocks || !type) return 0;
	uint32_t count = 0;
	for(uint32_t i = 0; i < m.block_count; ++i) {
		if(strcmp(m.blocks[i].type, type) == 0) count++;
	}
	return count;
}
