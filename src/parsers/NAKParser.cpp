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

#include "NAKParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
int NAKParser::parse(const char *filePath) {

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

	// read entire file as text
	char *buf = (char *)malloc((size_t)fsize + 1);
	if(!buf) {
		fclose(fp);
		return 0;
	}
	if((long)fread(buf, 1, (size_t)fsize, fp) != fsize) {
		free(buf);
		fclose(fp);
		return 0;
	}
	fclose(fp);
	buf[fsize] = '\0';

	// parse: first line = count, subsequent lines = keyframes
	// NOTE: Some .nak files use \r\n\0 line endings (NUL after CR/LF),
	// so we iterate by offset rather than relying on null-terminated strings.
	int lineNum		 = 0;
	m.keyframe_count = 0;
	size_t off		 = 0;

	while(off < (size_t)fsize) {
		// skip NUL bytes, CR, LF between lines
		while(off < (size_t)fsize && (buf[off] == '\0' || buf[off] == '\r' || buf[off] == '\n')) ++off;
		if(off >= (size_t)fsize) break;

		// find end of this line
		size_t lineStart = off;
		while(off < (size_t)fsize && buf[off] != '\0' && buf[off] != '\r' && buf[off] != '\n') ++off;

		char saved = buf[off < (size_t)fsize ? off : fsize - 1];
		if(off < (size_t)fsize) buf[off] = '\0';

		char *p = buf + lineStart;
		while(*p == ' ' || *p == '\t') ++p;

		if(*p != '\0') {
			if(lineNum == 0) {
				// first non-empty line: keyframe count (informational)
				// uint32_t expectedCount = (uint32_t)atoi(p);
			} else if(m.keyframe_count < MAX_KEYFRAMES) {

				// keyframe line: time fov near far
				CameraKeyframe &kf = m.keyframes[m.keyframe_count];
				memset(&kf, 0, sizeof(kf));
				int n = sscanf(p, "%f %f %f %f", &kf.time, &kf.fov, &kf.near_clip, &kf.far_clip);
				if(n >= 1) {
					m.keyframe_count++;
				}
			}
			lineNum++;
		}

		if(off < (size_t)fsize) buf[off] = saved;
	}

	free(buf);
	return m.keyframe_count > 0;
}

/*
 */
int NAKParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}
