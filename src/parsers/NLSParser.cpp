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

#include "NLSParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

/*
 */
NLSParser::~NLSParser() {

	free(m.entries);
	free(m.raw_text);
}

/*
*/
namespace {

	/*
	*/
	int stricmp_nls(const char *a, const char *b) {

		while(*a && *b) {
			int ca = tolower((unsigned char)*a);
			int cb = tolower((unsigned char)*b);
			if(ca != cb) return ca - cb;
			++a;
			++b;
		}
		return (unsigned char)*a - (unsigned char)*b;
	}

} // namespace

/*
 */
int NLSParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *raw	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!raw) return 0;

	m.file_size	   = file_size;
	m.raw_text_len = file_size;
	m.raw_text	   = (char *)malloc(file_size + 1);
	if(!m.raw_text) {
		free(raw);
		return 0;
	}
	memcpy(m.raw_text, raw, file_size);
	m.raw_text[file_size] = '\0';
	free(raw);

	uint32_t capacity = 256;
	m.entries		  = (StringEntry *)malloc(capacity * sizeof(StringEntry));
	if(!m.entries) return 0;
	m.entry_count = 0;

	// parse line by line
	char *line = m.raw_text;
	while(*line) {
		// find end of line
		char *eol = line;
		while(*eol && *eol != '\n' && *eol != '\r') ++eol;
		char saved = *eol;
		*eol	   = '\0';

		char *p = line;
		while(*p == ' ' || *p == '\t') ++p;

		// skip empty lines and comments
		if(*p != '\0' && *p != '#' && *p != ';') {
			// find key-value separator (first whitespace after key)
			char *keyStart = p;
			while(*p && *p != ' ' && *p != '\t') ++p;

			if(*p) {
				*p = '\0'; // null-terminate key
				++p;
				while(*p == ' ' || *p == '\t') ++p; // skip whitespace before value

				if(m.entry_count >= capacity) {
					capacity *= 2;
					if(capacity > MAX_ENTRIES) capacity = MAX_ENTRIES;
					m.entries = (StringEntry *)realloc(m.entries, capacity * sizeof(StringEntry));
					if(!m.entries) return 0;
				}

				if(m.entry_count < MAX_ENTRIES) {
					StringEntry *e = &m.entries[m.entry_count];
					strncpy(e->key, keyStart, sizeof(e->key) - 1);
					e->key[sizeof(e->key) - 1] = '\0';
					strncpy(e->value, p, sizeof(e->value) - 1);
					e->value[sizeof(e->value) - 1] = '\0';
					++m.entry_count;
				}
			}
		}

		// advance past line ending
		*eol = saved;
		line = eol;
		if(*line == '\r') ++line;
		if(*line == '\n') ++line;
	}

	printf("[NLS] %s: %u string entries\n", filePath, m.entry_count);
	return 1;
}

/*
 */
int NLSParser::serialize(const char *filePath) const {

	if(!m.raw_text || m.raw_text_len == 0) return 0;
	return FileUtils::writeBinaryFile(filePath, (const uint8_t *)m.raw_text, m.raw_text_len);
}

/*
 */
const char *NLSParser::lookupString(const char *key) const {

	for(uint32_t i = 0; i < m.entry_count; ++i) {
		if(stricmp_nls(m.entries[i].key, key) == 0) return m.entries[i].value;
	}
	return nullptr;
}
