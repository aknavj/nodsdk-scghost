/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: maptonpd.cpp
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

#include "path_graph.h"
#include "tool_cli.h"
#include "tool_help.h"
#include "../editors/workspace.h"
#include <iostream>

/*
 */
namespace {
namespace fs = std::filesystem;

/*
 */
int failure(const std::string &message) {
	std::cerr << "MAPtoNPD: " << message << '\n';
	return 1;
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
		nod_tools_print_help(argc < 2 ? stderr : stdout, "MAPtoNPD",
			"--workspace DIR : Existing authoring workspace containing the output.\n"
			"--asset-root DIR : Read-only original asset directory; output must be outside it.\n"
			"--help : Show help.\n"
			"-- : End options.\n"
			"input.map : Manual world-space waypoints with directed normal links.\n"
			"output.npd : Destination NPD path graph in sector 0, inside the workspace.\n",
			"MAPtoNPD --workspace workspace --asset-root assets input.map workspace/output.npd\n");
		return argc < 2 ? 1 : 0;
	}

	fs::path workspace;
	fs::path assets;
	fs::path input;
	fs::path output;
	bool options = true;
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg.empty()) {
			return failure("Empty argument");
		}

		if(options && arg == "--") {
			options = false;
			continue;
		}

		if(options && (arg == "--workspace" || arg == "--asset-root")) {
			if(i + 1 == argc || !*argv[i + 1]) {
				return failure("Missing value for " + arg);
			}

			auto &path = arg == "--workspace" ? workspace : assets;
			if(!path.empty()) {
				return failure("Duplicate " + arg);
			}

			path = fs::u8path(argv[++i]);
		} else if(options && arg[0] == '-') {
			return failure("Unknown option: " + arg);
		} else if(input.empty()) {
			input = fs::u8path(arg);
		} else if(output.empty()) {
			output = fs::u8path(arg);
		} else {
			return failure("Expected one MAP and one NPD path");
		}
	}

	if(!NodTools::is_directory(workspace) || !NodTools::is_directory(assets) || input.empty() || output.empty()) {
		return failure("Specify workspace, assets and paths; use --help");
	}

	if(NodTools::lower_ascii(input.extension().u8string()) != ".map" ||
	   NodTools::lower_ascii(output.extension().u8string()) != ".npd") {
		return failure("Expected input.map and output.npd");
	}

	if(!NodTools::validate_output(workspace, assets, output, error) ||
	   !NodTools::distinct_input_output(input, output, error)) {
		return failure(error);
	}

	nod_scene::AuthoringMap source;
	if(!source.loadFile(input, error)) {
		return failure(error);
	}

	nod::path::Graph graph;
	if(!nod::path::compile(source.getMap(), graph, error)) {
		return failure(error);
	}

	if(!nod::edit::atomic_write(output, graph.bytes, true, error)) {
		return failure(error);
	}

	std::cout << "MAPtoNPD: wrote " << graph.bytes.size() << " bytes, " << graph.nodes << " waypoints, " << graph.links
			  << " directed links, " << graph.components << " components\n";
	return 0;
}
