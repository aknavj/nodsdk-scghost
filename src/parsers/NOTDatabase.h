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

#ifndef NOD_NOT_DATABASE_H
#define NOD_NOT_DATABASE_H

#include "NOTParser.h"
#include <filesystem>
#include <map>
#include <memory>

/*
 */
class NOTDatabase {

	public:

		struct Snapshot {
			std::string name;
			std::vector<uint8_t> bytes;
		};

		struct ModelPreview {
			std::string model;
			const NOTParser::Template *owner = nullptr;
			float scale = 1;
			float offset[3] = {};
		};
		
		static bool modelPreview(const std::vector<NOTParser::ResolvedProperty> &properties,
								ModelPreview &out, std::string &error);
		static bool archiveName(std::string value, std::string &name, std::string &error);
		static bool readArchive(const std::filesystem::path &root, const std::string &name, Snapshot &out,
								std::string &error, const std::filesystem::path &fallback_root = {},
								std::filesystem::path *resolved_path = nullptr);

		bool load(const std::filesystem::path &game_root, const std::vector<std::string> &scene_archives,
				std::string &error, const std::filesystem::path &fallback_root = {});
		bool loadBuffers(const std::vector<Snapshot> &archives, std::string &error,
						const std::vector<std::string> &required = {});
		const NOTParser::Template *findTemplate(const char *name) const;
		const Snapshot *source(const NOTParser::Template *owner) const;
		NOTParser::ResolveStatus resolveModifiers(const char *name, std::vector<NOTParser::ResolvedProperty> &out,
												std::string &error) const;
		NOTParser::ResolveStatus resolveModifiers(const NODModifier *properties, size_t count,
												std::vector<NOTParser::ResolvedProperty> &out, std::string &error) const;
		const auto &snapshots() const { return snapshots_; }
		const auto &templates() const { return templates_; }
		const auto &warnings() const { return warnings_; }

	private:
		std::vector<Snapshot> snapshots_;
		std::vector<std::unique_ptr<NOTParser>> parsers_;
		std::map<std::string, const NOTParser::Template *> templates_;
		std::map<const NOTParser::Template *, size_t> sources_;
		std::vector<std::string> warnings_;
};

#endif
