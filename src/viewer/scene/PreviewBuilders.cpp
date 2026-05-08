/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
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

#include "scene/PreviewBuilders.h"
#include "scene/MaterialLibrary.h"
#include "scene/DdsCatalog.h"
#include "scene/EntityInstancer.h"
#include "core/Strings.h"
#include "render/PreviewRenderer.h"

#include "parsers/NODParser.h"
#include "parsers/NMBParser.h"
#include "parsers/NILParser.h"
#include "parsers/NOCParser.h"

#include <cstdio>
#include <cstring>
#include <vector>

/*
 */
namespace viewer::scene {

/*
 */
void buildNodPreview(NODParser *nod, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog) {
	if(!nod) return;
	const auto &md = nod->getModelData();

	std::vector<GLuint> groupTex;
	materials.loadMaterialsForModel(md.shader_names, md.shader_count, md.mesh_groups, md.mesh_group_count, ddsCatalog, groupTex);

	viewer::render::PreviewRenderer::MeshBuildInfo info{};
	info.vertex_data = md.vertex_data;
	info.vertex_data_size = md.vertex_data_size;
	info.vertex_set_count = md.vertex_set_count;
	info.vertex_sets = md.vertex_sets;
	info.indices = md.indices;
	info.index_count = md.index_count;
	info.mesh_groups = md.mesh_groups;
	info.mesh_group_count = md.mesh_group_count;
	info.bounds_min = md.header.bounds_min;
	info.bounds_max = md.header.bounds_max;
	info.group_textures = groupTex.data();
	info.group_texture_count = static_cast<uint32_t>(groupTex.size());
	info.bones = md.bones;
	info.bone_count = md.bone_count;
	renderer.buildMesh(info);
}

/*
 */
void buildNmbPreview(NMBParser *nmb, int modelIndex, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog) {
	if(!nmb) return;
	const auto &data = nmb->getBundleData();
	if(modelIndex < 0 || static_cast<uint32_t>(modelIndex) >= data.model_count) return;

	const auto &entry = data.models[modelIndex];
	if(!entry.vertex_data || !entry.indices || entry.index_count == 0) return;

	std::vector<GLuint> groupTex;
	materials.loadMaterialsForModel(entry.shader_names, entry.shader_count, entry.mesh_groups, entry.mesh_group_count, ddsCatalog, groupTex);

	viewer::render::PreviewRenderer::MeshBuildInfo info{};
	info.vertex_data = entry.vertex_data;
	info.vertex_data_size = entry.vertex_data_size;
	info.vertex_set_count = entry.vertex_set_count;
	info.vertex_sets = entry.vertex_sets;
	info.indices = entry.indices;
	info.index_count = entry.index_count;
	info.mesh_groups = entry.mesh_groups;
	info.mesh_group_count = entry.mesh_group_count;
	info.bounds_min = glm::vec3(entry.bounds_min[0], entry.bounds_min[1], entry.bounds_min[2]);
	info.bounds_max = glm::vec3(entry.bounds_max[0], entry.bounds_max[1], entry.bounds_max[2]);
	info.group_textures = groupTex.data();
	info.group_texture_count = static_cast<uint32_t>(groupTex.size());
	info.bones = entry.bones;
	info.bone_count = entry.bone_count;
	renderer.buildMesh(info);
}

/*
 */
void buildNilPreview(NILParser *nil, const std::string &nilFullPath, const std::string &assetDir, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog) {
	if(!nil) return;
	const auto &lev = nil->getLevelData();

	materials.buildMaterialDB();

	// resolve one texture per surface group, in the same iteration order
	// as PreviewRenderer::buildMeshNIL so that group_idx values line up.
	std::vector<GLuint> groupTextures;
	for(uint32_t si = 0; si < lev.num_sectors && lev.sectors; ++si) {
		const auto &s = lev.sectors[si];
		if(!s.vertex_data || s.num_verts == 0) continue;
		if(!s.indices || s.num_indices == 0) continue;

		for(uint16_t gi = 0; gi < s.num_groups && s.groups; ++gi) {
			const auto &g = s.groups[gi];
			GLuint tid = 0;
			if(g.shader_idx < lev.num_shaders) {
				const std::string shaderName(lev.shader_names[g.shader_idx]);
				if(!shaderName.empty()) {
					tid = materials.resolveTexture(shaderName, ddsCatalog);
					if(!tid) {
						std::printf("[NIL] sector=%u group=%u shader='%s' -> UNRESOLVED\n", si, gi, shaderName.c_str());
					}
				}
			}
			groupTextures.push_back(tid);
		}
	}
	std::printf("[PreviewBuilders] NIL: resolved %zu group textures\n", groupTextures.size());

	renderer.buildMeshNIL(nil, groupTextures.data(), static_cast<uint32_t>(groupTextures.size()));

	// phase 2: bake entity instances from sibling NSD into entity mesh.
	bakeNilEntities(nilFullPath, assetDir, renderer, materials, ddsCatalog);
}

/*
 */
void buildNocPreview(NOCParser *noc, viewer::render::PreviewRenderer &renderer) {
	if(!noc) return;
	const auto &data = noc->getCollisionData();
	if(!data.vertex_data || data.num_vertices == 0 ||
	   !data.indices || data.num_indices == 0) return;

	viewer::render::PreviewRenderer::SimpleMeshBuildInfo info{};
	info.vertex_data = data.vertex_data;
	info.vertex_count = data.num_vertices;
	info.vertex_stride = NOCParser::VERTEX_STRIDE;
	info.pos_offset = 0;
	info.normal_offset = 12;
	info.indices = data.indices;
	info.index_count = data.num_indices;
	info.is_strip = true;
	info.bounds_min = data.bounds_min;
	info.bounds_max = data.bounds_max;
	renderer.buildMeshSimple(info);
}

} // namespace viewer::scene
