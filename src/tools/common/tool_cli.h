/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: tool_cli.h
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

#ifndef __TOOL_CLI_H__
#define __TOOL_CLI_H__

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

/*
 */
namespace NodTools {

/*
 */
class CommandLine {
	public:
		CommandLine()								= default;
		CommandLine(const CommandLine &)			= delete;
		CommandLine &operator=(const CommandLine &) = delete;
		bool initialize(int &argument_count, char **&arguments, std::string &error);

	private:
		std::vector<std::string> strings;
		std::vector<char *> pointers;
};

/*
 */
std::string lower_ascii(std::string text);
bool is_directory(const std::filesystem::path &path);
bool validate_output(const std::filesystem::path &workspace, const std::filesystem::path &assets,
					 const std::filesystem::path &output, std::string &error);
bool distinct_input_output(const std::filesystem::path &input, const std::filesystem::path &output, std::string &error);
bool read_file(const std::filesystem::path &path, std::vector<uint8_t> &bytes, std::string &error,
			   size_t maximum_bytes = (std::numeric_limits<size_t>::max)());

} // namespace NodTools

#endif /* __TOOL_CLI_H__ */
