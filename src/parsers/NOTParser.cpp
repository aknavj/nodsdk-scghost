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

#include "NOTParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 */
NOTParser::~NOTParser() {
	clear();
}

void NOTParser::clear() {
	if(m.templates) {
		for(uint32_t i = 0; i < m.num_templates; ++i) {
			if(m.templates[i].properties) {
				free(m.templates[i].properties);
				m.templates[i].properties = nullptr;
			}
			if(m.templates[i].comment) {
				free(m.templates[i].comment);
				m.templates[i].comment = nullptr;
			}
		}
		free(m.templates);
		m.templates = nullptr;
	}
	if(m.raw_data) {
		free(m.raw_data);
		m.raw_data = nullptr;
	}
	memset(&m, 0, sizeof(m));
}

/*
 */
namespace {

	/*
	*/
	uint32_t ReadU32(const uint8_t *p) {

		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}

	/*
	*/
	float ReadF32(const uint8_t *p) {

		uint32_t bits = ReadU32(p);
		float f;
		memcpy(&f, &bits, 4);
		return f;
	}

} // namespace

/*
 */
int NOTParser::parse(const char *filePath) {
	size_t size = 0;
	uint8_t *data = FileUtils::readBinaryFile(filePath, &size);
	if(!data) return reject("cannot read NOT file");
	const int result = parseBuffer(data, size);
	free(data);
	return result;
}

int NOTParser::reject(const char *message) {
	clear();
	last_error = message;
	return 0;
}

int NOTParser::parseBuffer(const uint8_t *data, size_t size) {
	last_error = "";
	if(!data || size < 12 || ReadU32(data) != MAGIC)
		return reject("expected NOT header with magic NOT\\x10");
	const uint32_t version = ReadU32(data + 4);
	const uint32_t count = ReadU32(data + 8);
	if(version != 1) return reject("unsupported NOT version");
	if(count == 0 || count > MAX_TEMPLATES || count > (size - 12) / 36)
		return reject("invalid or truncated NOT table of contents");
	auto *copy = static_cast<uint8_t *>(malloc(size));
	if(!copy) return reject("out of memory reading NOT");
	memcpy(copy, data, size);
	clear();
	m.raw_data = copy;
	m.raw_data_size = m.file_size = size;
	m.version = version;
	m.num_templates = count;
	m.templates = static_cast<Template *>(calloc(count, sizeof(Template)));
	if(!m.templates) return reject("out of memory reading NOT templates");
	if(!parse_toc(copy, size)) return reject("invalid NOT template offsets");
	for(uint32_t i = 0; i < count; ++i) {
		if(!parse_template_data(i, copy, size)) return reject("truncated NOT template header");
	}
	return 1;
}

/*
 */
int NOTParser::parse_toc(const uint8_t *data, size_t size) {

	for(uint32_t i = 0; i < m.num_templates; ++i) {
		size_t off			 = 12 + (size_t)i * 36;
		const uint8_t *entry = data + off;

		// name: 32 bytes, null-terminated
		memset(m.templates[i].name, 0, 33);
		for(int j = 0; j < 32 && entry[j] != 0; ++j) {
			m.templates[i].name[j] = (char)entry[j];
		}

		m.templates[i].data_offset = ReadU32(entry + 32);

		if(m.templates[i].data_offset < 12 + size_t(m.num_templates) * 36 ||
		   m.templates[i].data_offset >= size ||
		   (i && m.templates[i].data_offset <= m.templates[i - 1].data_offset)) return 0;
	}
	return 1;
}

/*
 */
int NOTParser::parse_template_data(uint32_t index, const uint8_t *data, size_t size) {

	Template &tmpl = m.templates[index];
	uint32_t start = tmpl.data_offset;

	size_t end;
	if(index + 1 < m.num_templates) {
		end = m.templates[index + 1].data_offset;
	} else {
		end = size;
	}

	if(end - start < 40 || end - start > UINT32_MAX) return 0;
	tmpl.data_size = static_cast<uint32_t>(end - start);

	const uint8_t *tdata = data + start;

	// template header: num_properties(4) + flags(4) + parent(32) = 40 bytes
	tmpl.num_properties = ReadU32(tdata);
	tmpl.flags			= ReadU32(tdata + 4);

	memset(tmpl.parent, 0, 33);
	for(int j = 0; j < 32 && tdata[8 + j] != 0; ++j) {
		tmpl.parent[j] = (char)tdata[8 + j];
	}

	size_t propDataSize = tmpl.data_size - 40;
	if(tmpl.num_properties == 0) tmpl.properties_complete = true;
	if(tmpl.num_properties > 0 && propDataSize > 0) {
		if(!parse_properties(tmpl, tdata + 40, propDataSize)) {
			return 0;
		}
	}

	return 1;
}

