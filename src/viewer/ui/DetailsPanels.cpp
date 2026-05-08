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

#include "ui/DetailsPanels.h"
#include "ui/Inspectors.h"

#include "core/AssetDatabase.h"
#include "animation/AnimationController.h"

#include "parsers/NODParser.h"
#include "parsers/NMBParser.h"
#include "parsers/NILParser.h"
#include "parsers/NOCParser.h"

#include <imgui.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

/*
 */
namespace viewer::ui {

/*
 */
namespace {

// per-frame static buffer for the NAD filter input.
char gAnimFilter[64] = {};

// shared by NOD and NMB details panels. Lists every NAD asset filtered by
// substring; click loads, then play/pause/stop + frame slider + fps input.
/*
 */
void drawNadPicker(InspectorContext &ctx, const char *uniqueId) {
	if(!ctx.db || !ctx.animation || !ctx.renderer) return;

	auto &anim = *ctx.animation;
	const auto &assets = ctx.db->entries();

	char headerLabel[64];
	std::snprintf(headerLabel, sizeof(headerLabel), "Animations##%s", uniqueId);
	if(!ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen)) return;

	char filterLabel[64];
	std::snprintf(filterLabel, sizeof(filterLabel), "Filter##%s_anim_filter", uniqueId);
	ImGui::InputText(filterLabel, gAnimFilter, sizeof(gAnimFilter));

	char listLabel[64];
	std::snprintf(listLabel, sizeof(listLabel), "%s_nad_list", uniqueId);
	ImGui::BeginChild(listLabel, ImVec2(0, 120), true);

