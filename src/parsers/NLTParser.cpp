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

#include "NLTParser.h"

/*
 */
int NLTParser::parse(const char *filePath) {

	FILE *f = fopen(filePath, "rb");
	if(!f) return 0;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if(len <= 0) {
		fclose(f);
		return 0;
	}

	td.text	  = (char *)malloc((size_t)len + 1);
	size_t rd = fread(td.text, 1, (size_t)len, f);
	fclose(f);

	td.text[rd] = '\0';
	td.text_len = rd;

	return td.text_len > 0;
}

/*
 */
int NLTParser::serialize(const char *filePath) const {

	FILE *f = fopen(filePath, "wb");
	if(!f) return 0;

	if(td.text && td.text_len > 0) fwrite(td.text, 1, td.text_len, f);

	fclose(f);
	return 1;
}
