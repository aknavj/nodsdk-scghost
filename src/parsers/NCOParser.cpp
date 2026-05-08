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

#include "NCOParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

/*
 */
// parse a single text line of the form:
// <duration_ms> <speaker_id> [optional (SpeakerName)] <dialogue text>
int NCOParser::parseLine(const char *line, DialogueLine *out) {

	if(!line || !out) return 0;

	memset(out, 0, sizeof(DialogueLine));

	const char *p = line;
	while(*p && isspace((unsigned char)*p)) p++;
	if(*p == '\0') return 0; // blank line

	// duration (milliseconds)
	char *end = nullptr;
	long dur  = strtol(p, &end, 10);
	if(end == p) return 0; // no number found
	out->duration_ms = (uint32_t)(dur > 0 ? dur : 0);
	p				 = end;

	while(*p && isspace((unsigned char)*p)) p++;
	if(*p == '\0') return 0;

	// speaker ID
	long sid = strtol(p, &end, 10);
	if(end == p) return 0;
	out->speaker_id = (uint32_t)(sid >= 0 ? sid : 0);
	p				= end;

	while(*p && isspace((unsigned char)*p)) p++;
	if(*p == '\0') return 0; // no dialogue text

	// optional (SpeakerName) prefix
	if(*p == '(') {
		const char *close = strchr(p, ')');
		if(close && (close - p - 1) < (int)sizeof(out->speaker_name)) {
			size_t nameLen = (size_t)(close - p - 1);
			memcpy(out->speaker_name, p + 1, nameLen);
			out->speaker_name[nameLen] = '\0';

			p = close + 1;
			// skip whitespace after closing paren
			while(*p && isspace((unsigned char)*p)) p++;
		}
	}

	// dialogue text (rest of line)
	// trim trailing whitespace / CR / LF
	size_t text_len = strlen(p);
	while(text_len > 0 &&
		  (p[text_len - 1] == '\r' || p[text_len - 1] == '\n' || isspace((unsigned char)p[text_len - 1])))
		text_len--;

	if(text_len == 0) return 0;

	size_t copyLen = text_len < sizeof(out->text) - 1 ? text_len : sizeof(out->text) - 1;
	memcpy(out->text, p, copyLen);
	out->text[copyLen] = '\0';

	return 1;
}

/*
 */
int NCOParser::parse(const char *filePath) {

	FILE *fp = fopen(filePath, "rb");
	if(!fp) return 0;

	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(fsize <= 0) {
		fclose(fp);
		return 0;
	}
	m.file_size = (size_t)fsize;

	// read entire file into buffer (these are tiny — <2 KB)
	char *data = (char *)malloc((size_t)fsize + 1);
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
	data[fsize] = '\0';

	// split into lines on \n (handles both \r\n and \n)
	m.line_count	= 0;
	m.speaker_count = 0;
	memset(m.speaker_names, 0, sizeof(m.speaker_names));

	bool speakerSeen[MAX_SPEAKERS] = {};

	char *saveptr = nullptr;
	char *line	  = strtok_r(data, "\n", &saveptr);

	while(line && m.line_count < MAX_LINES) {
		DialogueLine dl;
		if(parseLine(line, &dl)) {
			m.lines[m.line_count] = dl;
			m.line_count++;

			if(dl.speaker_id < MAX_SPEAKERS) {
				if(!speakerSeen[dl.speaker_id]) {
					speakerSeen[dl.speaker_id] = true;
					m.speaker_count++;
				}

				// store speaker name if we have one and haven't stored one yet
				if(dl.speaker_name[0] != '\0' && m.speaker_names[dl.speaker_id][0] == '\0') {
					strncpy(m.speaker_names[dl.speaker_id], dl.speaker_name, sizeof(m.speaker_names[0]) - 1);
				}
			}
		}

		line = strtok_r(nullptr, "\n", &saveptr);
	}

	free(data);

	printf("[NCO] %s: %u lines, %u speakers", filePath, m.line_count, m.speaker_count);

	for(uint32_t i = 0; i < MAX_SPEAKERS; i++) {
		if(speakerSeen[i]) {
			if(m.speaker_names[i][0] != '\0')
				printf(", [%u]=%s", i, m.speaker_names[i]);
			else
				printf(", [%u]=?", i);
		}
	}
	printf("\n");

	return 1;
}

/*
 */
int NCOParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}
