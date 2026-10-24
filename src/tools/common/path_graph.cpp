/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: path_graph.cpp
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
#include "NPDParser.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <limits>
#include <numeric>
#include <set>

/*
 */
namespace nod::path {
namespace {

/*
 */
const char *property(const nod_scene::Entity &entity, const char *key) {
	const auto *value = nod_scene::get_property(&entity, key);
	return value ? value : "";
}

/*
 */
bool number(const char *text, uint32_t limit, uint32_t &value) {
	return nod_scene::parse_unsigned_integer(text, limit, &value) && std::to_string(value) == text;
}

/*
 */
struct Node {
		uint16_t id;
		std::array<float, 3> position;
};

/*
 */
struct Link {
		uint16_t from;
		uint16_t to;
};

} // namespace

/*
 */
bool authoring_class(const nod_scene::Entity &entity) {
	const std::string name = property(entity, "classname");
	return name == "nod_npd_waypoint" || name == "nod_npd_link";
}

/*
 */
bool compile(const nod_scene::Map &map, Graph &out, std::string &error) {
	error.clear();
	if(!map.count || std::string(property(map.entities[0], "classname")) != "worldspawn") {
		error = "Expected a leading worldspawn";
		return false;
	}

	std::vector<Node> nodes;
	std::vector<Link> links;
	std::map<uint16_t, uint16_t> indices;
	uint32_t next_id = 0;
	for(size_t i = 0; i < map.count; ++i) {
		const auto &entity = map.entities[i];
		if(!authoring_class(entity)) {
			continue;
		}

		const auto fail = [&](const std::string &why) {
			error = "Entity " + std::to_string(i) + ": " + why;
			return false;
		};
		if(std::string(property(entity, "nod_authoring")) != "npd-world-v1" || *property(entity, "nod_id") ||
		   entity.brush_count) {
			return fail("Expected a MAP-only world-space waypoint/link, not imported NPD metadata");
		}

		if(std::string(property(entity, "nod_space")) != "world") {
			return fail("Expected nod_space world");
		}

		std::array<float, 3> position;
		if(!nod_scene::float_vector(property(entity, "origin"), position.data())) {
			return fail("Expected a finite world-space origin");
		}

		const bool node = std::string(property(entity, "classname")) == "nod_npd_waypoint";
		const std::set<std::string> allowed =
			node ? std::set<std::string>{"classname", "nod_authoring",	 "nod_space",
										 "origin",	  "nod_waypoint_id", "nod_flags"}
				 : std::set<std::string>{"classname", "nod_authoring", "nod_space",		"origin",
										 "nod_from",  "nod_to",		   "nod_link_flags"};
		std::set<std::string> keys;
		for(size_t j = 0; j < entity.pair_count; ++j) {
			const std::string key = entity.pairs[j].key;
			if(!allowed.count(key) || !keys.insert(key).second) {
				return fail("Unsupported or duplicate key: " + key);
			}
		}

		if(node) {
			uint32_t id	   = 0;
			uint32_t flags = 0;
			if(!number(property(entity, "nod_waypoint_id"), 65534, id) ||
			   !number(property(entity, "nod_flags"), 0, flags)) {
				return fail("Use a unique ID 0..65534 and normal flags 0");
			}

			if(nodes.size() == 65535 || indices.count(uint16_t(id))) {
				return fail("Too many or duplicate waypoint IDs");
			}

			indices.emplace(uint16_t(id), static_cast<uint16_t>(nodes.size()));
			nodes.push_back({uint16_t(id), position});
			next_id = std::max(next_id, id + 1);
		} else {
			uint32_t from  = 0;
			uint32_t to	   = 0;
			uint32_t flags = 0;
			if(!number(property(entity, "nod_from"), 65534, from) || !number(property(entity, "nod_to"), 65534, to) ||
			   !number(property(entity, "nod_link_flags"), 0, flags)) {
				return fail("Use waypoint IDs and normal link flags 0");
			}

			if(links.size() == 65535 || from == to) {
				return fail("Too many links or a self-link");
			}

			links.push_back({uint16_t(from), uint16_t(to)});
		}
	}

	std::vector<size_t> parents(nodes.size());
	std::vector<size_t> sizes(nodes.size(), 1);
	std::iota(parents.begin(), parents.end(), size_t(0));
	const auto root = [&](size_t value) {
		while(parents[value] != value) {
			value = parents[value];
		}

		return value;
	};
	std::set<std::pair<uint16_t, uint16_t>> unique;
	std::vector<size_t> outgoing(nodes.size(), 0);
	for(auto &link : links) {
		const auto from = indices.find(link.from);
		const auto to	= indices.find(link.to);
		if(from == indices.end() || to == indices.end()) {
			error = "Link references a missing waypoint ID";
			return false;
		}

		link = {from->second, to->second};
		if(!unique.emplace(link.from, link.to).second) {
			error = "Duplicate directed link";
			return false;
		}

		if(++outgoing[link.from] > 255) {
			error = "A waypoint has more than 255 outgoing links";
			return false;
		}

		float length2 = 0;
		for(size_t axis = 0; axis < 3; ++axis) {
			const float delta = nodes[link.from].position[axis] - nodes[link.to].position[axis];
			length2 += delta * delta;
		}

		if(!std::isnormal(length2)) {
			error = "Squared link length must be positive and representable as normal float32";
			return false;
		}

		auto a = root(link.from);
		auto b = root(link.to);
		if(a != b) {
			if(sizes[a] < sizes[b]) {
				std::swap(a, b);
			}

			parents[b] = a;
			sizes[a] += sizes[b];
		}
	}

	std::map<size_t, uint8_t> groups;
	std::vector<uint8_t> components;
	for(size_t i = 0; i < nodes.size(); ++i) {
		const auto key = root(i);
		if(!groups.count(key)) {
			if(groups.size() == 255) {
				error = "More than 255 disconnected components";
				return false;
			}

			groups.emplace(key, static_cast<uint8_t>(groups.size()));
		}

		components.push_back(groups.at(key));
	}

	std::sort(links.begin(), links.end(),
			  [](const Link &a, const Link &b) { return std::make_pair(a.from, a.to) < std::make_pair(b.from, b.to); });
	Graph result;
	result.nodes	  = nodes.size();
	result.links	  = links.size();
	result.components = groups.size();
	result.bytes.assign(8 + nodes.size() * 26 + links.size() * 6, 0);
	auto *bytes = result.bytes.data();
	put16(bytes, 2);
	put16(bytes + 2, static_cast<uint16_t>(nodes.size()));
	put16(bytes + 4, static_cast<uint16_t>(links.size()));
	put16(bytes + 6, uint16_t(next_id));
	size_t first = 0;
	for(size_t i = 0; i < nodes.size(); ++i) {
		auto *record = bytes + 8 + i * 26;
		put16(record, nodes[i].id);
		for(size_t axis = 0; axis < 3; ++axis) {
			put_float(record + 2 + axis * 4, nodes[i].position[axis]);
		}

		record[16] = components[i];
		put16(record + 17, 0xffff);
		put16(record + 19, 0xffff);
		put16(record + 21, 0);
		put16(record + 23, static_cast<uint16_t>(first));
		record[25] = static_cast<uint8_t>(outgoing[i]);
		first += outgoing[i];
	}

	for(size_t i = 0; i < links.size(); ++i) {
		auto *record = bytes + 8 + nodes.size() * 26 + i * 6;
		put16(record, links[i].from);
		put16(record + 2, links[i].to);
	}

	NPDParser check;
	if(!check.parseBuffer(bytes, result.bytes.size())) {
		error = check.getLastError();
		return false;
	}

	out = std::move(result);
	return true;
}

} // namespace nod::path
