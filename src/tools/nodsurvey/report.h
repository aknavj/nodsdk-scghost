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

// The JSON pieces and file listing the NODSurvey modes share.

#ifndef __NODSURVEY_REPORT_H__
#define __NODSURVEY_REPORT_H__

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace survey {

	namespace fs = std::filesystem;

	inline std::string lower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		return s;
	}

	// Every regular file of `dir` with extension `ext` (lower case, with the dot), sorted.
	inline std::vector<fs::path> filesOf(const fs::path &dir, const std::string &ext) {
		std::vector<fs::path> out;
		for(const auto &entry : fs::directory_iterator(dir))
			if(entry.is_regular_file() && lower(entry.path().extension().string()) == ext) out.push_back(entry.path());
		std::sort(out.begin(), out.end());
		return out;
	}

	inline std::string quote(const std::string &s) {
		std::string out = "\"";
		for(char c : s) {
			if(c == '"' || c == '\\') out += '\\';
			if(static_cast<unsigned char>(c) < 0x20) continue;
			out += c;
		}
		return out + "\"";
	}

	template <typename Key> std::string keyText(const Key &key) { return std::to_string(key); }
	inline std::string keyText(const std::string &key) { return key; }

	template <typename Key> std::string histogram(const std::map<Key, uint32_t> &h) {
		std::string out = "{";
		for(const auto &[key, count] : h) {
			if(out.size() > 1) out += ",";
			out += quote(keyText(key)) + ":" + std::to_string(count);
		}
		return out + "}";
	}

	inline std::string list(const std::vector<std::string> &items) {
		std::string out = "[";
		for(size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
		return out + "]";
	}

	inline std::string quotedList(const std::vector<std::string> &items) {
		std::vector<std::string> quoted;
		for(const auto &item : items) quoted.push_back(quote(item));
		return list(quoted);
	}

	inline std::string num(double v) {
		if(!std::isfinite(v)) return "null";
		char buf[32];
		std::snprintf(buf, sizeof buf, "%.6g", v);
		return buf;
	}

	inline bool writeFile(const char *path, const std::string &text) {
		FILE *out = std::fopen(path, "wb");
		if(!out) return false;
		const bool written = std::fwrite(text.data(), 1, text.size(), out) == text.size();
		return std::fclose(out) == 0 && written;
	}

	// Mode entry points (nodsurvey_nsa.cpp, ...): fill `json`, return true when clean.
	bool surveyNsa(const fs::path &buildRoot, std::string &json);
	bool surveyNms(const fs::path &buildRoot, std::string &json);

} // namespace survey

#endif // __NODSURVEY_REPORT_H__
