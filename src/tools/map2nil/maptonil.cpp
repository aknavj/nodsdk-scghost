/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: maptonil.cpp
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

#include "world_image.h"
#include "world_lighting.h"
#include "scene_bridge.h"
#include "tool_cli.h"
#include "tool_help.h"
#include "../editors/workspace.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>

/*
 */
namespace {
namespace fs = std::filesystem;

/*
 */
int failure(const std::string &message) {
	std::cerr << "MAPtoNIL: " << message << '\n';
	return 1;
}

/*
 */
void usage(FILE *output) {
	nod_tools_print_help(output, "MAPtoNIL",
		"--world-only : Required; compile world geometry without point-entity placements.\n"
		"--workspace DIR : Existing authoring workspace containing the output.\n"
		"--asset-root DIR : Read-only original asset directory; output must be outside it.\n"
		"--physics-bounds X0 Y0 Z0 X1 Y1 Z1 : Override generated physics bounds.\n"
		"--lighting auto|on|off : Select the light-baking mode.\n"
		"--light-spacing N : Set light sample spacing in world units.\n"
		"--light-samples 1|4 : Set the number of samples per lightmap texel.\n"
		"--light-shadows on|off : Enable or disable baked shadows.\n"
		"--light-phong DEG : Set the smoothing angle in degrees.\n"
		"--sun-light R G B I DX DY DZ : Set sunlight color, intensity, and direction.\n"
		"--ambient-light R G B I : Set ambient light color and intensity.\n"
		"--help : Show help.\n"
		"-- : End options.\n"
		"input.map : Source MAP world brushes.\n"
		"output.nil : Destination NIL v35 level inside the workspace.\n",
		"MAPtoNIL --world-only --workspace workspace --asset-root assets input.map workspace/output.nil\n"
		"MAPtoNIL --world-only --workspace workspace --asset-root assets --lighting on --light-samples 4 input.map workspace/output.nil\n");
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
	fs::path input;
	fs::path output;
	std::array<float, 6> bounds{};
	bool world_only	 = false;
	bool have_bounds = false;
	bool options	 = true;
	std::vector<std::string> lighting_arguments;
	for(int i = 1; i < argc; ++i) {
		const std::string arg	  = argv[i];
		const int lighting_values = nod::lighting::option_arity(arg);
		if(arg.empty()) {
			return failure("Empty argument");
		}

		if(options && arg == "--") {
			options = false;
			continue;
		}

		if(options && arg == "--world-only") {
			if(world_only) {
				return failure("Duplicate --world-only");
			}

			world_only = true;
		} else if(options && (arg == "--workspace" || arg == "--asset-root")) {
			if(i + 1 == argc || !*argv[i + 1]) {
				return failure("Missing value for " + arg);
			}

			auto &path = arg == "--workspace" ? workspace : assets;
			if(!path.empty()) {
				return failure("Duplicate " + arg);
			}

			path = fs::u8path(argv[++i]);
		} else if(options && arg == "--physics-bounds") {
			if(have_bounds) {
				return failure("Duplicate --physics-bounds");
			}

			if(i + 6 >= argc) {
				return failure("--physics-bounds requires six numbers");
			}

			for(float &value : bounds) {
				const char *text = argv[++i];
				char *end		 = nullptr;
				errno			 = 0;
				value			 = std::strtof(text, &end);
				if(end == text || *end || errno == ERANGE || !std::isfinite(value)) {
					return failure("Invalid physics bound: " + std::string(text));
				}
			}

			have_bounds = true;
		} else if(options && lighting_values >= 0) {
			if(i + lighting_values >= argc) {
				return failure("Missing values for " + arg);
			}

			lighting_arguments.push_back(arg);
			for(int k = 0; k < lighting_values; ++k) {
				lighting_arguments.push_back(argv[++i]);
			}
		} else if(options && !arg.empty() && arg[0] == '-') {
			return failure("Unknown option: " + arg);
		} else if(input.empty()) {
			input = fs::u8path(arg);
		} else if(output.empty()) {
			output = fs::u8path(arg);
		} else {
			return failure("Expected one MAP and one NIL path");
		}
	}

	if(!world_only || !NodTools::is_directory(workspace) || !NodTools::is_directory(assets) || input.empty() ||
	   output.empty()) {
		return failure("Specify --world-only, workspace, assets and paths; use --help");
	}

	if(NodTools::lower_ascii(input.extension().u8string()) != ".map" ||
	   NodTools::lower_ascii(output.extension().u8string()) != ".nil") {
		return failure("Expected input.map and output.nil");
	}

	if(!NodTools::validate_output(workspace, assets, output, error) ||
	   !NodTools::distinct_input_output(input, output, error)) {
		return failure(error);
	}

	nod_scene::AuthoringMap source;
	if(!source.loadFile(input, error)) {
		return failure(error);
	}

	nod::lighting::Settings lighting;
	if(!nod::lighting::parse(source.getMap(), lighting, error)) {
		return failure(error);
	}

	if(!nod::lighting::apply_options(lighting_arguments, lighting, error)) {
		return failure(error);
	}

	nod::world::Geometry geometry;
	if(!nod::world::build_geometry(source.getMap(), geometry, error)) {
		return failure(error);
	}

	const nod::world::Geometry occluders = geometry;
	if(lighting.active() && !nod::world::refine_lighting_geometry(occluders, lighting.spacing, geometry, error)) {
		return failure(error);
	}

	if(!have_bounds) {
		for(size_t axis = 0; axis < 3; ++axis) {
			bounds[axis]	 = std::min(geometry.low[axis] - 1.0f,
										std::nextafter(geometry.low[axis], -std::numeric_limits<float>::infinity()));
			bounds[axis + 3] = std::max(geometry.high[axis] + 1.0f,
										std::nextafter(geometry.high[axis], std::numeric_limits<float>::infinity()));
		}
	}

	auto bundle = output.filename();
	bundle.replace_extension(".nmb");
	const auto root	 = assets.u8string();
	const auto level = bundle.u8string();
	std::unique_ptr<NodMaterials, decltype(&nod_materials_close)> materials(
		nod_materials_open(root.c_str(), level.c_str(), ""), nod_materials_close);
	if(!materials) {
		return failure("Cannot open material catalog");
	}

	nod::world::RenderGeometry render;
	if(!nod::world::build_render_geometry(geometry, materials.get(), render, error)) {
		return failure(error);
	}

	if(!nod::world::bake_lighting(occluders, lighting, render, error)) {
		return failure(error);
	}

	nod::world::WorldImage image;
	if(!nod::world::build_world_image(geometry, render, bounds, image, error)) {
		return failure(error);
	}

	nod::nil::CollisionLayout collision;
	if(!nod::nil::readCollisionLayout(image.bytes.data(), image.bytes.size(), collision, error)) {
		return failure("Invalid compiled NIL: " + error);
	}

	if(!nod::edit::atomic_write(output, image.bytes, true, error)) {
		return failure(error);
	}

	std::cout << "MAPtoNIL: wrote " << image.bytes.size() << " bytes, " << geometry.brush_count << " brushes, "
			  << geometry.mesh.indices.size() / 3 << " triangles, " << image.excluded_entities.size()
			  << " excluded entities";
	if(!lighting.points.empty()) {
		std::cout << ", " << lighting.points.size() << " baked lights";
	}

	std::cout << '\n';
	return 0;
}
