/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: mopp_compiler.cpp
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

#include "mopp_compiler.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

/*
 */
namespace nod::mopp {
namespace {

/*
 */
constexpr size_t maximum_triangles = 4096;
constexpr double fixed_point_unit  = 65536.0;

/*
 */
struct Bounds {
		std::array<double, 3> low{}, high{};
};

/*
 */
struct Branch {
		std::vector<uint8_t> bytes;
		std::array<uint8_t, 3> low{}, high{};
};

/*
 */
bool reject(std::string &error, const std::string &message) {
	error = message;
	return false;
}

/*
 */
Bounds combine(const std::vector<Bounds> &triangles, const std::vector<uint32_t> &keys) {
	Bounds bounds = triangles[keys.front()];
	for(const auto key : keys) {
		for(size_t axis = 0; axis < 3; ++axis) {
			bounds.low[axis]  = std::min(bounds.low[axis], triangles[key].low[axis]);
			bounds.high[axis] = std::max(bounds.high[axis], triangles[key].high[axis]);
		}
	}

	return bounds;
}

/*
 */
void half(std::vector<uint8_t> &bytes, size_t value) {
	bytes.push_back(static_cast<uint8_t>(value >> 8));
	bytes.push_back(static_cast<uint8_t>(value));
}

/*
 */
Branch emit(const std::vector<Bounds> &triangles, std::vector<uint32_t> keys) {
	const Bounds bounds = combine(triangles, keys);
	Branch result;
	for(size_t axis = 0; axis < 3; ++axis) {
		// One quantization cell of slack protects native float-to-fixed rounding.
		result.low[axis]  = static_cast<uint8_t>(std::floor(bounds.low[axis]) - 1);
		result.high[axis] = static_cast<uint8_t>(std::ceil(bounds.high[axis]) + 1);
		result.bytes.insert(result.bytes.end(),
							{static_cast<uint8_t>(0x26 + axis), result.low[axis], result.high[axis]});
	}

	if(keys.size() == 1) {
		const uint32_t key = keys.front();
		if(key < 32) {
			result.bytes.push_back(static_cast<uint8_t>(0x30 + key));
		} else if(key < 256) {
			result.bytes.insert(result.bytes.end(), {0x50, static_cast<uint8_t>(key)});
		} else {
			result.bytes.push_back(0x51);
			half(result.bytes, key);
		}

		return result;
	}

	size_t axis = 0;
	for(size_t candidate = 1; candidate < 3; ++candidate) {
		if(bounds.high[candidate] - bounds.low[candidate] > bounds.high[axis] - bounds.low[axis]) {
			axis = candidate;
		}
	}

	std::stable_sort(keys.begin(), keys.end(), [&](uint32_t a, uint32_t b) {
		return triangles[a].low[axis] + triangles[a].high[axis] < triangles[b].low[axis] + triangles[b].high[axis];
	});
	const auto middle = keys.begin() + keys.size() / 2;
	Branch left		  = emit(triangles, {keys.begin(), middle});
	Branch right	  = emit(triangles, {middle, keys.end()});
	if(left.bytes.size() <= 255) {
		result.bytes.insert(result.bytes.end(), {static_cast<uint8_t>(0x10 + axis), left.high[axis], right.low[axis],
												 static_cast<uint8_t>(left.bytes.size())});
	} else {
		result.bytes.insert(result.bytes.end(), {static_cast<uint8_t>(0x23 + axis), left.high[axis], right.low[axis]});
		half(result.bytes, 0);
		half(result.bytes, left.bytes.size());
	}

	result.bytes.insert(result.bytes.end(), left.bytes.begin(), left.bytes.end());
	result.bytes.insert(result.bytes.end(), right.bytes.begin(), right.bytes.end());
	return result;
}
} // namespace

/*
 */
bool compile(const Mesh &mesh, Code &code, std::string &error) {
	error.clear();
	if(mesh.vertices.empty() || mesh.vertices.size() > 65535) {
		return reject(error, "MOPP requires 1-65535 vertices");
	}

	if(mesh.indices.empty() || mesh.indices.size() % 3 || mesh.indices.size() / 3 > maximum_triangles) {
		return reject(error, "MOPP requires 1-4096 triangles");
	}

	for(const auto &vertex : mesh.vertices) {
		for(float value : vertex) {
			if(!std::isfinite(value)) {
				return reject(error, "Non-finite collision vertex");
			}
		}
	}

	std::vector<Bounds> triangles(mesh.indices.size() / 3);
	for(size_t triangle = 0; triangle < triangles.size(); ++triangle) {
		std::array<std::array<float, 3>, 3> points;
		for(size_t corner = 0; corner < 3; ++corner) {
			const auto index = mesh.indices[triangle * 3 + corner];
			if(index >= mesh.vertices.size()) {
				return reject(error, "Collision index outside vertex buffer");
			}

			points[corner] = mesh.vertices[index];
		}

		std::array<double, 3> first{}, second{}, normal{};
		double first_length	 = 0;
		double second_length = 0;
		double normal_length = 0;
		for(size_t axis = 0; axis < 3; ++axis) {
			first[axis]	 = double(points[1][axis]) - points[0][axis];
			second[axis] = double(points[2][axis]) - points[0][axis];
			first_length += first[axis] * first[axis];
			second_length += second[axis] * second[axis];
			triangles[triangle].low[axis]  = std::min({points[0][axis], points[1][axis], points[2][axis]});
			triangles[triangle].high[axis] = std::max({points[0][axis], points[1][axis], points[2][axis]});
		}

		for(size_t axis = 0; axis < 3; ++axis) {
			const size_t b = (axis + 1) % 3;
			const size_t c = (axis + 2) % 3;
			normal[axis]   = first[b] * second[c] - first[c] * second[b];
			normal_length += normal[axis] * normal[axis];
		}

		if(normal_length <= first_length * second_length * 1e-20) {
			return reject(error, "Degenerate collision triangle " + std::to_string(triangle));
		}
	}

	std::vector<uint32_t> keys(triangles.size());
	std::iota(keys.begin(), keys.end(), 0);
	const Bounds bounds = combine(triangles, keys);
	double span			= 0;
	for(size_t axis = 0; axis < 3; ++axis) {
		span = std::max(span, bounds.high[axis] - bounds.low[axis]);
	}

	Code result;
	const double scale = 250.0 / span;
	result.info[3]	   = static_cast<float>(scale * fixed_point_unit);
	for(size_t axis = 0; axis < 3; ++axis) {
		result.info[axis] = static_cast<float>(bounds.low[axis] - 2.0 / scale);
	}

	if(!std::isfinite(result.info[3]) || result.info[3] <= 0) {
		return reject(error, "Collision scale is not representable by native MOPP");
	}

	for(size_t axis = 0; axis < 3; ++axis) {
		if(!std::isfinite(result.info[axis])) {
			return reject(error, "Collision offset is not representable by native MOPP");
		}

		for(auto &triangle : triangles) {
			triangle.low[axis]	= (triangle.low[axis] - result.info[axis]) * result.info[3] / fixed_point_unit;
			triangle.high[axis] = (triangle.high[axis] - result.info[axis]) * result.info[3] / fixed_point_unit;
			if(triangle.low[axis] < 1 || triangle.high[axis] > 254) {
				return reject(error, "Collision coordinates lose native MOPP quantization precision");
			}
		}
	}

	// With 4096 leaves, even the largest left subtree is below the 16-bit
	// branch limit (12 bytes/leaf + 16 bytes/internal node).
	result.bytes = emit(triangles, std::move(keys)).bytes;
	code		 = std::move(result);
	return true;
}

} // namespace nod::mopp
