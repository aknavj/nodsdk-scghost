/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodtonmb.cpp
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

#include "nmb_compiler.h"
#include "tool_cli.h"
#include "tool_help.h"
#include "../editors/workspace.h"

#include <algorithm>
#include <iostream>
#include <utility>

/*
 */
namespace {
namespace fs = std::filesystem;

/*
 */
int failure(const std::string &message) {
	std::cerr << "NODtoNMB: " << message << '\n';
	return 1;
}

/*
 */
void usage(FILE *output) {
	nod_tools_print_help(output, "NODtoNMB",
		"--workspace DIR : Existing authoring workspace containing the output.\n"
		"--asset-root DIR : Read-only original asset directory; output must be outside it.\n"
		"--model NAME FILE : Add a NOD/NOC member with an explicit bundle name; repeatable.\n"
		"--help : Show help.\n"
		"-- : End options.\n"
		"output.nmb : Destination bundle inside the workspace.\n"
		"model.nod|model.noc : Optional input models; up to 256 members in total.\n",
		"NODtoNMB --workspace workspace --asset-root assets workspace/output.nmb model.nod\n"
		"NODtoNMB --workspace workspace --asset-root assets --model Crate.nod model.nod workspace/output.nmb\n");
}

} // namespace

/*
 */
int main(int argc, char **argv) {
	std::string error;
	NodTools::CommandLine command_line;
	if(!command_line.initialize(argc, argv, error)) {
		return failure(error);
	}

	if(argc < 2 || (argc == 2 && std::string(argv[1]) == "--help")) {
		usage(argc < 2 ? stderr : stdout);
		return argc < 2 ? 1 : 0;
	}

	fs::path workspace;
	fs::path assets;
	fs::path output;
	std::vector<std::pair<std::string, fs::path>> sources;
	bool options = true;
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(options && arg == "--") {
			options = false;
			continue;
		}

		if(options && (arg == "--workspace" || arg == "--asset-root")) {
			if(i + 1 == argc) {
				return failure("Missing value for " + arg);
			}

			auto &path = arg == "--workspace" ? workspace : assets;
			if(!path.empty()) {
				return failure("Duplicate " + arg);
			}

			path = fs::u8path(argv[++i]);
		} else if(options && arg == "--model") {
			if(i + 2 >= argc) {
				return failure("--model requires a member name and a NOD path");
			}

			const std::string name = argv[++i];
			sources.emplace_back(name, fs::u8path(argv[++i]));
		} else if(options && !arg.empty() && arg[0] == '-') {
			return failure("Unknown option: " + arg);
		} else if(output.empty()) {
			output = fs::u8path(arg);
		} else {
			const auto path = fs::u8path(arg);
			sources.emplace_back(path.filename().u8string(), path);
		}
	}

	if(!NodTools::is_directory(workspace) || !NodTools::is_directory(assets) || output.empty()) {
		return failure("Specify existing --workspace and --asset-root directories and an output NMB; use --help");
	}

	if(NodTools::lower_ascii(output.extension().u8string()) != ".nmb") {
		return failure("Output must have the .nmb extension");
	}

	if(sources.size() > 256) {
		return failure("NMB supports at most 256 models");
	}

	if(!NodTools::validate_output(workspace, assets, output, error)) {
		return failure(error);
	}

	std::vector<nod::bundle::Input> inputs;
	size_t total_bytes = 0;
	for(const auto &source : sources) {
		const auto extension = NodTools::lower_ascii(source.second.extension().u8string());
		if(extension != ".nod" && extension != ".noc") {
			return failure("Input must have the .nod or .noc extension: " + source.second.u8string());
		}

		if(!NodTools::distinct_input_output(source.second, output, error)) {
			return failure(error);
		}

		constexpr size_t maximum_bytes = 256u * 1024u * 1024u;
		nod::bundle::Input input{source.first, {}};
		if(!NodTools::read_file(source.second, input.bytes, error, maximum_bytes - total_bytes)) {
			return failure(error);
		}

		total_bytes += input.bytes.size();
		inputs.push_back(std::move(input));
	}

	std::vector<uint8_t> bytes;
	if(!nod::bundle::compile(inputs, bytes, error)) {
		return failure(error);
	}

	if(!nod::edit::atomic_write(output, bytes, true, error)) {
		return failure(error);
	}

	std::cout << "NODtoNMB: wrote " << inputs.size() << " models, " << bytes.size() << " bytes to " << output.u8string()
			  << '\n';
	return 0;
}
