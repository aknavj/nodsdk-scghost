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

#include "NLFXParser.h"

/*
 */
int NLFXParser::parse(const char *filePath) {

	FILE *f = fopen(filePath, "r");
	if(!f) return 0;

	fx.effect_count = 0;
	char line[256];

	while(fgets(line, sizeof(line), f) && fx.effect_count < MAX_EFFECTS) {
		size_t len = strlen(line);
		while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
		if(len == 0) continue;

		strncpy(fx.effect_names[fx.effect_count], line, 127);
		fx.effect_names[fx.effect_count][127] = '\0';
		fx.effect_count++;
	}

	fclose(f);
	return fx.effect_count > 0;
}

/*
 */
int NLFXParser::serialize(const char *filePath) const {

	FILE *f = fopen(filePath, "w");
	if(!f) return 0;

	for(uint32_t i = 0; i < fx.effect_count; ++i) fprintf(f, "%s\n", fx.effect_names[i]);

	fclose(f);
	return 1;
}
