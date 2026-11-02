/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_lighting.cpp
Version: 0.6.8

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

#include "world_lighting.h"
#include "world_binary.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

/*
 */
namespace nod::world {
namespace {

/*
 */
using Vec = std::array<double, 3>;

/*
 */
Vec vector(const std::array<float, 3> &v) {
	return {v[0], v[1], v[2]};
}

/*
 */
Vec sub(const Vec &a, const Vec &b) {
	return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

/*
 */
Vec cross(const Vec &a, const Vec &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

/*
 */
double dot(const Vec &a, const Vec &b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/*
 */
bool reject(std::string &error, const char *message) {
	error = message;
	return false;
}

/*
 */
class LightingOccluders {
	struct Triangle {
		Vec a, ab, ac, low, high;
	};

	struct Node {
		Vec low, high;
		size_t begin, end, left = 0, right = 0;
	};

	std::vector<Triangle> triangles;
	std::vector<size_t> order;
	std::vector<Node> nodes;

	static bool overlaps(const Vec &low, const Vec &high, const Vec &other_low, const Vec &other_high) {
		for(size_t axis = 0; axis < 3; ++axis) {
			if(high[axis] < other_low[axis] || low[axis] > other_high[axis]) {
				return false;
			}
		}

		return true;
	}

	static bool hits(const Triangle &triangle, const Vec &origin, const Vec &ray) {
		const Vec &a			 = triangle.a;
		const Vec &ab			 = triangle.ab;
		const Vec &ac			 = triangle.ac;
		const Vec p				 = cross(ray, ac);
		const double determinant = dot(ab, p);
		if(std::abs(determinant) < 1e-14) {
			return false;
		}

		const Vec distance = sub(origin, a);
		const double u	   = dot(distance, p) / determinant;
		if(u < -1e-9 || u > 1 + 1e-9) {
			return false;
		}

		const Vec q	   = cross(distance, ab);
		const double v = dot(ray, q) / determinant;
		if(v < -1e-9 || u + v > 1 + 1e-9) {
			return false;
		}

		const double t = dot(ac, q) / determinant;
		return t > 1e-8 && t < 1 - 1e-8;
	}

	size_t build_node(size_t begin, size_t end) {
		Node node{triangles[order[begin]].low, triangles[order[begin]].high, begin, end};
		for(size_t i = begin + 1; i < end; ++i) {
			const Triangle &triangle = triangles[order[i]];
			for(size_t axis = 0; axis < 3; ++axis) {
				node.low[axis] = std::min(node.low[axis], triangle.low[axis]);
				node.high[axis] = std::max(node.high[axis], triangle.high[axis]);
			}
		}

		const size_t index = nodes.size();
		nodes.push_back(node);
		if(end - begin > 8) {
			size_t axis = 0;
			for(size_t candidate = 1; candidate < 3; ++candidate) {
				if(node.high[candidate] - node.low[candidate] > node.high[axis] - node.low[axis]) {
					axis = candidate;
				}
			}

			const size_t middle = begin + (end - begin) / 2;
			std::nth_element(order.begin() + begin, order.begin() + middle, order.begin() + end,
							 [&](size_t a, size_t b) {
								 const double first = triangles[a].low[axis] + triangles[a].high[axis];
								 const double second = triangles[b].low[axis] + triangles[b].high[axis];
								 return first < second || (first == second && a < b);
							 });
			const size_t left = build_node(begin, middle);
			const size_t right = build_node(middle, end);
			nodes[index].left = left;
			nodes[index].right = right;
		}

		return index;
	}

	bool blocked_node(size_t index, const Vec &origin, const Vec &ray, const Vec &low, const Vec &high,
					  size_t &budget) const {
		if(!budget) {
			return true;
		}

		--budget;
		const Node &node = nodes[index];
		if(!overlaps(low, high, node.low, node.high)) {
			return false;
		}

		if(node.left) {
			return blocked_node(node.left, origin, ray, low, high, budget) ||
				   blocked_node(node.right, origin, ray, low, high, budget);
		}

		for(size_t i = node.begin; i < node.end; ++i) {
			if(!budget) {
				return true;
			}

			--budget;
			const Triangle &triangle = triangles[order[i]];
			if(overlaps(low, high, triangle.low, triangle.high) && hits(triangle, origin, ray)) {
				return true;
			}
		}

		return false;
	}

public:
	bool build(const mopp::Mesh &mesh, std::string &error) {
		if(mesh.indices.size() % 3) {
			return reject(error, "Invalid lighting occluder mesh");
		}

		triangles.reserve(mesh.indices.size() / 3);
		order.reserve(mesh.indices.size() / 3);
		for(size_t i = 0; i < mesh.indices.size(); i += 3) {
			std::array<Vec, 3> points;
			for(size_t corner = 0; corner < 3; ++corner) {
				const auto index = mesh.indices[i + corner];
				if(index >= mesh.vertices.size()) {
					return reject(error, "Invalid lighting occluder index");
				}

				points[corner] = vector(mesh.vertices[index]);
				for(double value : points[corner]) {
					if(!std::isfinite(value)) {
						return reject(error, "Invalid lighting occluder vertex");
					}
				}
			}

			Triangle triangle{points[0], sub(points[1], points[0]), sub(points[2], points[0]), {}, {}};
			for(size_t axis = 0; axis < 3; ++axis) {
				triangle.low[axis] = std::min({points[0][axis], points[1][axis], points[2][axis]});
				triangle.high[axis] = std::max({points[0][axis], points[1][axis], points[2][axis]});
			}

			order.push_back(triangles.size());
			triangles.push_back(triangle);
		}

		if(!order.empty()) {
			build_node(0, order.size());
		}

		return true;
	}

	bool blocked(const Vec &origin, const Vec &ray, size_t &budget) const {
		if(nodes.empty()) {
			return false;
		}

		Vec low{}, high{};
		for(size_t axis = 0; axis < 3; ++axis) {
			low[axis] = std::min(origin[axis], origin[axis] + ray[axis]);
			high[axis] = std::max(origin[axis], origin[axis] + ray[axis]);
		}

		// Keep the original bounding-box and triangle tolerances, including near-edge hits.
		return blocked_node(0, origin, ray, low, high, budget);
	}
};
} // namespace

/*
 */
bool refine_lighting_geometry(const Geometry &input, float spacing, Geometry &output, std::string &error) {
	error.clear();
	if(!std::isfinite(spacing) || spacing < 0.01f || spacing > 1024 || input.mesh.indices.empty() ||
	   input.mesh.indices.size() % 3 || input.mesh.indices.size() / 3 > 4096) {
		return reject(error, "Invalid lighting geometry or spacing");
	}

	Geometry result = input;
	for(;;) {
		double longest = double(spacing) * spacing;
		std::array<uint16_t, 2> edge{};
		bool found = false;
		for(size_t i = 0; i < result.mesh.indices.size(); i += 3) {
			for(size_t j = 0; j < 3; ++j) {
				const auto a = result.mesh.indices[i + j];
				const auto b = result.mesh.indices[i + (j + 1) % 3];
				if(a >= result.mesh.vertices.size() || b >= result.mesh.vertices.size()) {
					return reject(error, "Lighting index outside mesh");
				}

				const Vec delta		 = sub(vector(result.mesh.vertices[a]), vector(result.mesh.vertices[b]));
				const double squared = dot(delta, delta);
				if(squared > longest) {
					longest = squared;
					edge	= {a, b};
					found	= true;
				}
			}
		}

		if(!found) {
			break;
		}

		if(result.mesh.vertices.size() == 65535) {
			return reject(error, "Lighting exceeds 65535 vertices; increase nod_light_spacing");
		}

		const auto a = result.mesh.vertices[edge[0]];
		const auto b = result.mesh.vertices[edge[1]];
		std::array<float, 3> midpoint{};
		for(size_t axis = 0; axis < 3; ++axis) {
			midpoint[axis] = static_cast<float>((double(a[axis]) + b[axis]) / 2);
		}

		if(midpoint == a || midpoint == b) {
			return reject(error, "Lighting subdivision loses float precision; increase nod_light_spacing");
		}

		const auto middle = static_cast<uint16_t>(result.mesh.vertices.size());
		result.mesh.vertices.push_back(midpoint);
		std::vector<uint16_t> indices;
		for(auto &surface : result.surfaces) {
			const size_t begin = surface.first_triangle * 3;
			const size_t end   = begin + surface.triangle_count * 3;
			if(end > result.mesh.indices.size()) {
				return reject(error, "Invalid lighting surface span");
			}

			surface.first_triangle = static_cast<uint32_t>(indices.size() / 3);
			for(size_t i = begin; i < end; i += 3) {
				bool split = false;
				for(size_t j = 0; j < 3; ++j) {
					const auto from = result.mesh.indices[i + j];
					const auto to	= result.mesh.indices[i + (j + 1) % 3];
					if(!((from == edge[0] && to == edge[1]) || (from == edge[1] && to == edge[0]))) {
						continue;
					}

					const auto third = result.mesh.indices[i + (j + 2) % 3];
					indices.insert(indices.end(), {from, middle, third, middle, to, third});
					split = true;
					break;
				}

				if(!split) {
					indices.insert(indices.end(), result.mesh.indices.begin() + i, result.mesh.indices.begin() + i + 3);
				}

				if(indices.size() / 3 > 4096) {
					error = "Lighting exceeds 4096 triangles (partial subdivision count: " +
							std::to_string(indices.size() / 3) + ", nod_light_spacing: " + std::to_string(spacing) +
							"); increase nod_light_spacing";
					return false;
				}
			}

			surface.triangle_count = static_cast<uint32_t>(indices.size() / 3) - surface.first_triangle;
		}

		result.mesh.indices = std::move(indices);
	}

	if(!mopp::compile(result.mesh, result.collision_code, error)) {
		return false;
	}

	result.lighting_refined = true;
	output					= std::move(result);
	return true;
}

/*
 */
bool bake_lighting(const Geometry &occluders, const lighting::Settings &settings, RenderGeometry &render,
				   std::string &error) {
	error.clear();
	if(!settings.active()) {
		return true;
	}

	if(render.indices.empty() || render.indices.size() % 3 || (settings.samples != 1 && settings.samples != 4)) {
		return reject(error, "Invalid lighting render data or sample count");
	}

	LightingOccluders shadow_mesh;
	if(settings.shadows && (!settings.points.empty() || settings.sun) && !shadow_mesh.build(occluders.mesh, error)) {
		return false;
	}

	RenderGeometry result = render;
	std::vector<std::vector<std::pair<uint16_t, uint16_t>>> adjacent(result.vertices.size());
	std::map<std::array<float, 3>, std::vector<Vec>> face_normals;
	for(size_t i = 0; i < result.indices.size(); i += 3) {
		const auto a = result.indices[i];
		const auto b = result.indices[i + 1];
		const auto c = result.indices[i + 2];
		if(a >= result.vertices.size() || b >= result.vertices.size() || c >= result.vertices.size()) {
			return reject(error, "Invalid lighting render index");
		}

		const Vec area = cross(sub(vector(result.vertices[b].position), vector(result.vertices[a].position)),
							   sub(vector(result.vertices[c].position), vector(result.vertices[a].position)));
		for(size_t j = 0; j < 3; ++j) {
			const auto corner = result.indices[i + j];
			adjacent[corner].push_back({result.indices[i + (j + 1) % 3], result.indices[i + (j + 2) % 3]});
			if(settings.phong > 0) {
				face_normals[result.vertices[corner].position].push_back(area);
			}
		}
	}

	if(settings.phong > 0) {
		const double threshold = std::cos(settings.phong * 3.14159265358979323846 / 180);
		for(auto &vertex : result.vertices) {
			Vec sum{};
			const Vec normal = vector(vertex.normal);
			for(const auto &face : face_normals[vertex.position]) {
				const double length = std::sqrt(dot(face, face));
				if(length > 1e-14 && dot(normal, face) / length >= threshold - 1e-7) {
					for(size_t axis = 0; axis < 3; ++axis) {
						sum[axis] += face[axis];
					}
				}
			}

			const double length = std::sqrt(dot(sum, sum));
			if(length <= 1e-14) {
				return reject(error, "Phong smoothing produced a degenerate normal; reduce angle");
			}

			for(size_t axis = 0; axis < 3; ++axis) {
				vertex.normal[axis] = static_cast<float>(sum[axis] / length);
			}
		}
	}

	size_t budget			  = 64000000;
	const Vec extent		  = sub(vector(occluders.high), vector(occluders.low));
	const double sun_distance = std::sqrt(dot(extent, extent)) + 1;
	for(size_t index = 0; index < result.vertices.size(); ++index) {
		auto &vertex		  = result.vertices[index];
		const Vec normal	  = vector(vertex.normal);
		const Vec bias_normal = vector(render.vertices[index].normal);
		Vec color{};
		for(unsigned sample = 0; sample < settings.samples; ++sample) {
			Vec position = vector(vertex.position);
			if(sample && !adjacent[index].empty()) {
				const auto edge		= adjacent[index][(sample - 1) % adjacent[index].size()];
				const auto q		= vector(result.vertices[edge.first].position);
				const auto r		= vector(result.vertices[edge.second].position);
				const double weight = sample * 0.05;
				for(size_t axis = 0; axis < 3; ++axis) {
					position[axis] = position[axis] * 0.8 + q[axis] * weight + r[axis] * (0.2 - weight);
				}
			}

			Vec start = position;
			for(size_t axis = 0; axis < 3; ++axis) {
				start[axis] += bias_normal[axis] * 0.0001;
			}

			for(const auto &light : settings.points) {
				const Vec delta		= sub(vector(light.position), position);
				const double length = std::sqrt(dot(delta, delta));
				if(length <= 1e-8 || length > 9999) {
					continue;
				}

				const double cosine = std::max(0.0, dot(normal, delta) / length);
				if(!cosine) {
					continue;
				}

				const bool shadow =
					settings.shadows && shadow_mesh.blocked(start, sub(vector(light.position), start), budget);
				if(!budget) {
					return reject(error, "Lighting ray budget exceeded; increase nod_light_spacing or reduce lights");
				}

				if(shadow) {
					continue;
				}

				const double strength =
					cosine * light.intensity / (1 + light.falloff * length + 1.2 * light.falloff * length * length);
				for(size_t axis = 0; axis < 3; ++axis) {
					color[axis] += light.color[axis] * strength / settings.samples;
				}
			}

			if(settings.sun) {
				Vec ray{};
				for(size_t axis = 0; axis < 3; ++axis) {
					ray[axis] = -settings.sun_direction[axis];
				}

				const double cosine = std::max(0.0, dot(normal, ray));
				for(double &component : ray) {
					component *= sun_distance;
				}

				const bool shadow = cosine > 0 && settings.shadows && shadow_mesh.blocked(start, ray, budget);
				if(!budget) {
					return reject(error, "Lighting ray budget exceeded; increase nod_light_spacing or reduce lights");
				}

				if(!shadow) {
					for(size_t axis = 0; axis < 3; ++axis) {
						color[axis] += settings.sun_color[axis] * settings.sun_intensity * cosine / settings.samples;
					}
				}
			}
		}

		if(settings.ambient) {
			for(size_t axis = 0; axis < 3; ++axis) {
				color[axis] += std::min(1.0, double(settings.ambient_color[axis]) * settings.ambient_intensity) *
							   0.16326530277729034;
			}
		}

		uint32_t packed = 0xff000000;
		for(size_t axis = 0; axis < 3; ++axis) {
			packed |= static_cast<uint32_t>(std::lround(std::min(1.0, color[axis]) * 255)) << (16 - 8 * axis);
		}

		vertex.diffuse = packed;
	}

	result.static_lights = lighting::records(settings);
	if(settings.sun) {
		detail::Writer writer;
		writer.word(0);
		writer.word(2);
		writer.word(0);
		for(float component : settings.sun_color) {
			writer.real(component * 255);
		}

		writer.real(settings.sun_intensity);
		writer.real(settings.sun_intensity);
		writer.vector(settings.sun_direction);
		writer.vector({0, 0, 0});
		writer.real(0);
		writer.real(0);
		writer.real(0);
		writer.real(0);
		std::copy(writer.bytes.begin(), writer.bytes.end(), result.global_light.begin());
	}

	if(settings.ambient) {
		for(size_t axis = 0; axis < 3; ++axis) {
			result.ambient_light[axis] = static_cast<float>(
				std::min(1.0, double(settings.ambient_color[axis]) * settings.ambient_intensity) * 255);
		}
	}

	render = std::move(result);
	return true;
}

} // namespace nod::world
