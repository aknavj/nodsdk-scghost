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

#include "NSAParser.h"

/*
*/
namespace {

	/*
	*/
	char *TrimInPlace(char *s) {

		while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') ++s;
		char *end = s + strlen(s) - 1;
		while(end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) *end-- = '\0';
		return s;
	}

} // namespace

/*
 */
int NSAParser::parse(const char *filePath) {

	FILE *f = fopen(filePath, "r");
	if(!f) return 0;

	uint32_t matCap	  = 256;
	md.materials	  = (Material *)calloc(matCap, sizeof(Material));
	md.material_count = 0;

	Material *cur  = nullptr;
	int braceDepth = 0;
	char line[512];

	while(fgets(line, sizeof(line), f)) {
		size_t len = strlen(line);
		while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';

		char *trimmed = TrimInPlace(line);
		if(trimmed[0] == '\0') continue;

		if(strcmp(trimmed, "{") == 0) {
			++braceDepth;
			continue;
		}
		if(strcmp(trimmed, "}") == 0) {
			--braceDepth;
			if(braceDepth <= 0) {
				cur		   = nullptr;
				braceDepth = 0;
			}
			continue;
		}

		if(braceDepth == 0) {
			if(md.material_count >= matCap) {
				matCap *= 2;
				md.materials = (Material *)realloc(md.materials, matCap * sizeof(Material));
			}
			cur = &md.materials[md.material_count++];
			memset(cur, 0, sizeof(Material));
			strncpy(cur->name, trimmed, 127);
		} else if(cur && cur->prop_count < MAX_PROPS) {

			MaterialProperty *p = &cur->props[cur->prop_count];
			memset(p, 0, sizeof(MaterialProperty));

			// first token = key
			char *sp	 = trimmed;
			char *keyEnd = sp;
			while(*keyEnd && *keyEnd != ' ' && *keyEnd != '\t') ++keyEnd;

			size_t kLen = (size_t)(keyEnd - sp);
			if(kLen > 127) kLen = 127;
			memcpy(p->key, sp, kLen);
			p->key[kLen] = '\0';

			char *val = keyEnd;
			while(*val == ' ' || *val == '\t') ++val;
			strncpy(p->value, val, 255);

			cur->prop_count++;
		}
	}

	fclose(f);
	return 1;
}

/*
 */
int NSAParser::serialize(const char *filePath) const {

	FILE *f = fopen(filePath, "w");
	if(!f) return 0;

	for(uint32_t i = 0; i < md.material_count; ++i) {
		const Material *m = &md.materials[i];
		fprintf(f, "%s\r\n{\r\n", m->name);
		for(uint32_t j = 0; j < m->prop_count; ++j) {
			const MaterialProperty *p = &m->props[j];
			if(p->value[0])
				fprintf(f, "\t%s\t%s\r\n", p->key, p->value);
			else
				fprintf(f, "\t%s\r\n", p->key);
		}
		fprintf(f, "}\r\n");
	}

	fclose(f);
	return 1;
}
