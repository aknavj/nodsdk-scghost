/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: world_geometry.cpp
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

#include "world_geometry.h"
#include "scene_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <utility>

/*
 */
namespace nod::world {
namespace {

/*
 */
using Vec = std::array<double, 3>;

/*
 */
struct Plane {
		Vec normal{};
		double distance = 0;
		Surface surface;
};

/*
 */
struct Polygon {
		Surface surface;
		Vec normal{};
		double distance = 0;
		std::vector<Vec> points;
};

/*
 */
struct Brush {
		std::vector<Plane> planes;
		std::vector<Polygon> polygons;
		Vec low{}, high{};
};

/*
 */
Vec sub(const Vec &a, const Vec &b) {
	return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

/*
 */
double dot(const Vec &a, const Vec &b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/*
 */
Vec cross(const Vec &a, const Vec &b) {
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

/*
 */
Vec unit(Vec value) {
	const double length = std::sqrt(dot(value, value));
	for(auto &component : value) {
		component /= length;
	}

	return value;
}

/*
 */
bool reject(std::string &error, const std::string &message) {
	error = message;
	return false;
}

/*
 */
bool read_planes(const char *text, std::vector<Plane> &planes, std::string &error) {
	using namespace nod_scene;

	Lexer lexer{ 
		reinterpret_cast<const unsigned char *>(text), std::strlen(text), 0, {}, false
	};

	int token;
	while((token = read_token(&lexer)) == 1) {
		if(!std::strcmp(lexer.token, "brushDef")) {
			return reject(error, "brushDef geometry is not supported yet");
		}

		if(planes.size() == 64) {
			return reject(error, "More than 64 brush planes");
		}

		std::array<Vec, 3> points{};
		for(size_t i = 0; i < 9; ++i) {
			float value;
			if((i && read_token(&lexer) != 1) || !parse_float(lexer.token, &value)) {
				return reject(error, "Invalid canonical brush coordinates");
			}

			points[i / 3][i % 3] = value;
		}

		Plane plane;
		if(read_token(&lexer) != 1 || lexer.is_quoted || !*lexer.token) {
			return reject(error, "Missing brush material");
		}

		plane.surface.material = lexer.token;
		for(auto &value : plane.surface.texture) {
			if(read_token(&lexer) != 1 || !parse_float(lexer.token, &value)) {
				return reject(error, "Invalid classic texture fields");
			}
		}

		if(!plane.surface.texture[3] || !plane.surface.texture[4]) {
			return reject(error, "Zero texture scale");
		}

		for(auto &flag : plane.surface.flags) {
			if(read_token(&lexer) != 1 || !parse_unsigned_integer(lexer.token, UINT32_MAX, &flag)) {
				return reject(error, "Invalid brush flags");
			}
		}

		const Vec first	 = sub(points[0], points[1]);
		const Vec second = sub(points[2], points[1]);
		const Vec normal = cross(first, second);
		if(dot(normal, normal) <= dot(first, first) * dot(second, second) * 1e-20) {
			return reject(error, "Degenerate brush plane");
		}

		plane.normal = unit(normal);
		std::array<float, 3> editor_first{}, editor_second{};
		for(size_t axis = 0; axis < 3; ++axis) {
			editor_first[axis]	= static_cast<float>(points[0][axis]) - static_cast<float>(points[1][axis]);
			editor_second[axis] = static_cast<float>(points[2][axis]) - static_cast<float>(points[1][axis]);
		}

		Vec editor_normal{};
		for(size_t axis = 0; axis < 3; ++axis) {
			editor_normal[axis] = editor_first[(axis + 1) % 3] * editor_second[(axis + 2) % 3] -
								  editor_first[(axis + 2) % 3] * editor_second[(axis + 1) % 3];
		}

		const double editor_length = std::sqrt(dot(editor_normal, editor_normal));
		for(size_t axis = 0; axis < 3; ++axis) {
			plane.surface.texture_normal[axis] =
				editor_length ? static_cast<float>(editor_normal[axis] / editor_length) : 0;
		}

		plane.distance = dot(plane.normal, points[0]);
		planes.push_back(std::move(plane));
	}

	if(token < 0) {
		return reject(error, "Invalid canonical brush tokens");
	}

	if(planes.size() < 4) {
		return reject(error, "A solid brush needs at least four planes");
	}

	return true;
}

/*
 */
bool reconstruct(Brush &input, size_t brush, Geometry &result, std::string &error) {
	auto &planes = input.planes;
	double scale = 1;
	for(const auto &plane : planes) {
		scale = std::max(scale, std::abs(plane.distance));
	}

	const double epsilon = 1e-7 + scale * 1e-9;
	for(size_t i = 0; i < planes.size(); ++i) {
		for(size_t j = 0; j < i; ++j) {
			if(dot(sub(planes[i].normal, planes[j].normal), sub(planes[i].normal, planes[j].normal)) < 1e-20 &&
			   std::abs(planes[i].distance - planes[j].distance) <= epsilon) {
				return reject(error, "Duplicate brush plane");
			}
		}
	}

	std::vector<Vec> vertices;
	for(size_t i = 0; i < planes.size(); ++i) {
		for(size_t j = i + 1; j < planes.size(); ++j) {
			for(size_t k = j + 1; k < planes.size(); ++k) {
				const Vec a				 = cross(planes[j].normal, planes[k].normal);
				const double determinant = dot(planes[i].normal, a);
				if(std::abs(determinant) < 1e-10) {
					continue;
				}

				const Vec b = cross(planes[k].normal, planes[i].normal);
				const Vec c = cross(planes[i].normal, planes[j].normal);
				Vec point{};
				for(size_t axis = 0; axis < 3; ++axis) {
					point[axis] =
						(planes[i].distance * a[axis] + planes[j].distance * b[axis] + planes[k].distance * c[axis]) /
						determinant;
				}

				bool inside = true;
				for(const auto &plane : planes) {
					if(dot(plane.normal, point) - plane.distance > epsilon) {
						inside = false;
						break;
					}
				}

				if(!inside) {
					continue;
				}

				for(const auto &vertex : vertices) {
					if(dot(sub(point, vertex), sub(point, vertex)) <= epsilon * epsilon) {
						inside = false;
						break;
					}
				}

				if(inside) {
					vertices.push_back(point);
				}
			}
		}
	}

	if(vertices.size() < 4) {
		return reject(error, "Empty, unbounded or sub-precision brush");
	}

	std::sort(vertices.begin(), vertices.end());
	input.low = input.high = vertices.front();
	for(const auto &point : vertices) {
		for(size_t a = 0; a < 3; ++a) {
			input.low[a]  = std::min(input.low[a], point[a]);
			input.high[a] = std::max(input.high[a], point[a]);
		}
	}

	if(result.mesh.vertices.size() + vertices.size() > 65535) {
		return reject(error, "World exceeds 65535 collision vertices");
	}

	const size_t base = result.mesh.vertices.size();
	for(const auto &point : vertices) {
		std::array<float, 3> vertex{};
		for(size_t axis = 0; axis < 3; ++axis) {
			vertex[axis] = static_cast<float>(point[axis]);
			if(!std::isfinite(vertex[axis])) {
				return reject(error, "Brush vertex exceeds binary32 range");
			}
		}

		result.mesh.vertices.push_back(vertex);
	}

	std::map<std::pair<size_t, size_t>, std::pair<unsigned, int>> edges;
	double volume			   = 0;
	const size_t first_surface = result.surfaces.size();
	for(auto &plane : planes) {
		std::vector<size_t> winding;
		Vec center{};
		for(size_t i = 0; i < vertices.size(); ++i) {
			if(std::abs(dot(plane.normal, vertices[i]) - plane.distance) <= epsilon) {
				winding.push_back(i);
				for(size_t axis = 0; axis < 3; ++axis) {
					center[axis] += vertices[i][axis];
				}
			}
		}

		if(winding.size() < 3) {
			continue; // Redundant half-spaces have no polygon.
		}

		for(auto &value : center) {
			value /= static_cast<double>(winding.size());
		}

		size_t axis = 0;
		for(size_t a = 1; a < 3; ++a) {
			if(std::abs(plane.normal[a]) < std::abs(plane.normal[axis])) {
				axis = a;
			}
		}

		Vec reference{};
		reference[axis] = 1;
		const Vec u		= unit(cross(reference, plane.normal));
		const Vec v		= cross(plane.normal, u);
		std::stable_sort(winding.begin(), winding.end(), [&](size_t a, size_t b) {
			const Vec first	 = sub(vertices[a], center);
			const Vec second = sub(vertices[b], center);
			return std::atan2(dot(first, v), dot(first, u)) < std::atan2(dot(second, v), dot(second, u));
		});
		Surface surface = std::move(plane.surface);
		surface.brush	= brush;
		for(size_t a = 0; a < 3; ++a) {
			surface.normal[a] = static_cast<float>(plane.normal[a]);
		}

		for(const auto index : winding) {
			surface.winding.push_back(static_cast<uint16_t>(base + index));
		}

		surface.first_triangle = static_cast<uint32_t>(result.mesh.indices.size() / 3);
		surface.triangle_count = static_cast<uint32_t>(winding.size() - 2);
		if(result.mesh.indices.size() / 3 + surface.triangle_count > 4096) {
			return reject(error, "World exceeds 4096 collision triangles");
		}

		for(size_t i = 0; i < winding.size(); ++i) {
			const size_t from  = winding[i];
			const size_t to	   = winding[(i + 1) % winding.size()];
			const auto ordered = std::minmax(from, to);
			auto &edge		   = edges[{ordered.first, ordered.second}];
			++edge.first;
			edge.second += from < to ? 1 : -1;
		}

		for(size_t i = 1; i + 1 < winding.size(); ++i) {
			const Vec first	 = sub(vertices[winding[i]], vertices[winding[0]]);
			const Vec second = sub(vertices[winding[i + 1]], vertices[winding[0]]);
			if(dot(cross(first, second), plane.normal) <= epsilon * epsilon) {
				return reject(error, "Degenerate brush polygon");
			}

			Vec rounded_first{}, rounded_second{};
			for(size_t a = 0; a < 3; ++a) {
				rounded_first[a] =
					double(result.mesh.vertices[surface.winding[i]][a]) - result.mesh.vertices[surface.winding[0]][a];
				rounded_second[a] = double(result.mesh.vertices[surface.winding[i + 1]][a]) -
									result.mesh.vertices[surface.winding[0]][a];
			}

			if(dot(cross(rounded_first, rounded_second), plane.normal) <= 0) {
				return reject(error, "Brush winding loses binary32 precision");
			}

			// Translate before summing volume to avoid cancellation far from zero.
			volume += dot(sub(vertices[winding[0]], vertices[0]),
						  cross(sub(vertices[winding[i]], vertices[0]), sub(vertices[winding[i + 1]], vertices[0]))) /
					  6;
			result.mesh.indices.insert(result.mesh.indices.end(),
									   {surface.winding[0], surface.winding[i], surface.winding[i + 1]});
		}

		Polygon polygon{surface, plane.normal, plane.distance, {}};
		for(const auto index : winding) {
			polygon.points.push_back(vertices[index]);
		}

		input.polygons.push_back(std::move(polygon));
		result.surfaces.push_back(std::move(surface));
	}

	if(result.surfaces.size() - first_surface < 4 || volume <= epsilon * epsilon * epsilon) {
		return reject(error, "Brush has no closed positive volume");
	}

	for(const auto &edge : edges) {
		if(edge.second.first != 2 || edge.second.second != 0) {
			return reject(error, "Brush boundary is open or non-manifold");
		}
	}

	return true;
}

/*
 */
double tolerance(const Vec &point) {
	double scale = 1;
	for(double value : point) {
		scale = std::max(scale, std::abs(value));
	}

	return 128 * std::numeric_limits<double>::epsilon() * scale;
}

/*
 */
bool spend(size_t &budget, size_t count, std::string &error) {
	if(count > budget) {
		return reject(error, "World CSG complexity limit exceeded");
	}

	budget -= count;
	return true;
}

// Keep outside pieces while the inside remainder proceeds through the brush's
// other planes. Only the final remainder is covered by the entire brush.
bool subtract(const Polygon &polygon, const Brush &brush, size_t other, std::vector<Polygon> &outside, size_t &budget,
			  std::string &error) {
	if(!spend(budget, polygon.points.size(), error)) {
		return false;
	}

	double epsilon = tolerance(polygon.points.front());
	Vec low		   = polygon.points.front();
	Vec high	   = low;
	for(const auto &point : polygon.points) {
		epsilon = std::max(epsilon, tolerance(point));
		for(size_t a = 0; a < 3; ++a) {
			low[a]	= std::min(low[a], point[a]);
			high[a] = std::max(high[a], point[a]);
		}
	}

	for(size_t a = 0; a < 3; ++a) {
		if(high[a] < brush.low[a] - epsilon || low[a] > brush.high[a] + epsilon) {
			outside.push_back(polygon);
			return true;
		}
	}

	for(const auto &plane : brush.planes) {
		if(!spend(budget, polygon.points.size(), error)) {
			return false;
		}

		const Vec difference = sub(polygon.normal, plane.normal);
		if(dot(difference, difference) < 1e-24 && std::abs(polygon.distance - plane.distance) <= epsilon &&
		   polygon.surface.brush < other) {
			outside.push_back(polygon);
			return true;
		}

		double minimum_distance = std::numeric_limits<double>::infinity();
		double maximum_distance = -minimum_distance;
		for(const auto &point : polygon.points) {
			const double distance = dot(plane.normal, point) - plane.distance;
			minimum_distance	  = std::min(minimum_distance, distance);
			maximum_distance	  = std::max(maximum_distance, distance);
		}

		if(minimum_distance >= -epsilon && maximum_distance > epsilon) {
			outside.push_back(polygon);
			return true;
		}
	}

	Polygon inside = polygon;
	for(const auto &plane : brush.planes) {
		if(!spend(budget, inside.points.size(), error)) {
			return false;
		}

		std::vector<double> distances;
		std::vector<int> sides;
		bool front = false;
		bool back  = false;
		for(const auto &point : inside.points) {
			const double distance = dot(plane.normal, point) - plane.distance;
			const int side		  = distance > epsilon ? 1 : distance < -epsilon ? -1 : 0;
			distances.push_back(distance);
			sides.push_back(side);
			front = front || side > 0;
			back  = back || side < 0;
		}

		if(!front) {
			continue;
		}

		if(!back) {
			outside.push_back(std::move(inside));
			return true;
		}

		Polygon cut_front = inside;
		Polygon cut_back  = inside;
		cut_front.points.clear();
		cut_back.points.clear();
		for(size_t i = 0; i < inside.points.size(); ++i) {
			const size_t next = (i + 1) % inside.points.size();
			const Vec &a	  = inside.points[i];
			const Vec &b	  = inside.points[next];
			if(sides[i] >= 0) {
				cut_front.points.push_back(a);
			}

			if(sides[i] <= 0) {
				cut_back.points.push_back(a);
			}

			if(sides[i] * sides[next] != -1) {
				continue;
			}

			const double fraction = distances[i] / (distances[i] - distances[next]);
			Vec point{};
			for(size_t axis = 0; axis < 3; ++axis) {
				point[axis] = a[axis] + fraction * (b[axis] - a[axis]);
				if(std::abs(plane.normal[axis]) == 1) {
					point[axis] = plane.distance / plane.normal[axis];
				}
			}

			cut_front.points.push_back(point);
			cut_back.points.push_back(point);
		}

		outside.push_back(std::move(cut_front));
		inside = std::move(cut_back);
		if(outside.size() > 8192) {
			return reject(error, "Too many world CSG fragments");
		}
	}

	return true;
}

/*
 */
bool triangulate(Surface &surface, const std::vector<Vec> &precise, Geometry &result, std::string &error) {
	std::vector<uint16_t> remaining = surface.winding;
	Vec normal{};
	for(size_t a = 0; a < 3; ++a) {
		normal[a] = surface.normal[a];
	}

	const auto point		= [&](uint16_t index) { return precise[index]; };
	double scale			= 0;
	double distance_epsilon = 0;
	for(auto index : remaining) {
		const Vec delta	 = sub(point(index), point(remaining.front()));
		scale			 = std::max(scale, dot(delta, delta));
		distance_epsilon = std::max(distance_epsilon, tolerance(point(index)));
	}

	const double epsilon   = scale * 128 * std::numeric_limits<double>::epsilon() + distance_epsilon * std::sqrt(scale);
	surface.first_triangle = static_cast<uint32_t>(result.mesh.indices.size() / 3);
	while(remaining.size() >= 3) {
		bool clipped = false;
		for(size_t i = 0; i < remaining.size(); ++i) {
			const size_t previous = (i + remaining.size() - 1) % remaining.size();
			const size_t next	  = (i + 1) % remaining.size();
			const Vec a			  = point(remaining[previous]);
			const Vec b			  = point(remaining[i]);
			const Vec c			  = point(remaining[next]);
			if(dot(cross(sub(b, a), sub(c, a)), normal) <= epsilon) {
				continue;
			}

			bool occupied = false;
			for(size_t j = 0; j < remaining.size(); ++j) {
				if(j == previous || j == i || j == next) {
					continue;
				}

				const Vec p = point(remaining[j]);
				if(dot(cross(sub(b, a), sub(p, a)), normal) >= -epsilon &&
				   dot(cross(sub(c, b), sub(p, b)), normal) >= -epsilon &&
				   dot(cross(sub(a, c), sub(p, c)), normal) >= -epsilon) {
					occupied = true;
					break;
				}
			}

			if(occupied) {
				continue;
			}

			Vec first{}, second{};
			for(size_t axis = 0; axis < 3; ++axis) {
				first[axis] =
					double(result.mesh.vertices[remaining[i]][axis]) - result.mesh.vertices[remaining[previous]][axis];
				second[axis] = double(result.mesh.vertices[remaining[next]][axis]) -
							   result.mesh.vertices[remaining[previous]][axis];
			}

			if(dot(cross(first, second), normal) <= 0) {
				return reject(error, "CSG winding loses binary32 precision");
			}

			if(result.mesh.indices.size() / 3 == 4096) {
				return reject(error, "World exceeds 4096 collision triangles");
			}

			result.mesh.indices.insert(result.mesh.indices.end(), {remaining[previous], remaining[i], remaining[next]});
			remaining.erase(remaining.begin() + i);
			clipped = true;
			break;
		}

		if(!clipped) {
			return reject(error, "CSG polygon cannot be triangulated at binary32 precision");
		}
	}

	surface.triangle_count = static_cast<uint32_t>(result.mesh.indices.size() / 3) - surface.first_triangle;
	return true;
}

/*
 */
bool exterior(const std::vector<Brush> &brushes, Geometry &result, std::string &error) {
	std::vector<Polygon> polygons;
	size_t budget = 4000000;
	for(const auto &brush : brushes) {
		for(const auto &polygon : brush.polygons) {
			std::vector<Polygon> fragments{polygon};
			for(size_t other = 0; other < brushes.size() && !fragments.empty(); ++other) {
				if(other == polygon.surface.brush) {
					continue;
				}

				std::vector<Polygon> outside;
				for(const auto &fragment : fragments) {
					if(!subtract(fragment, brushes[other], other, outside, budget, error)) {
						return false;
					}
				}

				fragments = std::move(outside);
				if(polygons.size() + fragments.size() > 8192) {
					return reject(error, "Too many world CSG fragments");
				}
			}

			for(auto &fragment : fragments) {
				polygons.push_back(std::move(fragment));
			}
		}
	}

	if(polygons.empty()) {
		return reject(error, "World CSG has no exterior");
	}

	result.mesh = {};
	result.surfaces.clear();
	std::map<std::array<float, 3>, uint16_t> indices;
	std::vector<Vec> precise;
	for(auto &polygon : polygons) {
		polygon.surface.winding.clear();
		for(const auto &point : polygon.points) {
			std::array<float, 3> rounded{};
			for(size_t a = 0; a < 3; ++a) {
				rounded[a] = static_cast<float>(point[a]);
			}

			auto found = indices.find(rounded);
			if(found == indices.end()) {
				if(precise.size() == 65535) {
					return reject(error, "World exceeds 65535 collision vertices");
				}

				const auto index = static_cast<uint16_t>(precise.size());
				found			 = indices.emplace(rounded, index).first;
				result.mesh.vertices.push_back(rounded);
				precise.push_back(point);
			} else {
				const Vec delta		 = sub(point, precise[found->second]);
				const double epsilon = std::max(tolerance(point), tolerance(precise[found->second]));
				if(dot(delta, delta) > epsilon * epsilon) {
					return reject(error, "CSG vertices merge at binary32 precision");
				}
			}

			polygon.surface.winding.push_back(found->second);
		}
	}

	// Use precise pre-rounding positions to identify T-junctions: a float-sized
	// distance tolerance could accidentally bridge a genuine narrow gap.
	budget = 24000000;
	for(auto &polygon : polygons) {
		auto &surface = polygon.surface;
		std::vector<uint16_t> winding;
		for(size_t edge = 0; edge < surface.winding.size(); ++edge) {
			const uint16_t from			= surface.winding[edge];
			const uint16_t to			= surface.winding[(edge + 1) % surface.winding.size()];
			const Vec a					= precise[from];
			const Vec direction			= sub(precise[to], a);
			const double length_squared = dot(direction, direction);
			if(!length_squared) {
				return reject(error, "CSG polygon has a zero-length edge");
			}

			std::vector<std::pair<double, uint16_t>> splits{{0, from}};
			if(!spend(budget, precise.size(), error)) {
				return false;
			}

			for(size_t i = 0; i < precise.size(); ++i) {
				if(i == from || i == to) {
					continue;
				}

				const Vec delta		  = sub(precise[i], a);
				const double fraction = dot(delta, direction) / length_squared;
				if(fraction <= 0 || fraction >= 1) {
					continue;
				}

				const Vec perpendicular = cross(delta, direction);
				const double epsilon	= std::max(tolerance(a), tolerance(precise[i]));
				if(dot(perpendicular, perpendicular) <= epsilon * epsilon * length_squared) {
					splits.emplace_back(fraction, static_cast<uint16_t>(i));
				}
			}

			std::sort(splits.begin(), splits.end());
			for(const auto &split : splits) {
				winding.push_back(split.second);
			}
		}

		if(winding.size() > 256) {
			return reject(error, "CSG polygon exceeds 256 boundary vertices");
		}

		surface.winding = std::move(winding);
		if(!triangulate(surface, precise, result, error)) {
			return false;
		}

		result.surfaces.push_back(std::move(surface));
	}

	std::map<std::pair<uint16_t, uint16_t>, int> edges;
	for(size_t i = 0; i < result.mesh.indices.size(); i += 3) {
		for(size_t j = 0; j < 3; ++j) {
			const auto a	   = result.mesh.indices[i + j];
			const auto b	   = result.mesh.indices[i + (j + 1) % 3];
			const auto ordered = std::minmax(a, b);
			edges[{ordered.first, ordered.second}] += a < b ? 1 : -1;
		}
	}

	for(const auto &edge : edges) {
		if(edge.second) {
			return reject(error, "CSG exterior has an open boundary");
		}
	}

	return true;
}
} // namespace

bool build_geometry(const nod_scene::Map &map, Geometry &geometry, std::string &error) {
	error.clear();
	Geometry result;
	const nod_scene::Entity *world = nullptr;
	for(size_t i = 0; i < map.count; ++i) {
		const auto *name = nod_scene::get_property(&map.entities[i], "classname");
		if(name && !std::strcmp(name, "worldspawn")) {
			if(world) {
				return reject(error, "Multiple worldspawn entities");
			}

			world = &map.entities[i];
		} else {
			result.excluded_entities.push_back(i);
		}
	}

	if(!world || !world->brush_count) {
		return reject(error, "Worldspawn has no brushes");
	}

	if(world->brush_count > 4096) {
		return reject(error, "World exceeds 4096 brushes");
	}

	std::vector<Brush> brushes(world->brush_count);
	for(size_t brush = 0; brush < world->brush_count; ++brush) {
		if(!read_planes(world->brushes[brush], brushes[brush].planes, error) ||
		   !reconstruct(brushes[brush], brush, result, error)) {
			error = "Brush " + std::to_string(brush) + ": " + error;
			return false;
		}
	}

	if(!exterior(brushes, result, error)) {
		return false;
	}

	// Check the actual binary32 mesh, not just the double-precision construction.
	if(!mopp::compile(result.mesh, result.collision_code, error)) {
		return false;
	}

	result.low = result.high = result.mesh.vertices.front();
	for(const auto &vertex : result.mesh.vertices) {
		for(size_t axis = 0; axis < 3; ++axis) {
			result.low[axis]  = std::min(result.low[axis], vertex[axis]);
			result.high[axis] = std::max(result.high[axis], vertex[axis]);
		}
	}

	result.brush_count = world->brush_count;
	geometry		   = std::move(result);
	return true;
}

} // namespace nod::world
