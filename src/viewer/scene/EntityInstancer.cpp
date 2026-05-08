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

#include "scene/EntityInstancer.h"
#include "scene/MaterialLibrary.h"
#include "scene/DdsCatalog.h"
#include "core/Strings.h"
#include "render/PreviewRenderer.h"

#include "parsers/NMBParser.h"
#include "parsers/NODParser.h"
#include "parsers/NSDParser.h"
#include "parsers/NSDEntityResolver.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unordered_map>
#include <vector>

/*
 */
namespace viewer::scene {

/*
 */
namespace {

/*
 */
struct InstanceDesc {
	const char *modelName; // points into NSD entity table
	float pos[3];
	float rot[3];
	bool  isOffset;
};

/*
 */
struct LoadedModel {
	std::string name_lower;
	NODParser   nod;
	NMBParser   nmb;
	bool        isNmb = false;
	std::vector<GLuint>              groupTextures;     // NOD
	std::vector<std::vector<GLuint>> nmbGroupTextures;  // NMB, one per submodel
	bool loaded = false;
};

/*
 */
void buildModelsMap(const std::string &assetDir, std::unordered_map<std::string, std::string> &out) {
	const std::string modelsDir = assetDir + "/3D/Models";
	DIR *d = opendir(modelsDir.c_str());
	if(!d) return;
	while(auto *de = readdir(d)) {
		std::string fn = de->d_name;
		if(fn.empty() || fn[0] == '.') continue;
		out[viewer::core::toLower(fn)] = modelsDir + "/" + fn;
	}
	closedir(d);
}

} // namespace

/*
 */
void bakeNilEntities(const std::string &nilPath, const std::string &assetDir, viewer::render::PreviewRenderer &renderer, MaterialLibrary &materials, DdsCatalog &ddsCatalog) {

	// nil -> nsd
	std::string nsdPath = nilPath;
	{
		const std::size_t len = nsdPath.size();
		if(len > 4 && viewer::core::toLower(nsdPath.substr(len - 4)) == ".nil") {
			nsdPath[len - 3] = 'n'; nsdPath[len - 2] = 's'; nsdPath[len - 1] = 'd';
		} else { return; }
	}

	NSDParser nsdFull;
	if(!nsdFull.parse(nsdPath.c_str())) return;
	const auto &sd = nsdFull.getSceneData();
	if(!sd.entities || sd.entity_array_count == 0) return;

	std::printf("[EntityInstancer] NSD: %u entity defs from %s\n", sd.entity_array_count, nsdPath.substr(nsdPath.rfind('/') + 1).c_str());

	std::vector<InstanceDesc> instances;
	for(uint32_t i = 0; i < sd.entity_array_count; ++i) {
		const auto &se = sd.entities[i];
		if(se.type_name[0] == '\0') continue;
		if(NSDEntityResolver::isRegionType(se.type_name) ||
		   NSDEntityResolver::isSkipType(se.type_name)) continue;

		const char *resolved = NSDEntityResolver::resolveModelFile(se.type_name);
		const char *cand = nullptr;
		if(resolved && resolved[0] != '\0')      cand = resolved;
		else if(se.has_model && se.model_name[0]) cand = se.model_name;
		else { cand = se.type_name; }

		InstanceDesc d{};
		d.modelName = cand;
		d.pos[0] = se.position[0]; d.pos[1] = se.position[1]; d.pos[2] = se.position[2];
		d.rot[0] = se.rotation[0]; d.rot[1] = se.rotation[1]; d.rot[2] = se.rotation[2];
		d.isOffset = (std::strstr(se.type_name, "_offset") != nullptr);
		instances.push_back(d);
	}

	std::printf("[EntityInstancer] %zu instances to render\n", instances.size());

	// unique model names (case-insensitive).
	std::vector<std::string> uniqueNames;
	for(const auto &inst : instances) {
		std::string mn = viewer::core::toLower(inst.modelName);
		bool found = false;
		for(const auto &u : uniqueNames) if(u == mn) { found = true; break; }
		if(!found) uniqueNames.push_back(mn);
	}

	std::unordered_map<std::string, std::string> modelsMap;
	buildModelsMap(assetDir, modelsMap);

	std::vector<LoadedModel> loaded(uniqueNames.size());
	for(std::size_t u = 0; u < uniqueNames.size(); ++u) {
		loaded[u].name_lower = uniqueNames[u];

		auto it = modelsMap.find(uniqueNames[u]);
		if(it == modelsMap.end()) it = modelsMap.find(uniqueNames[u] + ".nod");
		if(it == modelsMap.end()) it = modelsMap.find(uniqueNames[u] + ".nmb");
		if(it == modelsMap.end()) {
			std::string base = uniqueNames[u];
			if(base.size() > 4) {
				const std::string ext4 = viewer::core::toLower(base.substr(base.size() - 4));
				if(ext4 == ".nod" || ext4 == ".nmb") {
					base = base.substr(0, base.size() - 4);
					it = modelsMap.find(base + ".nod");
					if(it == modelsMap.end()) it = modelsMap.find(base + ".nmb");
				}
			}
		}
		if(it == modelsMap.end()) {
			std::printf("[EntityInstancer] model not found: %s\n", uniqueNames[u].c_str());
			continue;
		}

		const std::string &fpath = it->second;
		const bool useNmb = fpath.size() >= 4 &&
		                    viewer::core::toLower(fpath.substr(fpath.size() - 4)) == ".nmb";
		if(useNmb) {
			loaded[u].isNmb = true;
			if(!loaded[u].nmb.parse(fpath.c_str())) {
				std::printf("[EntityInstancer] NMB parse failed: %s\n", uniqueNames[u].c_str());
				continue;
			}
			const auto &bd = loaded[u].nmb.getBundleData();
			loaded[u].nmbGroupTextures.resize(bd.model_count);
			for(uint32_t mi = 0; mi < bd.model_count && mi < NMBParser::MAX_MODELS; ++mi) {
				const auto &me = bd.models[mi];
				materials.loadMaterialsForModel(me.shader_names, me.shader_count, me.mesh_groups, me.mesh_group_count, ddsCatalog, loaded[u].nmbGroupTextures[mi]);
			}
		} else {
			if(!loaded[u].nod.parse(fpath.c_str())) {
				std::printf("[EntityInstancer] NOD parse failed: %s\n", uniqueNames[u].c_str());
				continue;
			}
			const auto &md = loaded[u].nod.getModelData();
			materials.loadMaterialsForModel(md.shader_names, md.shader_count, md.mesh_groups, md.mesh_group_count, ddsCatalog, loaded[u].groupTextures);
		}
		loaded[u].loaded = true;
	}

	// combined buffers — layout: pos(3) norm(3) uv(2) vcol(3) = 11f
	constexpr uint32_t NFLOAT = 11;
	std::vector<float>    evbuf;
	std::vector<uint32_t> eibuf;
	std::vector<viewer::render::PreviewRenderer::DrawGroup> egroups;

	const glm::quat coordQ = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	static const std::vector<GLuint> emptyGtex;

	auto renderNodBody = [&](
		const uint8_t *vertex_data, uint32_t vertex_set_count,
		const NODParser::VertexSet *vertex_sets,
		const uint16_t *indices, uint32_t index_count,
		uint32_t mesh_group_count, const NODParser::MeshGroup *mesh_groups,
		const std::vector<GLuint> &group_tex,
		const glm::mat4 &model_mat, const glm::mat3 &rot3
	) {
		for(uint32_t gi = 0; gi < mesh_group_count; ++gi) {
			const auto &mg = mesh_groups[gi];
			const auto &lod = mg.lods[0];
			const uint8_t vsi = mg.vertex_set_index;
			if(vsi >= vertex_set_count) continue;
			const auto &vs = vertex_sets[vsi];
			if(vs.count == 0 || vs.base_stride < 32) continue;

			const uint32_t num_v = vs.count;
			const bool has_strip = (lod.strip_count >= 3 &&
			                        lod.strip_start + (uint32_t)lod.strip_count <= index_count);
			const bool has_list = (lod.list_count  >= 3 &&
			                        lod.list_start  + (uint32_t)lod.list_count  <= index_count);
			if(!has_strip && !has_list) continue;

			const uint32_t group_vert_base = static_cast<uint32_t>(evbuf.size() / NFLOAT);
			const uint8_t *blob = vertex_data + vs.data_offset;

			for(uint32_t vi = 0; vi < num_v; ++vi) {
				const uint8_t *vp = blob + vi * vs.base_stride;
				float px, py, pz, nx, ny, nz, u, v;
				std::memcpy(&px, vp+0,4);  std::memcpy(&py, vp+4,4);  std::memcpy(&pz, vp+8,4);
				std::memcpy(&nx, vp+12,4); std::memcpy(&ny, vp+16,4); std::memcpy(&nz, vp+20,4);
				std::memcpy(&u,  vp+24,4); std::memcpy(&v,  vp+28,4);
				const glm::vec3 wp = glm::vec3(model_mat * glm::vec4(px, py, pz, 1.0f));
				const glm::vec3 wn = glm::normalize(rot3 * glm::vec3(nx, ny, nz));
				evbuf.push_back(wp.x); evbuf.push_back(wp.y); evbuf.push_back(wp.z);
				evbuf.push_back(wn.x); evbuf.push_back(wn.y); evbuf.push_back(wn.z);
				evbuf.push_back(u);    evbuf.push_back(v);
				evbuf.push_back(1.0f); evbuf.push_back(1.0f); evbuf.push_back(1.0f);
			}

			const uint32_t ibuf_start = static_cast<uint32_t>(eibuf.size());

			if(has_strip) {
				for(uint32_t n = 0; n + 2 < (uint32_t)lod.strip_count; ++n) {
					uint32_t ra = (uint32_t)indices[lod.strip_start + n]     + mg.base_vertex;
					uint32_t rb = (uint32_t)indices[lod.strip_start + n + 1] + mg.base_vertex;
					uint32_t rc = (uint32_t)indices[lod.strip_start + n + 2] + mg.base_vertex;
					if(ra >= num_v || rb >= num_v || rc >= num_v) continue;
					if(ra == rb || rb == rc || ra == rc) continue;
					if(n & 1) { eibuf.push_back(group_vert_base+rb); eibuf.push_back(group_vert_base+ra); eibuf.push_back(group_vert_base+rc); }
					else      { eibuf.push_back(group_vert_base+ra); eibuf.push_back(group_vert_base+rb); eibuf.push_back(group_vert_base+rc); }
				}
			}
			if(has_list) {
				for(uint32_t n = 0; n + 2 < (uint32_t)lod.list_count; n += 3) {
					uint32_t ra = (uint32_t)indices[lod.list_start + n]     + mg.base_vertex;
					uint32_t rb = (uint32_t)indices[lod.list_start + n + 1] + mg.base_vertex;
					uint32_t rc = (uint32_t)indices[lod.list_start + n + 2] + mg.base_vertex;
					if(ra >= num_v) ra = 0; if(rb >= num_v) rb = 0; if(rc >= num_v) rc = 0;
					eibuf.push_back(group_vert_base+ra); eibuf.push_back(group_vert_base+rb); eibuf.push_back(group_vert_base+rc);
				}
			}

			const uint32_t ibuf_end = static_cast<uint32_t>(eibuf.size());
			if(ibuf_end == ibuf_start) continue;

			const GLuint tid = (gi < group_tex.size()) ? group_tex[gi] : 0;
			viewer::render::PreviewRenderer::DrawGroup dg{};
			dg.index_offset = static_cast<GLsizei>(ibuf_start * sizeof(uint32_t));
			dg.index_count = static_cast<GLsizei>(ibuf_end - ibuf_start);
			dg.diffuse_tex = tid;
			dg.use_vcol = false;
			egroups.push_back(dg);
		}
	};

	uint32_t entLoaded = 0;
	for(const auto &ref : instances) {
		const std::string mn = viewer::core::toLower(ref.modelName);
		LoadedModel *lm = nullptr;
		for(auto &m : loaded) if(m.name_lower == mn && m.loaded) { lm = &m; break; }
		if(!lm) continue;

		const glm::vec3 gl_pos(ref.pos[0], ref.pos[2], -ref.pos[1]);
		const glm::quat qYaw = glm::angleAxis(glm::radians(ref.rot[1]), glm::vec3(0, 1, 0));
		const glm::quat qPitch = glm::angleAxis(glm::radians(ref.rot[0]), glm::vec3(1, 0, 0));
		const glm::quat qRoll = glm::angleAxis(glm::radians(ref.rot[2]), glm::vec3(0, 0, 1));
		const glm::quat gl_rot = coordQ * qYaw * qPitch * qRoll;
		const glm::mat3 rot3 = glm::mat3(glm::mat4_cast(gl_rot));

		glm::mat4 model_mat;
		if(ref.isOffset) {
			glm::vec3 vmin(1e30f), vmax(-1e30f);
			auto scanVerts = [&](const uint8_t *vdata, uint32_t vsc, const NODParser::VertexSet *vsets) {
				for(uint32_t s = 0; s < vsc; ++s) {
					const auto &vs = vsets[s];
					if(vs.count == 0 || vs.base_stride < 12) continue;
					const uint8_t *blob = vdata + vs.data_offset;
					for(uint32_t vi = 0; vi < vs.count; ++vi) {
						float px, py, pz;
						std::memcpy(&px, blob + vi*vs.base_stride + 0, 4);
						std::memcpy(&py, blob + vi*vs.base_stride + 4, 4);
						std::memcpy(&pz, blob + vi*vs.base_stride + 8, 4);
						vmin = glm::min(vmin, glm::vec3(px, py, pz));
						vmax = glm::max(vmax, glm::vec3(px, py, pz));
					}
				}
			};
			if(lm->isNmb) {
				const auto &bd = lm->nmb.getBundleData();
				for(uint32_t mi = 0; mi < bd.model_count && mi < NMBParser::MAX_MODELS; ++mi) {
					const auto &entry = bd.models[mi];
					if(entry.vertex_data && entry.vertex_set_count > 0) {
						scanVerts(entry.vertex_data, entry.vertex_set_count, entry.vertex_sets);
					}
				}
			} else {
				const auto &md = lm->nod.getModelData();
				if(md.vertex_data) scanVerts(md.vertex_data, md.vertex_set_count, md.vertex_sets);
			}
			glm::vec3 d3dCenter = (vmin + vmax) * 0.5f;
			d3dCenter.z = 0.0f;
			model_mat = glm::translate(glm::mat4(1.0f), gl_pos)
			          * glm::mat4_cast(gl_rot)
			          * glm::translate(glm::mat4(1.0f), -d3dCenter);
		} else {
			model_mat = glm::translate(glm::mat4(1.0f), gl_pos) * glm::mat4_cast(gl_rot);
		}

		if(lm->isNmb) {
			const auto &bd = lm->nmb.getBundleData();
			for(uint32_t mi = 0; mi < bd.model_count && mi < NMBParser::MAX_MODELS; ++mi) {
				const auto &entry = bd.models[mi];
				if(!entry.vertex_data || entry.vertex_set_count == 0 || entry.mesh_group_count == 0) continue;
				const auto &gtex = (mi < lm->nmbGroupTextures.size())
				                       ? lm->nmbGroupTextures[mi] : emptyGtex;
				renderNodBody(entry.vertex_data, entry.vertex_set_count, entry.vertex_sets, entry.indices, entry.index_count, entry.mesh_group_count, entry.mesh_groups, gtex, model_mat, rot3);
			}
		} else {
			const auto &md = lm->nod.getModelData();
			if(!md.vertex_data || md.vertex_set_count == 0 || md.mesh_group_count == 0) continue;
			renderNodBody(md.vertex_data, md.vertex_set_count, md.vertex_sets, md.indices, md.index_count, md.mesh_group_count, md.mesh_groups, lm->groupTextures, model_mat, rot3);
		}
		++entLoaded;
	}

	if(!eibuf.empty() && !egroups.empty()) {
		renderer.buildEntityGeometry(evbuf.data(), static_cast<uint32_t>(evbuf.size() / NFLOAT), eibuf.data(), static_cast<uint32_t>(eibuf.size()), egroups.data(), static_cast<uint32_t>(egroups.size()));
		std::printf("[EntityInstancer] %u/%zu instances rendered, %u draw groups\n", entLoaded, instances.size(), static_cast<uint32_t>(egroups.size()));
	}
}

} // namespace viewer::scene
