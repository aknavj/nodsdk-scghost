/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_render.cpp
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

#include "world_render.h"

#include <cmath>
#include <cstring>
#include <map>

/*
 */
namespace nod::world {
namespace {

/*
 */
bool reject(std::string &error, const std::string &reason) {
	error = reason;
	return false;
}

/*
 */
bool normalized(const std::array<float, 3> &normal) {
	double length = 0;
	for(float value : normal) {
		if(!std::isfinite(value)) {
			return false;
		}

		length += double(value) * value;
	}

	return std::abs(length - 1) <= 0.0001;
}
} // namespace

/*
 */
bool build_texture_projection(const Surface &surface, uint32_t width, uint32_t height, TextureProjection &projection,
							  std::string &error) {
	error.clear();
	if(!width || !height || width > 16384 || height > 16384) {
		return reject(error, "Invalid editor image dimensions");
	}

	if(!normalized(surface.texture_normal)) {
		return reject(error, "Invalid source texture normal");
	}

	for(float value : surface.texture) {
		if(!std::isfinite(value)) {
			return reject(error, "Nonfinite texture field");
		}
	}

	if(!surface.texture[3] || !surface.texture[4]) {
		return reject(error, "Zero texture scale");
	}

	// Strict comparisons preserve Radiant's +Z,-Z,+X,-X,+Y,-Y tie order.
	const auto &normal		  = surface.texture_normal;
	const float candidates[6] = {normal[2], -normal[2], normal[0], -normal[0], normal[1], -normal[1]};
	size_t axis				  = 0;
	float best				  = 0;
	for(size_t i = 0; i < 6; ++i) {
		if(candidates[i] > best) {
			best = candidates[i];
			axis = i;
		}
	}

	const size_t sv		 = axis == 2 || axis == 3 ? 1 : 0;
	const size_t tv		 = axis < 2 ? 1 : 2;
	const float rotation = surface.texture[2];
	float sine			 = 0;
	float cosine		 = 1;
	if(rotation == 90) {
		sine   = 1;
		cosine = 0;
	} else if(rotation == 180) {
		sine   = 0;
		cosine = -1;
	} else if(rotation == 270) {
		sine   = -1;
		cosine = 0;
	} else if(rotation != 0) {
		const float radians = rotation / 180 * 3.14159265358979323846f;
		sine				= std::sin(radians);
		cosine				= std::cos(radians);
	}

	TextureProjection result{};
	result[0][sv]				 = cosine;
	result[0][tv]				 = sine;
	result[1][sv]				 = sine;
	result[1][tv]				 = -cosine;
	const uint32_t dimensions[2] = {width, height};
	for(size_t row = 0; row < 2; ++row) {
		for(size_t component = 0; component < 3; ++component) {
			result[row][component] /= surface.texture[3 + row];
		}

		result[row][3] = surface.texture[row];
		for(float &value : result[row]) {
			value /= static_cast<float>(dimensions[row]);
			if(!std::isfinite(value)) {
				return reject(error, "Texture projection overflow");
			}
		}
	}

	projection = result;
	return true;
}

/*
 */
bool build_render_geometry(const Geometry &geometry, NodMaterials *materials, RenderGeometry &render,
						   std::string &error) {
	error.clear();
	const auto &mesh = geometry.mesh;
	if(!materials) {
		return reject(error, "Missing material catalog");
	}

	if(mesh.vertices.empty() || mesh.vertices.size() > 65535 || mesh.indices.empty() || mesh.indices.size() % 3 ||
	   mesh.indices.size() / 3 > 4096 || geometry.surfaces.empty() || geometry.surfaces.size() > 8192) {
		return reject(error, "Invalid world geometry limits");
	}

	RenderGeometry result;
	std::map<std::string, uint32_t> material_indices;
	for(const auto &surface : geometry.surfaces) {
		const std::string context = "Surface " + std::to_string(result.surfaces.size()) + ": ";
		if(surface.winding.size() < 3 || surface.winding.size() > 256 ||
		   surface.first_triangle != result.indices.size() / 3 || !surface.triangle_count ||
		   surface.first_triangle > mesh.indices.size() / 3 ||
		   surface.triangle_count > mesh.indices.size() / 3 - surface.first_triangle) {
			return reject(error, context + "Invalid triangle span or winding");
		}

		auto material = material_indices.find(surface.material);
		if(material == material_indices.end()) {
			NodMaterialInfo info{};
			char reason[256]{};
			if(!nod_materials_describe(materials, surface.material.c_str(), &info, reason, sizeof(reason))) {
				return reject(error, context + surface.material + ": " + reason);
			}

			uint32_t index = 0;
			while(index < result.materials.size() && std::strcmp(result.materials[index].shader, info.shader)) {
				++index;
			}

			if(index == result.materials.size()) {
				result.materials.push_back(info);
			} else if(result.materials[index].width != info.width || result.materials[index].height != info.height ||
					  result.materials[index].scripted != info.scripted ||
					  std::strcmp(result.materials[index].editor_image, info.editor_image) ||
					  std::strcmp(result.materials[index].diffuse, info.diffuse)) {
				return reject(error, context + "Conflicting material binding");
			}

			material = material_indices.emplace(surface.material, index).first;
		}

		const auto &info = result.materials[material->second];
		if(!normalized(surface.normal)) {
			return reject(error, context + "Invalid surface normal");
		}

		TextureProjection projection;
		if(!build_texture_projection(surface, info.width, info.height, projection, error)) {
			error = context + error;
			return false;
		}

		std::map<uint16_t, uint16_t> corners;
		const auto addCorner = [&](uint16_t key) {
			if(key >= mesh.vertices.size() || result.vertices.size() >= 65535) {
				return reject(error, context + "Invalid corner or vertex limit");
			}

			RenderVertex vertex{mesh.vertices[key], surface.normal, {}};
			for(float component : vertex.position) {
				if(!std::isfinite(component)) {
					return reject(error, context + "Nonfinite vertex");
				}
			}

			for(size_t row = 0; row < 2; ++row) {
				const auto &p = projection[row];
				vertex.uv[row] =
					vertex.position[0] * p[0] + vertex.position[1] * p[1] + vertex.position[2] * p[2] + p[3];
				if(!std::isfinite(vertex.uv[row])) {
					return reject(error, context + "Texture coordinate overflow");
				}
			}

			corners.emplace(key, static_cast<uint16_t>(result.vertices.size()));
			result.vertices.push_back(vertex);
			return true;
		};
		for(uint16_t key : surface.winding) {
			if(corners.count(key)) {
				return reject(error, context + "Repeated winding corner");
			}

			if(!addCorner(key)) {
				return false;
			}
		}

		const auto first   = static_cast<uint32_t>(result.indices.size());
		const size_t count = size_t(surface.triangle_count) * 3;
		for(size_t i = 0; i < count; ++i) {
			const auto key = mesh.indices[size_t(surface.first_triangle) * 3 + i];
			if(!corners.count(key) && !geometry.lighting_refined) {
				return reject(error, context + "Triangle is outside its winding");
			}

			if(!corners.count(key) && !addCorner(key)) {
				return false;
			}

			const auto corner = corners.find(key);
			result.indices.push_back(corner->second);
		}

		result.surfaces.push_back({material->second, first, static_cast<uint32_t>(count)});
	}

	if(result.indices.size() != mesh.indices.size()) {
		return reject(error, "Unmapped collision triangles");
	}

	render = std::move(result);
	return true;
	
}
} // namespace nod::world
