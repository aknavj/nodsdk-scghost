/*
===========================================================================
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

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

#include "NODCheck.h"
#include <algorithm>

/*
 */
namespace {

uint16_t u16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }

const uint8_t *groupRecord(const NODParser::ModelData &m, uint32_t group) {
	return m.raw_data + m.mesh_group_offset + size_t(group) * 56;
}

} // namespace

/*
 */
uint16_t nodGroupVertexCount(const NODParser::ModelData &m, uint32_t group) {
	return u16(groupRecord(m, group) + 0x1C);
}

/*
 */
bool nodGroupEmpty(const NODParser::ModelData &m, uint32_t group) {
	const auto &lod = m.mesh_groups[group].lods[0];
	return nodGroupVertexCount(m, group) == 0 && lod.strip_count + lod.list_count == 0;
}

/*
 */
std::vector<NODFault> nodFaults(const NODParser::ModelData &m) {
	std::vector<NODFault> faults;
	for(uint32_t b = 0; b < m.bone_count; ++b) {
		const uint8_t parent = m.bones[b].parent;
		if(parent != 0xFF && parent >= b)
			faults.push_back({-1, "bone " + std::to_string(b) + " has parent " + std::to_string(parent)});
	}
	uint32_t indexSum = 0;
	uint32_t advance[NODParser::MAX_VERTEX_SETS] = {0};
	for(uint32_t g = 0; g < m.mesh_group_count; ++g) {
		const uint8_t *raw = groupRecord(m, g);
		const auto &group  = m.mesh_groups[g];
		const int id	   = int(g);
		for(uint32_t lod = 0; lod < NODParser::MAX_LODS; ++lod) indexSum += u16(raw + 4 + lod * 6) + u16(raw + 6 + lod * 6);
		const uint16_t groupVerts = nodGroupVertexCount(m, g);
		advance[group.vertex_set_index] += groupVerts;
		if(group.shader_index >= m.shader_count) faults.push_back({id, "material " + std::to_string(group.shader_index)});
		for(uint32_t p = 0; p < group.num_bones; ++p)
			if(group.a_bones[p] >= m.bone_count) faults.push_back({id, "palette entry " + std::to_string(group.a_bones[p])});
		const bool empty = nodGroupEmpty(m, g);
		// a group with one bone draws rigid vertices; more bones, skinned ones
		const uint32_t type = m.vertex_sets[group.vertex_set_index].type;
		if(!empty && (group.num_bones <= 1) != (type % 3 == 0))
			faults.push_back({id, std::to_string(group.num_bones) + " bones on vertex type " + std::to_string(type)});
		// detail level 0: every index names a vertex of this group
		const auto &lod	   = group.lods[0];
		const uint32_t end = lod.list_start + lod.list_count;
		uint32_t highest   = 0;
		for(uint32_t i = lod.strip_start; i < end && i < m.index_count; ++i) highest = std::max<uint32_t>(highest, m.indices[i]);
		if(end > lod.strip_start && highest >= groupVerts)
			faults.push_back({id, "index " + std::to_string(highest) + " of " + std::to_string(groupVerts) + " vertices"});
	}
	if(indexSum != m.index_count)
		faults.push_back({-1, "groups hold " + std::to_string(indexSum) + " of " + std::to_string(m.index_count) + " indices"});
	for(uint32_t s = 0; s < m.vertex_set_count; ++s)
		if(advance[s] != m.vertex_sets[s].count)
			faults.push_back({-1, "groups hold " + std::to_string(advance[s]) + " of " + std::to_string(m.vertex_sets[s].count) +
									  " vertices of set " + std::to_string(s)});
	return faults;
}
