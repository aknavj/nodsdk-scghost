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

#include "NUIParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

/*
 */
NUIParser::~NUIParser() {

	if(m.raw_text) {
		free(m.raw_text);
		m.raw_text = nullptr;
	}
}

/*
 */
NUIParser::ElementType NUIParser::parse_element_type(const char *typeStr) {

	if(!typeStr) return ELEM_UNKNOWN;
	if(strcmp(typeStr, "button") == 0) return ELEM_BUTTON;
	if(strcmp(typeStr, "textbox") == 0) return ELEM_TEXTBOX;
	if(strcmp(typeStr, "edit") == 0) return ELEM_EDIT;
	if(strcmp(typeStr, "static") == 0) return ELEM_STATIC;
	if(strcmp(typeStr, "scrollbar") == 0) return ELEM_SCROLLBAR;
	if(strcmp(typeStr, "checkbox") == 0) return ELEM_CHECKBOX;
	if(strcmp(typeStr, "combobox") == 0) return ELEM_COMBOBOX;
	if(strcmp(typeStr, "listbox") == 0) return ELEM_LISTBOX;
	if(strcmp(typeStr, "slider") == 0) return ELEM_SLIDER;
	return ELEM_UNKNOWN;
}

/*
 */
const char *NUIParser::elementTypeToString(ElementType type) {

	switch(type) {
		case ELEM_BUTTON:
			return "Button";
		case ELEM_TEXTBOX:
			return "Textbox";
		case ELEM_EDIT:
			return "Edit";
		case ELEM_STATIC:
			return "Static";
		case ELEM_SCROLLBAR:
			return "Scrollbar";
		case ELEM_CHECKBOX:
			return "Checkbox";
		case ELEM_COMBOBOX:
			return "ComboBox";
		case ELEM_LISTBOX:
			return "ListBox";
		case ELEM_SLIDER:
			return "Slider";
		default:
			return "Unknown";
	}
}

/*
 */
int NUIParser::parse_quad_line(const char *line, UIQuadData &quad) {

	if(!line) return 0;
	memset(&quad, 0, sizeof(quad));

	const char *p = line;

	while(*p && !isspace((unsigned char)*p)) ++p;
	while(*p && isspace((unsigned char)*p)) ++p;

	while(*p) {
		if(strncmp(p, "mat=", 4) == 0) {
			p += 4;
			char *dst = quad.material;
			while(*p && !isspace((unsigned char)*p) && dst - quad.material < (int)sizeof(quad.material) - 1)
				*dst++ = *p++;
			*dst = '\0';
		} else if(strncmp(p, "pos=", 4) == 0) {
			p += 4;
			sscanf(p, "%d,%d", &quad.pos_x, &quad.pos_y);
			while(*p && !isspace((unsigned char)*p)) ++p;
		} else if(strncmp(p, "size=", 5) == 0) {
			p += 5;
			sscanf(p, "%d,%d", &quad.width, &quad.height);
			while(*p && !isspace((unsigned char)*p)) ++p;
		} else {
			++p;
		}
		while(*p && isspace((unsigned char)*p)) ++p;
	}

	return (quad.material[0] != '\0');
}

/*
 */
