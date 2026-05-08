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

#include "NNBParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
int NNBParser::parse(const char *filePath) {

	FILE *fp = fopen(filePath, "rb");
	if(!fp) return 0;

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(fsize < 12) {
		fclose(fp);
		return 0;
	}
	m.file_size = (size_t)fsize;

	uint8_t *data = (uint8_t *)malloc((size_t)fsize);
	if(!data) {
		fclose(fp);
		return 0;
	}
	if((long)fread(data, 1, (size_t)fsize, fp) != fsize) {
		free(data);
		fclose(fp);
		return 0;
	}
	fclose(fp);

	memcpy(&m.version, data + 0, 4);
	memcpy(&m.anim_count, data + 4, 4);
	memcpy(&m.data_size, data + 8, 4);

	if(m.version != 1) {
		free(data);
		return 0;
	}

	if(m.anim_count == 0 || (size_t)fsize <= 12) {
		m.names_found = 0;
		printf("[NNB] %s: version=%u, 0 anims (empty)\n", filePath, m.version);
		free(data);
		return 1;
	}

	// scan for .nad filename patterns in the binary data.
	// animation names appear embedded at various offsets.  We look for
	// runs of printable ASCII ending in ".nad" (case-insensitive).
	m.names_found = 0;
	size_t i	  = 12;
	while(i + 4 < (size_t)fsize && m.names_found < MAX_ANIMS) {
		// check for ".nad" / ".NAD" suffix at position i
		if((data[i] == '.' && ((data[i + 1] == 'n' && data[i + 2] == 'a' && data[i + 3] == 'd') ||
							   (data[i + 1] == 'N' && data[i + 2] == 'A' && data[i + 3] == 'D')))) {

			// walk backwards to find start of name
			size_t end	 = i + 4;
			size_t start = i;
			while(start > 12 && data[start - 1] >= 32 && data[start - 1] < 127) {
				--start;
				if(start + 64 < end) break; // sanity limit
			}

			size_t len = end - start;
			if(len >= 5 && len < 64) { // at least "X.nad"
				AnimEntry *e = &m.anims[m.names_found];
				memcpy(e->name, data + start, len);
				e->name[len] = '\0';
				e->offset	 = start;
				++m.names_found;
			}
			i = end; // skip past this match
		} else {
			++i;
		}
	}

	printf("[NNB] %s: version=%u, %u anims (header), %u names found\n", filePath, m.version, m.anim_count,
		   m.names_found);

	free(data);
	return 1;
}

/*
 */
int NNBParser::serialize(const char * /*filePath*/) const {

	return 0;
}