	const int activeIdx = anim.assetIndex();
	for(int i = 0; i < static_cast<int>(assets.size()); ++i) {
		const auto &ae = assets[i];
		const std::size_t dot = ae.relativePath.rfind('.');
		if(dot == std::string::npos) continue;
		std::string ext = ae.relativePath.substr(dot + 1);
		for(auto &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if(ext != "nad") continue;
		if(gAnimFilter[0] != '\0' && ae.relativePath.find(gAnimFilter) == std::string::npos) continue;

		const bool sel = (i == activeIdx);
		if(ImGui::Selectable(ae.relativePath.c_str(), sel)) {
			anim.loadAnimation(i, *ctx.db);
		}
	}
	ImGui::EndChild();

	if(anim.isLoaded()) {
		const auto &clip = anim.clip()->getAnimationClip();
		ImGui::Text("Duration: %.1f frames", clip.duration);

		char pauseLabel[32];
		std::snprintf(pauseLabel, sizeof(pauseLabel), anim.playing() ? "Pause##%s" : "Play##%s", uniqueId);
		if(ImGui::Button(pauseLabel)) {
			anim.playing() = !anim.playing();
		}
		ImGui::SameLine();
		char stopLabel[32];
		std::snprintf(stopLabel, sizeof(stopLabel), "Stop##%s", uniqueId);
		if(ImGui::Button(stopLabel)) {
			anim.playing() = false;
			anim.frame() = 0.0f;
		}

		const float maxFrame = clip.duration > 0.0f ? clip.duration : 1.0f;
		char frameLabel[32];
		std::snprintf(frameLabel, sizeof(frameLabel), "Frame##%s", uniqueId);
		ImGui::SliderFloat(frameLabel, &anim.frame(), 0.0f, maxFrame);

		char fpsLabel[32];
		std::snprintf(fpsLabel, sizeof(fpsLabel), "FPS##%s", uniqueId);
		ImGui::InputFloat(fpsLabel, &anim.fps(), 1.0f, 5.0f, "%.1f");
		if(anim.fps() < 1.0f) anim.fps() = 1.0f;
	}
}

/*
 */
void detailsNod(IAssetParser *p, InspectorContext &ctx) {
	auto *nod = static_cast<NODParser *>(p);
	const auto &md = nod->getModelData();

	drawNadPicker(ctx, "nod");
	ImGui::Separator();

	if(md.vertex_set_count > 0 && ImGui::TreeNode("Vertex Sets")) {
		for(uint32_t i = 0; i < md.vertex_set_count; ++i) {
			const auto &vs = md.vertex_sets[i];
			ImGui::BulletText("Set %u: type=%u count=%u stride=%u", i, vs.type, vs.count, vs.stride);
		}
		ImGui::TreePop();
	}

	if(md.shader_count > 0 && ImGui::TreeNode("Shaders")) {
		for(uint32_t i = 0; i < md.shader_count; ++i)
			if(md.shader_names[i][0] != '\0') {
				ImGui::BulletText("[%u] %s", i, md.shader_names[i]);
			}
		ImGui::TreePop();
	}

	if(md.bone_count > 0 && ImGui::TreeNode("Bones")) {
		if(ImGui::BeginTable("bones", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn("Index");
			ImGui::TableSetupColumn("Parent");
			ImGui::TableSetupColumn("Joint ID");
			ImGui::TableSetupColumn("Offset");
			ImGui::TableHeadersRow();
			for(uint32_t i = 0; i < md.bone_count; ++i) {
				const auto &b = md.bones[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%u", i);
				ImGui::TableNextColumn(); ImGui::Text("%u", b.parent);
				ImGui::TableNextColumn(); ImGui::Text("%u", b.joint_id);
				ImGui::TableNextColumn(); ImGui::Text("(%.1f, %.1f, %.1f)", b.offset[0], b.offset[1], b.offset[2]);
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}

	if(md.mesh_group_count > 0 && ImGui::TreeNode("Mesh Groups")) {
		for(uint32_t i = 0; i < md.mesh_group_count; ++i) {
			const auto &g = md.mesh_groups[i];
			char label[64];
			std::snprintf(label, sizeof(label), "Group %u (shader %u)", i, g.shader_index);
			if(ImGui::TreeNode(label)) {
				ImGui::Text("Shader Index: %u", g.shader_index);
				ImGui::Text("Vertex Set: %u  |  Base Vertex: %u", g.vertex_set_index, g.base_vertex);
				ImGui::Text("Bones: %u", g.num_bones);
				for(uint32_t l = 0; l < NODParser::MAX_LODS; ++l) {
					const auto &lod = g.lods[l];
					if(lod.strip_count > 0 || lod.list_count > 0)
						ImGui::BulletText("LOD %u: strip=%u list=%u verts=%u", l, lod.strip_count, lod.list_count, lod.num_verts);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

/*
 */
void detailsNmb(IAssetParser *p, InspectorContext &ctx) {
	auto *nmb = static_cast<NMBParser *>(p);
	const auto &data = nmb->getBundleData();

	drawNadPicker(ctx, "nmb");
	ImGui::Separator();

	for(uint32_t i = 0; i < data.model_count && i < NMBParser::MAX_MODELS; ++i) {
		const auto &m = data.models[i];
		if(m.name[0] == '\0') continue;
		char label[128];
		std::snprintf(label, sizeof(label), "Model %u: %s", i, m.name);
		if(ImGui::TreeNode(label)) {
			ImGui::Text("Shaders: %u | Bones: %u | Mesh Groups: %u | VS: %u", m.shader_count, m.bone_count, m.mesh_group_count, m.vertex_set_count);
			ImGui::Text("Bounds Min: (%.1f, %.1f, %.1f)", m.bounds_min[0], m.bounds_min[1], m.bounds_min[2]);
			ImGui::Text("Bounds Max: (%.1f, %.1f, %.1f)", m.bounds_max[0], m.bounds_max[1], m.bounds_max[2]);
			for(int s = 0; s < NMBParser::MAX_SHADERS_PER_MODEL; ++s)
				if(m.shader_names[s][0] != '\0') {
					ImGui::BulletText("Shader [%d]: %s", s, m.shader_names[s]);
				}
			ImGui::TreePop();
		}
	}
}

/*
 */
void detailsNil(IAssetParser *p, InspectorContext &) {
	auto *nil = static_cast<NILParser *>(p);
	const auto &lev = nil->getLevelData();

	if(ImGui::BeginTable("nil_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Version");
		ImGui::TableNextColumn(); ImGui::Text("%u", lev.version);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Sectors");
		ImGui::TableNextColumn(); ImGui::Text("%u", lev.num_sectors);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Total Verts");
		ImGui::TableNextColumn(); ImGui::Text("%u", lev.total_verts);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Total Indices");
		ImGui::TableNextColumn(); ImGui::Text("%u", lev.total_indices);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Total Groups");
		ImGui::TableNextColumn(); ImGui::Text("%u", lev.total_groups);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Shaders");
		ImGui::TableNextColumn(); ImGui::Text("%u", lev.num_shaders);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Ambient");
		ImGui::TableNextColumn();
		ImGui::Text("(%.2f, %.2f, %.2f)", lev.ambient_color[0], lev.ambient_color[1], lev.ambient_color[2]);
		ImGui::EndTable();
	}

	if(lev.num_shaders > 0 && ImGui::TreeNode("Shaders")) {
		for(uint32_t i = 0; i < lev.num_shaders && i < NILParser::MAX_SHADERS; ++i)
			if(lev.shader_names[i][0] != '\0') {
				ImGui::BulletText("[%u] %s", i, lev.shader_names[i]);
			}
		ImGui::TreePop();
	}

	if(lev.num_env_maps > 0 && ImGui::TreeNode("Environment Maps")) {
		for(uint16_t i = 0; i < lev.num_env_maps && i < 64; ++i)
			if(lev.env_map_names[i][0] != '\0') {
				ImGui::BulletText("[%u] %s", i, lev.env_map_names[i]);
			}
		ImGui::TreePop();
	}

	if(lev.num_sectors > 0 && ImGui::TreeNode("Sectors")) {
		for(uint32_t i = 0; i < lev.num_sectors && i < 128; ++i) {
			const auto &s = lev.sectors[i];
			char label[64];
			std::snprintf(label, sizeof(label), "Sector %u", i);
			if(ImGui::TreeNode(label)) {
				ImGui::Text("Flags: 0x%08X", s.flags);
				ImGui::Text("Center: (%.1f, %.1f, %.1f)", s.center[0], s.center[1], s.center[2]);
				ImGui::Text("Portals: %u | Groups: %u | Verts: %u", s.num_portals, s.num_groups, s.num_verts);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

/*
 */
void detailsNoc(IAssetParser *p, InspectorContext &) {
	auto *noc = static_cast<NOCParser *>(p);
	const auto &data = noc->getCollisionData();

	ImGui::Text("Shapes: %u | Vertices: %u | Indices: %u", data.shape_count, data.num_vertices, data.num_indices);
	ImGui::Text("Bounds: (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f)", data.bounds_min.x, data.bounds_min.y, data.bounds_min.z, data.bounds_max.x, data.bounds_max.y, data.bounds_max.z);

	if(data.shape_count > 0 &&
	   ImGui::BeginTable("noc_shapes", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Collision Type");
		ImGui::TableSetupColumn("Position");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.shape_count; ++i) {
			const auto &s = data.shapes[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", s.name);
			ImGui::TableNextColumn(); ImGui::Text("%s", s.collision_type);
			ImGui::TableNextColumn(); ImGui::Text("(%.1f, %.1f, %.1f)", s.position[0], s.position[1], s.position[2]);
		}
		ImGui::EndTable();
	}
}

} // namespace

/*
 */
bool has3dPreview(IAssetParser *parser) noexcept {
	if(!parser) return false;
	const char *t = parser->getFileType();
	return std::strcmp(t, "NOD") == 0 ||
	       std::strcmp(t, "NMB") == 0 ||
	       std::strcmp(t, "NIL") == 0 ||
	       std::strcmp(t, "NOC") == 0;
}

/*
 */
void drawDetailsPane(IAssetParser *parser, InspectorContext &ctx) {
	if(!parser) return;
	const char *t = parser->getFileType();
	if     (std::strcmp(t, "NOD") == 0) detailsNod(parser, ctx);
	else if(std::strcmp(t, "NMB") == 0) detailsNmb(parser, ctx);
	else if(std::strcmp(t, "NIL") == 0) detailsNil(parser, ctx);
	else if(std::strcmp(t, "NOC") == 0) detailsNoc(parser, ctx);
}

} // namespace viewer::ui