int NUIParser::parse_element_line(const char *line, UIElementData &elem) {

	if(!line) return 0;
	memset(&elem, 0, sizeof(elem));
	elem.color_a = 255;

	const char *p = line;

	while(*p && !isspace((unsigned char)*p)) ++p;
	while(*p && isspace((unsigned char)*p)) ++p;

	// get element type string
	char typeStr[32] = {};
	char *dst		 = typeStr;
	while(*p && !isspace((unsigned char)*p) && dst - typeStr < (int)sizeof(typeStr) - 1) *dst++ = *p++;
	*dst	  = '\0';
	elem.type = parse_element_type(typeStr);

	while(*p && isspace((unsigned char)*p)) ++p;

	while(*p) {
		if(strncmp(p, "name=", 5) == 0) {
			p += 5;
			dst = elem.name;
			while(*p && !isspace((unsigned char)*p) && dst - elem.name < (int)sizeof(elem.name) - 1) *dst++ = *p++;
			*dst = '\0';
		} else if(strncmp(p, "pos=", 4) == 0) {
			p += 4;
			sscanf(p, "%d,%d", &elem.pos_x, &elem.pos_y);
			while(*p && !isspace((unsigned char)*p)) ++p;
		} else if(strncmp(p, "size=", 5) == 0) {
			p += 5;
			sscanf(p, "%d,%d", &elem.width, &elem.height);
			while(*p && !isspace((unsigned char)*p)) ++p;
		} else if(strncmp(p, "color=", 6) == 0) {
			p += 6;
			int ir = 255, ig = 255, ib = 255, ia = 255;
			sscanf(p, "%d,%d,%d,%d", &ir, &ig, &ib, &ia);
			elem.color_r = (uint8_t)(ir & 0xFF);
			elem.color_g = (uint8_t)(ig & 0xFF);
			elem.color_b = (uint8_t)(ib & 0xFF);
			elem.color_a = (uint8_t)(ia & 0xFF);
			while(*p && !isspace((unsigned char)*p)) ++p;
		} else if(strncmp(p, "font=", 5) == 0) {
			p += 5;
			dst = elem.font;
			while(*p && !isspace((unsigned char)*p) && dst - elem.font < (int)sizeof(elem.font) - 1) *dst++ = *p++;
			*dst = '\0';
		} else if(strncmp(p, "flags=", 6) == 0) {
			p += 6;
			sscanf(p, "%x", &elem.flags);
			while(*p && !isspace((unsigned char)*p)) ++p;
		} else {
			++p;
		}
		while(*p && isspace((unsigned char)*p)) ++p;
	}

	return (elem.name[0] != '\0');
}

/*
 */
int NUIParser::parse(const char *filePath) {

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

	char *line		= m.raw_text;
	m.entry_count	= 0;
	m.quad_count	= 0;
	m.element_count = 0;
	m.is_dialog		= 0;
	m.name[0]		= '\0';
	m.music[0]		= '\0';
	m.pos_x			= 0;
	m.pos_y			= 0;

	while(line && *line) {
		char *eol = line;
		while(*eol && *eol != '\n' && *eol != '\r') ++eol;
		char saved = *eol;
		*eol	   = '\0';

		char *p = line;
		while(*p == ' ' || *p == '\t') ++p;

		// skip blanks and comments
		if(*p == '\0' || *p == ';') {
			*eol = saved;
			line = eol;
			if(*line == '\r') ++line;
			if(*line == '\n') ++line;
			continue;
		}

		if(m.entry_count < MAX_ENTRIES) {
			UIEntry &e = m.entries[m.entry_count];
			strncpy(e.line, p, sizeof(e.line) - 1);
			e.line[sizeof(e.line) - 1] = '\0';

			if(strncmp(p, "name=", 5) == 0) {
				strncpy(m.name, p + 5, sizeof(m.name) - 1);
				strncpy(e.type, "header", sizeof(e.type) - 1);
			} else if(strncmp(p, "pos=", 4) == 0) {
				sscanf(p + 4, "%d,%d", &m.pos_x, &m.pos_y);
				strncpy(e.type, "property", sizeof(e.type) - 1);
			} else if(strncmp(p, "dialog=", 7) == 0) {
				m.is_dialog = atoi(p + 7);
				strncpy(e.type, "property", sizeof(e.type) - 1);
			} else if(strncmp(p, "music=", 6) == 0) {
				strncpy(m.music, p + 6, sizeof(m.music) - 1);
				strncpy(e.type, "property", sizeof(e.type) - 1);
			} else if(strncmp(p, "quad ", 5) == 0 || strncmp(p, "quad\t", 5) == 0) {
				strncpy(e.type, "quad", sizeof(e.type) - 1);
				if(m.quad_count < MAX_QUADS) {
					if(parse_quad_line(p, m.quads[m.quad_count])) m.quad_count++;
				}
			} else if(strncmp(p, "element ", 8) == 0 || strncmp(p, "element\t", 8) == 0) {
				strncpy(e.type, "element", sizeof(e.type) - 1);
				if(m.element_count < MAX_ELEMENTS) {
					if(parse_element_line(p, m.elements[m.element_count])) m.element_count++;
				}
			} else {
				strncpy(e.type, "property", sizeof(e.type) - 1);
			}
			m.entry_count++;
		}

		*eol = saved;
		line = eol;
		if(*line == '\r') ++line;
		if(*line == '\n') ++line;
	}

	printf("[NUI] %s: name='%s' pos=(%d,%d) %u quads, %u elements\n", filePath, m.name, m.pos_x, m.pos_y, m.quad_count,
		   m.element_count);

	return m.entry_count > 0;
}

/*
 */
int NUIParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}
