/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: workspace.h
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

#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

/*
 */
namespace nod::edit {

/*
 */
bool atomic_write(const std::filesystem::path &path, const std::vector<uint8_t> &bytes, bool replace,
				  std::string &error);

/*
 */
inline bool workspace_contains(const std::filesystem::path &root, const std::filesystem::path &path,
							   std::string &error) {
	std::error_code ec;
	const auto canonical_root = std::filesystem::weakly_canonical(root, ec);
	if(ec) {
		error = "Cannot resolve workspace: " + ec.message();
		return false;
	}

	const auto canonical_path = std::filesystem::weakly_canonical(path, ec);
	if(ec) {
		error = "Cannot resolve destination: " + ec.message();
		return false;
	}

	auto target = canonical_path.begin();
	for(auto component = canonical_root.begin(); component != canonical_root.end(); ++component, ++target) {
		if(target == canonical_path.end()) {
			return false;
		}
#ifdef _WIN32
		if(_wcsicmp(component->c_str(), target->c_str()) != 0) {
			return false;
		}
#else
		if(*component != *target) {
			return false;
		}
#endif
	}

	return true;
}

} // namespace nod::edit

#endif /* __WORKSPACE_H__ */
