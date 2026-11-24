/*
===========================================================================
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

#ifndef __NODEXPORT_EXPORT_H__
#define __NODEXPORT_EXPORT_H__

#include "nodsurvey/report.h"

#include <string>

namespace nodexport {

	namespace fs = std::filesystem;

	// Each mode fills `json`. On a refusal it returns false and `json` holds
	// {"format": ..., "error": ...}; no other output is written.
	bool exportNod(const char *in, const char *outBin, std::string &json);
	bool exportNad(const char *in, std::string &json);
	bool exportMaterials(const char *build, const char *names, std::string &json);
	bool exportCatalog(const char *build, std::string &json);

	inline std::string refusal(const char *format, const std::string &error) {
		return std::string("{\"format\":\"") + format + "\",\"error\":" + survey::quote(error) + "}\n";
	}

	// Floats in full precision; not-a-number and infinities as null.
	inline std::string exact(double v) {
		if(!std::isfinite(v)) return "null";
		char buf[32];
		std::snprintf(buf, sizeof buf, "%.9g", v);
		return buf;
	}

	inline std::string floats(const float *v, size_t n) {
		std::string out = "[";
		for(size_t i = 0; i < n; ++i) out += (i ? "," : "") + exact(v[i]);
		return out + "]";
	}

} // namespace nodexport

#endif // __NODEXPORT_EXPORT_H__
