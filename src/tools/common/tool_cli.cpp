/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: tool_cli.cpp
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

#include "tool_cli.h"

#include "workspace.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

#include <fstream>
#include <memory>
#include <utility>

/*
 */
namespace NodTools {

/*
 */
bool CommandLine::initialize(int &argument_count, char **&arguments, std::string &error) {
	error.clear();
#ifdef _WIN32
	int count = 0;
	std::unique_ptr<wchar_t *, decltype(&LocalFree)> wide_arguments(CommandLineToArgvW(GetCommandLineW(), &count),
																	&LocalFree);
	if(!wide_arguments) {
		error = "Cannot read Unicode arguments";
		return false;
	}

	strings.clear();
	pointers.clear();
	for(int i = 0; i < count; ++i) {
		strings.push_back(std::filesystem::path(wide_arguments.get()[i]).u8string());
	}

	for(auto &value : strings) {
		pointers.push_back(value.data());
	}

	pointers.push_back(nullptr);
	argument_count = count;
	arguments	   = pointers.data();
#else
	(void)argument_count;
	(void)arguments;
#endif
	return true;
}

/*
 */
std::string lower_ascii(std::string text) {
	for(char &character : text) {
		if(character >= 'A' && character <= 'Z') {
			character += 'a' - 'A';
		}
	}

	return text;
}

/*
 */
bool is_directory(const std::filesystem::path &path) {
	std::error_code error;
	return std::filesystem::is_directory(path, error) && !error;
}

/*
 */
bool validate_output(const std::filesystem::path &workspace, const std::filesystem::path &assets,
					 const std::filesystem::path &output, std::string &error) {
	error.clear();
	if(!nod::edit::workspace_contains(workspace, output, error)) {
		if(error.empty()) {
			error = "Output must be inside the workspace";
		}

		return false;
	}

	if(nod::edit::workspace_contains(assets, output, error) || !error.empty()) {
		if(error.empty()) {
			error = "Output must be outside original assets";
		}

		return false;
	}

	return true;
}

/*
 */
bool distinct_input_output(const std::filesystem::path &input, const std::filesystem::path &output,
						   std::string &error) {
	std::error_code comparison_error;
	error.clear();
	if(std::filesystem::equivalent(input, output, comparison_error)) {
		error = "Output aliases an input file";
		return false;
	}

	if(comparison_error && comparison_error != std::errc::no_such_file_or_directory) {
		error = "Cannot compare input/output identities: " + comparison_error.message();
		return false;
	}

	return true;
}

/*
 */
bool read_file(const std::filesystem::path &path, std::vector<uint8_t> &bytes, std::string &error,
			   size_t maximum_bytes) {
	error.clear();
	std::error_code size_error;
	const auto size = std::filesystem::file_size(path, size_error);
	if(size_error) {
		error = "Cannot read " + path.u8string() + ": " + size_error.message();
		return false;
	}

	if(size > maximum_bytes || size > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
		error = "Input exceeds the permitted byte limit: " + path.u8string();
		return false;
	}

	std::ifstream file(path, std::ios::binary);
	if(!file) {
		error = "Cannot open " + path.u8string();
		return false;
	}

	std::vector<uint8_t> contents(static_cast<size_t>(size));
	if((!contents.empty() &&
		!file.read(reinterpret_cast<char *>(contents.data()), static_cast<std::streamsize>(size))) ||
	   file.peek() != std::char_traits<char>::eof() || file.bad()) {
		error = "Cannot completely read " + path.u8string();
		return false;
	}

	bytes = std::move(contents);
	return true;
}

} // namespace NodTools