/*
 */
int NOTParser::parse_properties(Template &tmpl, const uint8_t *propData, size_t propDataSize) {

	const uint32_t limit = tmpl.num_properties < MAX_PROPERTIES ? tmpl.num_properties : MAX_PROPERTIES;
	tmpl.properties = (Property *)calloc(limit, sizeof(Property));
	if(!tmpl.properties) return 0;

	size_t pos		= 0;
	uint32_t parsed = 0;

	for(uint32_t p = 0; p < limit && pos + 4 <= propDataSize; ++p) {
		Property &prop = tmpl.properties[p];
		uint32_t val   = ReadU32(propData + pos);
		prop.value_offset = static_cast<size_t>(propData - m.raw_data) + pos + 4;

		// boolean property (prop_id = 1)
		if(val == PROP_BOOL_ID && pos + 5 <= propDataSize) {
			prop.prop_id	= val;
			prop.value[0]	= propData[pos + 4];
			prop.value_size = 1;
			prop.is_comment = false;
			pos += 5;
			parsed++;
			continue;
		}

		// check for comment string: uint32 strlen + ASCII text ending \r\n\0
		if(val > 0 && val < 256 && pos + 4 + val <= propDataSize) {
			const uint8_t *candidate = propData + pos + 4;
			// check if it ends with \r\n\0
			if(val >= 3 && candidate[val - 3] == '\r' && candidate[val - 2] == '\n' && candidate[val - 1] == '\0') {

				// verify all bytes are printable ASCII or whitespace
				int isAscii = true;
				for(uint32_t c = 0; c < val - 1; ++c) {
					uint8_t b = candidate[c];
					if(b != 0 && (b < 32 || b > 126) && b != '\r' && b != '\n' && b != '\t') {
						isAscii = false;
						break;
					}
				}
				if(isAscii) {
					// it's a developer comment
					prop.prop_id	= 0; // no real property ID for comments
					prop.value_size = val;
					prop.is_comment = true;

					// copy up to PROP_VALUE_SIZE into value buffer
					uint32_t copyLen = val < PROP_VALUE_SIZE ? val : PROP_VALUE_SIZE;
					memcpy(prop.value, candidate, copyLen);

					// also store full comment in template
					if(!tmpl.comment) {
						tmpl.comment_len = val - 1; // exclude null
						tmpl.comment	 = (char *)malloc(val);
						if(tmpl.comment) {
							memcpy(tmpl.comment, candidate, val);
						}
					}

					pos += 4 + val;
					parsed++;
					continue;
				}
			}
		}

		// standard property: 4 bytes ID + 32 bytes value
		if(pos + 36 <= propDataSize) {
			prop.prop_id = val;
			memcpy(prop.value, propData + pos + 4, PROP_VALUE_SIZE);
			prop.value_size = PROP_VALUE_SIZE;
			prop.is_comment = false;
			pos += 36;
			parsed++;
		} else {
			// not enough data — read what we can
			prop.prop_id	= val;
			uint32_t remain = (uint32_t)(propDataSize - pos - 4);
			if(remain > PROP_VALUE_SIZE) remain = PROP_VALUE_SIZE;
			memcpy(prop.value, propData + pos + 4, remain);
			prop.value_size = remain;
			prop.is_comment = false;
			pos += 4 + remain;
			parsed++;
			break;
		}
	}

	tmpl.property_count = parsed;
	tmpl.properties_complete = parsed == tmpl.num_properties &&
		(parsed == 0 || tmpl.properties[parsed - 1].is_comment ||
		 tmpl.properties[parsed - 1].value_size == (tmpl.properties[parsed - 1].prop_id == PROP_BOOL_ID ? 1u : 32u));
	return 1;
}

/*
 */
int NOTParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}

/*
 */
const NOTParser::Template *NOTParser::findTemplate(const char *name) const {

	if(!m.templates || !name) return nullptr;
	for(uint32_t i = 0; i < m.num_templates; ++i) {
#ifdef _WIN32
		if(_stricmp(m.templates[i].name, name) == 0)
#else
		if(strcasecmp(m.templates[i].name, name) == 0)
#endif
			return &m.templates[i];
	}
	return nullptr;
}

/*
 */
uint32_t NOTParser::countChildren(const char *parentName) const {

	if(!m.templates || !parentName) return 0;
	uint32_t count = 0;
	for(uint32_t i = 0; i < m.num_templates; ++i) {
#ifdef _WIN32
		if(_stricmp(m.templates[i].parent, parentName) == 0)
#else
		if(strcasecmp(m.templates[i].parent, parentName) == 0)
#endif
			count++;
	}
	return count;
}
