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

#include "NMSParser.h"

/*
 */
int NMSParser::parse(const char *filePath) {

	FILE *f = fopen(filePath, "r");
	if(!f) return 0;

	// allocate header comment buffer
	size_t hdrCap		  = 1024;
	ms.header_comment	  = (char *)malloc(hdrCap);
	ms.header_comment[0]  = '\0';
	ms.header_comment_len = 0;

	uint32_t entryCap = 512;
	ms.entries		  = (MotionEntry *)calloc(entryCap, sizeof(MotionEntry));
	ms.entry_count	  = 0;

	int inHeader = true;
	char line[1024];

	while(fgets(line, sizeof(line), f)) {
		size_t len = strlen(line);
		while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';

		if(len > 0 && line[0] == ';') {
			if(inHeader) {
				size_t needed = ms.header_comment_len + len + 2;
				if(needed > hdrCap) {
					hdrCap			  = needed * 2;
					ms.header_comment = (char *)realloc(ms.header_comment, hdrCap);
				}
				strcat(ms.header_comment, line);
				strcat(ms.header_comment, "\n");
				ms.header_comment_len = strlen(ms.header_comment);
			}
			continue;
		}

		if(len == 0) continue;
		inHeader = false;

		if(ms.entry_count >= entryCap) {
			entryCap *= 2;
			ms.entries = (MotionEntry *)realloc(ms.entries, entryCap * sizeof(MotionEntry));
		}

		MotionEntry *e = &ms.entries[ms.entry_count];
		memset(e, 0, sizeof(MotionEntry));

		char *tok = strtok(line, " \t");
		if(!tok) continue;
		strncpy(e->motion_name, tok, 127);

		tok = strtok(nullptr, " \t");
		if(!tok) continue;
		strncpy(e->animation_file, tok, 127);

		while((tok = strtok(nullptr, " \t")) != nullptr && e->flag_count < MAX_FLAGS) {
			strncpy(e->flags[e->flag_count], tok, 63);
			e->flag_count++;
		}

		ms.entry_count++;
	}

	fclose(f);
	return 1;
}

/*
 */
int NMSParser::serialize(const char *filePath) const {

	FILE *f = fopen(filePath, "w");
	if(!f) return 0;

	if(ms.header_comment && ms.header_comment_len > 0) {
		fprintf(f, "%s\n", ms.header_comment);
	}

	for(uint32_t i = 0; i < ms.entry_count; ++i) {
		const MotionEntry *e = &ms.entries[i];
		fprintf(f, "%s\t%s", e->motion_name, e->animation_file);
		for(uint32_t fl = 0; fl < e->flag_count; ++fl) fprintf(f, "\t%s", e->flags[fl]);
		fprintf(f, "\n");
	}

	fclose(f);
	return 1;
}
