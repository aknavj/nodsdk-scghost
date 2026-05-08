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

#include "core/Strings.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace viewer::core {

/*
 */
std::string toLower(std::string_view s) {
	std::string r(s);
	for(auto &c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

/*
 */
int striCmp(const char *a, const char *b) noexcept {
	while(*a && *b) {
		const int d = std::tolower(static_cast<unsigned char>(*a))
		            - std::tolower(static_cast<unsigned char>(*b));
		if(d != 0) return d;
		++a; ++b;
	}
	return std::tolower(static_cast<unsigned char>(*a))
	     - std::tolower(static_cast<unsigned char>(*b));
}

/*
 */
std::string cleanTexName(const char *raw) {
	if(!raw) return {};
	char clean[128];
	std::strncpy(clean, raw, 127);
	clean[127] = '\0';
	if(char *sp = std::strchr(clean, ' ')) *sp = '\0';
	const std::size_t len = std::strlen(clean);
	if(len > 4 && striCmp(clean + len - 4, ".dds") == 0) clean[len - 4] = '\0';
	return std::string(clean);
}

/*
 */
std::string getExtension(const std::string &path) {
	const std::size_t dot = path.rfind('.');
	if(dot == std::string::npos) return {};
	std::string ext = path.substr(dot + 1);
	for(auto &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return ext;
}

/*
 */
std::string getFilename(const std::string &path) {
	const std::size_t slash = path.rfind('/');
	if(slash != std::string::npos) return path.substr(slash + 1);
	return path;
}

/*
 */
std::string formatSize(std::size_t bytes) {
	char buf[64];
	if(bytes >= 1024 * 1024) {
		std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	}
	else if(bytes >= 1024) {
		std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
	}
	else {
		std::snprintf(buf, sizeof(buf), "%zu B", bytes);
	}
	return buf;
}

} // namespace viewer::core
