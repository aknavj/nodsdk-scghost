/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: map_lighting.cpp
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

#include "map_lighting.h"
#include "scene_bridge.h"
#include "world_binary.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <locale>
#include <set>

/*
 */
namespace nod::lighting {
namespace {
	/*
	 */
	bool numbers(const char *text, float *values, size_t count) {
		if(!text || !*text) {
			return false;
		}

		std::istringstream stream(text);
		stream.imbue(std::locale::classic());
		for(size_t i = 0; i < count; ++i) {
			if(!(stream >> values[i]) || !std::isfinite(values[i])) {
				return false;
			}
		}

		stream >> std::ws;
		return stream.eof();
	}
} // namespace

/*
 */
bool authoring_class(const nod_scene::Entity &entity) {
	const char *name = nod_scene::get_property(&entity, "classname");
	return name && !std::strcmp(name, "nod_static_light");
}

/*
 */
int option_arity(const std::string &name) {
	if(name == "--sun-light") {
		return 7;
	}

	if(name == "--ambient-light") {
		return 4;
	}

	if(name == "--lighting" || name == "--light-spacing" || name == "--light-samples" || name == "--light-shadows" ||
	   name == "--light-phong") {
		return 1;
	}

	return -1;
}

/*
 */
bool apply_options(const std::vector<std::string> &arguments, Settings &settings, std::string &error) {
	error.clear();
	Settings result = settings;
	std::set<std::string> seen;
	for(size_t i = 0; i < arguments.size();) {
		const std::string name = arguments[i++];
		const int count		   = option_arity(name);
		if(count < 0 || arguments.size() - i < static_cast<size_t>(count) || !seen.insert(name).second) {
			error = "Unknown, incomplete or duplicate lighting option: " + name;
			return false;
		}

		const auto &value = arguments[i];
		if(name == "--lighting") {
			if(value != "auto" && value != "on" && value != "off") {
				error = "--lighting requires auto, on or off";
				return false;
			}

			result.enabled = value != "off";
			result.force   = value == "on";
		} else if(name == "--light-shadows") {
			if(value != "on" && value != "off") {
				error = "--light-shadows requires on or off";
				return false;
			}

			result.shadows = value == "on";
		} else {
			float values[7]{};
			for(int k = 0; k < count; ++k) {
				if(!numbers(arguments[i + k].c_str(), &values[k], 1)) {
					error = "Invalid finite number for " + name;
					return false;
				}
			}

			bool valid = true;
			if(name == "--light-spacing") {
				valid		   = values[0] >= 0.01f && values[0] <= 1024;
				result.spacing = values[0];
			} else if(name == "--light-samples") {
				valid		   = values[0] == 1 || values[0] == 4;
				result.samples = valid ? static_cast<unsigned>(values[0]) : 1;
			} else if(name == "--light-phong") {
				valid		 = values[0] >= 0 && values[0] <= 180;
				result.phong = values[0];
			} else {
				for(int k = 0; k < 3; ++k) {
					valid = valid && values[k] >= 0 && values[k] <= 1;
				}

				valid =
					valid && (values[0] > 0 || values[1] > 0 || values[2] > 0) && values[3] > 0 && values[3] <= 1024;
				if(name == "--sun-light") {
					double length = 0;
					for(int k = 4; k < 7; ++k) {
						length += double(values[k]) * values[k];
					}

					valid = valid && length > 1e-16;
					if(valid) {
						for(int k = 0; k < 3; ++k) {
							result.sun_direction[k] = static_cast<float>(values[k + 4] / std::sqrt(length));
						}
					}

					result.sun			 = true;
					result.sun_intensity = values[3];
					result.sun_color	 = {values[0], values[1], values[2]};
				} else {
					result.ambient			 = true;
					result.ambient_intensity = values[3];
					result.ambient_color	 = {values[0], values[1], values[2]};
				}
			}

			if(!valid) {
				error = "Lighting value out of range for " + name;
				return false;
			}
		}

		i += count;
	}

	if(!result.enabled && (result.sun || result.ambient)) {
		error = "Sun/ambient options require lighting to be enabled";
		return false;
	}

	settings = std::move(result);
	return true;
}

/*
 */
bool parse(const nod_scene::Map &map, Settings &settings, std::string &error) {
	error.clear();
	Settings result;
	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		const char *name   = nod_scene::get_property(&entity, "classname");
		if(name && !std::strcmp(name, "worldspawn")) {
			const char *value = nod_scene::get_property(&entity, "nod_light_spacing");
			if(value) {
				if(!numbers(value, &result.spacing, 1) || result.spacing < 0.01f || result.spacing > 1024) {
					error = "nod_light_spacing must be 0.01..1024 world units";
					return false;
				}
			}
		}

		if(!authoring_class(entity)) {
			continue;
		}

		const std::string context = "Light entity " + std::to_string(i) + ": ";
		if(entity.brush_count || result.points.size() == 64) {
			error = context + "Expected a point entity; at most 64 lights";
			return false;
		}

		const std::set<std::string> allowed{"classname", "origin", "_color", "light", "radius"};
		std::set<std::string> seen;
		for(size_t k = 0; k < entity.pair_count; ++k) {
			if(!allowed.count(entity.pairs[k].key) || !seen.insert(entity.pairs[k].key).second) {
				error = context + "Unsupported or duplicate property " + entity.pairs[k].key;
				return false;
			}
		}

		Point point;
		bool valid		  = numbers(nod_scene::get_property(&entity, "origin"), point.position.data(), 3);
		const char *color = nod_scene::get_property(&entity, "_color");
		if(color) {
			valid = numbers(color, point.color.data(), 3) && valid;
		}

		const char *intensity = nod_scene::get_property(&entity, "light");
		if(intensity) {
			valid = numbers(intensity, &point.intensity, 1) && valid;
		}

		const char *radius = nod_scene::get_property(&entity, "radius");
		if(radius) {
			valid = numbers(radius, &point.radius, 1) && valid;
		}

		for(float component : point.color) {
			valid = valid && component >= 0 && component <= 1;
		}

		valid = valid && (point.color[0] > 0 || point.color[1] > 0 || point.color[2] > 0);
		valid = valid && point.intensity > 0.2f && point.intensity <= 1024 && point.radius >= 0.01f &&
				point.radius <= 65536;
		if(!valid) {
			error = context + "Require finite origin, _color 0..1 (nonblack), light >0.2..1024, radius 0.01..65536";
			return false;
		}

		// Original worldLighting_CalcLightFalloff(intensity, radius, cutoff).
		point.falloff = static_cast<float>((double(point.intensity) - 0.2) /
										   (double(point.radius) * (1 + 1.2 * point.radius) * 0.2));
		result.points.push_back(point);
	}

	settings = std::move(result);
	return true;
}

/*
 */
std::vector<uint8_t> records(const Settings &settings) {
	world::detail::Writer writer;
	for(const auto &light : settings.points) {
		writer.word(0);
		writer.word(0);
		writer.word(0);
		for(float component : light.color) {
			writer.real(component * 255);
		}

		writer.real(light.intensity);
		writer.real(light.intensity);
		writer.vector({0, 0, -1});
		writer.vector(light.position);
		writer.real(0);
		writer.real(0);
		writer.real(light.radius);
		writer.real(light.falloff);
	}

	return std::move(writer.bytes);
}

} // namespace nod::lighting
