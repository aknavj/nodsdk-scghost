/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: metadata_xml.h
Version: 0.6.7

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

#ifndef NOD_EDITOR_METADATA_XML_H
#define NOD_EDITOR_METADATA_XML_H

#include "document.h"
#include <ostream>

namespace nod::metadata {
inline std::string xml(const std::string &value) {
	std::string result;
	for(unsigned char c : value) {
		switch(c) {
		case '&': result += "&amp;"; break;
		case '<': result += "&lt;"; break;
		case '>': result += "&gt;"; break;
		case '"': result += "&quot;"; break;
		case '\'': result += "&apos;"; break;
		case '\n': result += "&#10;"; break;
		case '\r': result += "&#13;"; break;
		case '\t': result += "&#9;"; break;
		default:
			if(c < 32 || c > 126) {
				const char *hex = "0123456789ABCDEF";
				result += "\\x";
				result += hex[c >> 4];
				result += hex[c & 15];
			} else result += char(c);
		}
	}
	return result;
}

inline bool map_name(const std::string &name) {
	if(name.empty() || name.size() > 31) return false;
	bool nonblank = false;
	for(unsigned char c : name) {
		if(c < 32 || c > 126 || c == '"') return false;
		if(c != ' ') nonblank = true;
	}
	return nonblank;
}

inline const char *storage(nod::edit::ValueKind kind) {
	using nod::edit::ValueKind;
	return kind == ValueKind::text ? "string32" : kind == ValueKind::float32 ? "float32" :
		kind == ValueKind::uint32 ? "uint32 bits" : kind == ValueKind::vector3 ? "vector3" : "raw";
}

inline bool editable(const nod::edit::Document &document, const nod::edit::Field &field,
					 const std::string &value, std::string &issue) {
	bool enabled = field.editable && field.kind != nod::edit::ValueKind::raw &&
		field.property_id > 3 && field.property_id != 20;
	if(enabled && value.find('"') != std::string::npos) {
		enabled = false; issue = "Quotes cannot be round-tripped by the MAP format";
	}
	if(enabled && !document.validate_value(field, value, issue)) enabled = false;
	if(!enabled && issue.empty()) issue = "Native structure, transform alias or unsupported payload; read-only";
	return enabled;
}

int instance_catalog(int argc, char **argv);
}

#endif
