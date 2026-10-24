/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodscene_cli.cpp
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

#include "scene_bridge.h"
#include "tool_help.h"

#include <stdlib.h>
#include <string.h>

/*
 */
namespace {

/*
 */
void usage(FILE *file) {
	nod_tools_print_help(file, "NODscene",
		"import : Import native scenes for NOT-driven MAP authoring.\n"
		"export : Export legacy MAP geometry and a NODSRC sidecar.\n"
		"export-properties : Export geometry with placement properties and a sidecar.\n"
		"compile : Apply sidecar-backed edits; authoring MAPs and geometry edits are unsupported.\n"
		"preview : Export a model-brush preview using native assets.\n"
		"preview-obj : Export an OBJ-backed preview using native assets.\n"
		"entitydefs : Generate entity definitions from one or more NSD files.\n"
		"--assets-root DIR : Set the asset root for entitydefs.\n"
		"--inventory FILE : Write the entitydefs inventory as JSON.\n"
		"--help : Show help.\n"
		"input.nodsrc : Required source sidecar for compile and preview commands.\n"
		"level.nmb|- : Level model bundle, or - when no bundle is available.\n",
		"NODscene import input.nil input.nsd input.npd workspace assets-root output.map output.nodsrc\n"
		"NODscene export input.nil input.nsd input.npd output.map output.nodsrc\n"
		"NODscene export-properties input.nil input.nsd input.npd output.map output.nodsrc\n"
		"NODscene compile input.map input.nodsrc output.nil output.nsd output.npd\n"
		"NODscene preview input.map input.nodsrc assets-root level.nmb preview-root output.map output.nodsrc\n"
		"NODscene preview-obj input.map input.nodsrc assets-root level.nmb preview-root output.map output.nodsrc\n"
		"NODscene entitydefs output.def --assets-root assets-root --inventory output.json input.nsd\n");
}

/*
 */
int distinct_paths(int argc, char **argv) {
	for(size_t i = 2; i < static_cast<size_t>(argc); ++i) {
		for(size_t j = i + 1; j < static_cast<size_t>(argc); ++j) {
			if(!strcmp(argv[i], argv[j])) {
				return nod_scene::scene_error("all input and output paths must differ");
			}
		}
	}

	return 1;
}

} // namespace

/*
 */
int main(int argc, char **argv) {
	if(argc == 2 && !strcmp(argv[1], "--help")) {
		usage(stdout);
		return EXIT_SUCCESS;
	}

	const bool definitions = argc >= 4 && !strcmp(argv[1], "entitydefs");
	const bool authoring   = argc == 9 && !strcmp(argv[1], "import");
	if(!(definitions || authoring ||
		 (argc == nod_scene::export_argument_count &&
		  (!strcmp(argv[1], "export") || !strcmp(argv[1], "export-properties") || !strcmp(argv[1], "compile"))) ||
		 (argc == nod_scene::preview_argument_count &&
		  (!strcmp(argv[1], "preview") || !strcmp(argv[1], "preview-obj"))))) {
		usage(stderr);
		return EXIT_FAILURE;
	}

	if(!distinct_paths(argc, argv)) {
		return EXIT_FAILURE;
	}

	if(definitions) {
		return nod_scene::entity_definitions(argc, argv);
	}

	if(authoring) {
		return nod_scene::import_authoring(argv);
	}

	return !strcmp(argv[1], "compile") ? nod_scene::compile_scene(argv) : nod_scene::export_scene(argv);
}
