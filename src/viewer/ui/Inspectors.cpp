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

#include "ui/Inspectors.h"

#include "core/AssetDatabase.h"
#include "render/PreviewRenderer.h"
#include "scene/PreviewBuilders.h"
#include "AudioPlayer.h"

#include "parsers/AssetParser.h"
#include "parsers/AssetParserFactory.h"
#include "parsers/DDSParser.h"
#include "parsers/TGAParser.h"
#include "parsers/XPRParser.h"
#include "parsers/NODParser.h"
#include "parsers/NMBParser.h"
#include "parsers/NADParser.h"
#include "parsers/NMSParser.h"
#include "parsers/NILParser.h"
#include "parsers/NSDParser.h"
#include "parsers/NOTParser.h"
#include "parsers/NSAParser.h"
#include "parsers/NFXParser.h"
#include "parsers/NUIParser.h"
#include "parsers/NLTParser.h"
#include "parsers/NLSParser.h"
#include "parsers/NOCParser.h"
#include "parsers/NPDParser.h"
#include "parsers/NAKParser.h"
#include "parsers/NCEParser.h"
#include "parsers/NCSParser.h"
#include "parsers/NCOParser.h"
#include "parsers/NNBParser.h"
#include "parsers/TerrainParser.h"
#include "parsers/NLFXParser.h"
#include "parsers/XWBParser.h"
#include "parsers/XSBParser.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

/*
 */
namespace viewer::ui {

/*
 */
namespace {

/*
 */
void inspectDDS(IAssetParser *p, InspectorContext &ctx) {
	auto *dds = static_cast<DDSParser *>(p);
	const auto &tex = dds->getTextureData();
	const auto &hdr = tex.header;

	ImGui::Text("DDS Texture");
	ImGui::Separator();

	if(ImGui::BeginTable("dds_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Width");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.width);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Height");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.height);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Depth");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.depth);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Mipmaps");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.mip_map_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("FourCC");
		ImGui::TableNextColumn();
		{
			char fourcc[5] = {};
			std::memcpy(fourcc, &hdr.four_cc, 4);
			ImGui::Text("0x%08X (%s)", hdr.four_cc, fourcc);
		}
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("RGB Bits");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.rgb_bit_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Pixel Data Size");
		ImGui::TableNextColumn(); ImGui::Text("%zu bytes", tex.pixel_data_size);
		ImGui::EndTable();
	}

	ImGui::Separator();
	if(ctx.texCache && ctx.texCache->find(ctx.selectedAssetIdx) == ctx.texCache->end()) {
		const GLuint tid = viewer::render::PreviewRenderer::uploadDDS(dds);
		if(tid) {
			TexturePreviewEntry tp{tid, static_cast<int>(hdr.width), static_cast<int>(hdr.height)};
			(*ctx.texCache)[ctx.selectedAssetIdx] = tp;
		}
	}
	if(ctx.texCache) {
		const auto cit = ctx.texCache->find(ctx.selectedAssetIdx);
		if(cit != ctx.texCache->end() && cit->second.gl_texture && ctx.renderer) {
			ctx.renderer->drawTexturePreview(cit->second.gl_texture, cit->second.width, cit->second.height);
		}
	}
}

/*
 */
void inspectTGA(IAssetParser *p, InspectorContext &ctx) {
	auto *tga = static_cast<TGAParser *>(p);
	const auto &img = tga->getImageData();
	const auto &hdr = img.header;

	ImGui::Text("TGA Image");
	ImGui::Separator();

	if(ImGui::BeginTable("tga_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Width");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.width);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Height");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.height);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Bit Depth");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.bit_depth);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Image Type");
		ImGui::TableNextColumn(); ImGui::Text("%u", hdr.image_type);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Pixel Data Size");
		ImGui::TableNextColumn(); ImGui::Text("%zu bytes", img.pixel_data_size);
		ImGui::EndTable();
	}

	ImGui::Separator();
	if(ctx.texCache && ctx.texCache->find(ctx.selectedAssetIdx) == ctx.texCache->end()) {
		const GLuint tid = viewer::render::PreviewRenderer::uploadTGA(tga);
		if(tid) {
			TexturePreviewEntry tp{tid, static_cast<int>(hdr.width), static_cast<int>(hdr.height)};
			(*ctx.texCache)[ctx.selectedAssetIdx] = tp;
		}
	}
	if(ctx.texCache) {
		const auto cit = ctx.texCache->find(ctx.selectedAssetIdx);
		if(cit != ctx.texCache->end() && cit->second.gl_texture && ctx.renderer) {
			ctx.renderer->drawTexturePreview(cit->second.gl_texture, cit->second.width, cit->second.height);
		}
	}
}

/*
 */
void inspectXPR(IAssetParser *p, InspectorContext &) {
	auto *xpr = static_cast<XPRParser *>(p);
	const auto &data = xpr->getXPRData();

	ImGui::Text("XPR Xbox Packed Resource");
	ImGui::Separator();
	ImGui::Text("Resources: %u", data.resource_count);

	if(data.resource_array_count > 0 &&
	   ImGui::BeginTable("xpr_res", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Index");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Format");
		ImGui::TableSetupColumn("Data Offset");
		ImGui::TableHeadersRow();

		for(uint32_t i = 0; i < data.resource_array_count; ++i) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%u", i);
			ImGui::TableNextColumn(); ImGui::Text("%s", data.resources[i].name);
			ImGui::TableNextColumn(); ImGui::Text("0x%08X", data.resources[i].descriptor.format);
			ImGui::TableNextColumn(); ImGui::Text("0x%08X", data.resources[i].descriptor.data_offset);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNOD(IAssetParser *p, InspectorContext &ctx) {
	auto *nod = static_cast<NODParser *>(p);
	const auto &md = nod->getModelData();

	ImGui::Text("NOD 3D Model");
	ImGui::Separator();

	if(ImGui::BeginTable("nod_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Flags");
		ImGui::TableNextColumn(); ImGui::Text("0x%08X", md.header.flags);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Bones");
		ImGui::TableNextColumn(); ImGui::Text("%u", md.bone_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Mesh Groups");
		ImGui::TableNextColumn(); ImGui::Text("%u", md.mesh_group_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Shaders");
		ImGui::TableNextColumn(); ImGui::Text("%u", md.shader_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Index Count");
		ImGui::TableNextColumn(); ImGui::Text("%u", md.index_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Vertex Sets");
		ImGui::TableNextColumn(); ImGui::Text("%u", md.vertex_set_count);
		ImGui::EndTable();
	}

	ImGui::Separator();
	if(ctx.renderer && !ctx.renderer->drawMeshPreview()) {
		ImGui::TextDisabled("No preview available.");
	}
}

/*
 */
void inspectNMB(IAssetParser *p, InspectorContext &ctx) {
	auto *nmb = static_cast<NMBParser *>(p);
	const auto &data = nmb->getBundleData();

	ImGui::Text("NMB Model Bundle");
	ImGui::Separator();
	ImGui::Text("Models: %u (parsed: %u) | Total Verts: %u | Total Indices: %u", data.model_count, data.parsed_count, data.total_vertices, data.total_indices);

	if(data.model_count > 1 && ctx.previewModelIndex && ctx.renderer && ctx.materials && ctx.ddsCatalog) {
		const int curIdx = *ctx.previewModelIndex;
		const char *curName = (curIdx >= 0 && static_cast<uint32_t>(curIdx) < data.model_count)
		                        ? data.models[curIdx].name : "(none)";
		if(ImGui::BeginCombo("Preview Model", curName)) {
			for(uint32_t i = 0; i < data.model_count && i < NMBParser::MAX_MODELS; ++i) {
				if(data.models[i].name[0] == '\0') continue;
				const bool sel = (static_cast<int>(i) == curIdx);
				if(ImGui::Selectable(data.models[i].name, sel) && static_cast<int>(i) != curIdx) {
					*ctx.previewModelIndex = static_cast<int>(i);
					viewer::scene::buildNmbPreview(nmb, *ctx.previewModelIndex, *ctx.renderer, *ctx.materials, *ctx.ddsCatalog);
				}
			}
			ImGui::EndCombo();
		}
	}

	ImGui::Separator();
	if(ctx.renderer && !ctx.renderer->drawMeshPreview()) {
		ImGui::TextDisabled("No preview available.");
	}
}

/*
 */
void inspectNIL(IAssetParser *, InspectorContext &ctx) {
	ImGui::Text("NIL Indoor Level");
	ImGui::Separator();
	if(ctx.renderer) ctx.renderer->drawMeshPreview();
}

/*
 */
void inspectNOC(IAssetParser *, InspectorContext &ctx) {
	ImGui::Text("NOC Collision Data");
	ImGui::Separator();
	if(ctx.renderer) ctx.renderer->drawMeshPreview();
}


/*
 */
void inspectNAD(IAssetParser *p, InspectorContext &) {
	auto *nad = static_cast<NADParser *>(p);
	const auto &clip = nad->getAnimationClip();

	ImGui::Text("NAD Animation Clip");
	ImGui::Separator();

	if(ImGui::BeginTable("nad_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Version");
		ImGui::TableNextColumn(); ImGui::Text("%u", clip.version);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Duration (frames)");
		ImGui::TableNextColumn(); ImGui::Text("%.1f", clip.duration);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Flags");
		ImGui::TableNextColumn(); ImGui::Text("0x%08X", clip.flags);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Bone Tracks");
		ImGui::TableNextColumn(); ImGui::Text("%u", clip.num_bone_tracks);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Key Tags");
		ImGui::TableNextColumn(); ImGui::Text("%u", clip.num_key_tags);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Shape Tracks");
		ImGui::TableNextColumn(); ImGui::Text("%u", clip.num_shape_tracks);
		ImGui::EndTable();
	}

	if(clip.num_bone_tracks > 0 && ImGui::TreeNode("Bone Tracks")) {
		if(ImGui::BeginTable("tracks", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			ImGui::TableSetupColumn("Track");
			ImGui::TableSetupColumn("Bone");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Keys");
			ImGui::TableHeadersRow();
			for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) {
				const auto &t = clip.bone_tracks[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%u", i);
				ImGui::TableNextColumn(); ImGui::Text("%u", t.bone_number);
				ImGui::TableNextColumn(); ImGui::Text("%s", t.type == 0 ? "Rotation" : "Translation");
				ImGui::TableNextColumn(); ImGui::Text("%u", t.num_keys);
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}

	if(clip.num_key_tags > 0 && ImGui::TreeNode("Key Tags")) {
		for(uint32_t i = 0; i < clip.num_key_tags; ++i)
			ImGui::BulletText("Frame %.1f: tag type %u", clip.key_tags[i].frame_num, clip.key_tags[i].tag_type);
		ImGui::TreePop();
	}
}

/*
 */
void inspectNMS(IAssetParser *p, InspectorContext &) {
	auto *nms = static_cast<NMSParser *>(p);
	const auto &data = nms->getMotionSetData();

	ImGui::Text("NMS Motion Set");
	ImGui::Separator();
	ImGui::Text("Entries: %u", data.entry_count);

	if(data.header_comment && data.header_comment_len > 0) {
		if(ImGui::TreeNode("Header Comment")) {
			ImGui::TextWrapped("%.*s", static_cast<int>(data.header_comment_len), data.header_comment);
			ImGui::TreePop();
		}
	}

	if(data.entry_count > 0 &&
	   ImGui::BeginTable("nms_motions", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Motion Name");
		ImGui::TableSetupColumn("Animation File");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.entry_count; ++i) {
			const auto &e = data.entries[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", e.motion_name);
			ImGui::TableNextColumn(); ImGui::Text("%s", e.animation_file);
			ImGui::TableNextColumn();
			for(uint32_t f = 0; f < e.flag_count; ++f) {
				if(f > 0) ImGui::SameLine();
				ImGui::TextDisabled("%s", e.flags[f]);
			}
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNSD(IAssetParser *p, InspectorContext &) {
	auto *nsd = static_cast<NSDParser *>(p);
	const auto &scene = nsd->getSceneData();

	ImGui::Text("NSD Scene Data");
	ImGui::Separator();
	ImGui::Text("Entities: %u  |  Types: %u", scene.entity_count, scene.type_name_count);
	if(scene.has_sky_model) ImGui::Text("Sky Model: %s", scene.sky_model_name);

	if(scene.entity_count > 0 &&
	   ImGui::BeginTable("nsd_ents", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Index");
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Model");
		ImGui::TableSetupColumn("Position");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < scene.entity_count; ++i) {
			const auto &e = scene.entities[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%u", i);
			ImGui::TableNextColumn(); ImGui::Text("%s", e.type_name);
			ImGui::TableNextColumn(); ImGui::Text("%s", e.has_model ? e.model_name : "—");
			ImGui::TableNextColumn();
			if(e.has_transform) {
				ImGui::Text("(%.1f, %.1f, %.1f)", e.position[0], e.position[1], e.position[2]);
			}
			else {
				ImGui::TextDisabled("—");
			}
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNOT(IAssetParser *p, InspectorContext &) {
	auto *notP = static_cast<NOTParser *>(p);
	const auto &db = notP->getTemplateDB();

	ImGui::Text("NOT Object Template Database");
	ImGui::Separator();
	ImGui::Text("Templates: %u", db.num_templates);

	if(db.num_templates > 0 &&
	   ImGui::BeginTable("not_tmpl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Parent");
		ImGui::TableSetupColumn("Properties");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < db.num_templates; ++i) {
			const auto &t = db.templates[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", t.name);
			ImGui::TableNextColumn(); ImGui::Text("%s", t.parent[0] ? t.parent : "—");
			ImGui::TableNextColumn(); ImGui::Text("%u", t.property_count);
			ImGui::TableNextColumn(); ImGui::Text("%u", t.flags);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNSA(IAssetParser *p, InspectorContext &) {
	auto *nsa = static_cast<NSAParser *>(p);
	const auto &data = nsa->getMaterialsData();

	ImGui::Text("NSA Material Archive");
	ImGui::Separator();
	ImGui::Text("Materials: %u", data.material_count);

	for(uint32_t i = 0; i < data.material_count && i < 256; ++i) {
		const auto &mat = data.materials[i];
		if(ImGui::TreeNode(mat.name)) {
			for(uint32_t j = 0; j < mat.prop_count; ++j)
				ImGui::BulletText("%s = %s", mat.props[j].key, mat.props[j].value);
			ImGui::TreePop();
		}
	}
}

/*
 */
void inspectNFX(IAssetParser *p, InspectorContext &) {
	auto *nfx = static_cast<NFXParser *>(p);
	const auto &data = nfx->getEffectsData();

	ImGui::Text("NFX Effects");
	ImGui::Separator();
	ImGui::Text("Blocks: %u (controllers: %u, effects: %u)", data.block_count, data.controller_count, data.effect_count);

	for(uint32_t i = 0; i < data.block_count && i < 128; ++i) {
		const auto &blk = data.blocks[i];
		char label[128];
		std::snprintf(label, sizeof(label), "%s (%s)##%u", blk.name, blk.type, i);
		if(ImGui::TreeNode(label)) {
			for(uint32_t j = 0; j < blk.property_count; ++j)
				ImGui::BulletText("%s = %s", blk.properties[j].key, blk.properties[j].value);
			ImGui::TreePop();
		}
	}
}

/*
 */
void inspectNUI(IAssetParser *p, InspectorContext &) {
	auto *nui = static_cast<NUIParser *>(p);
	const auto &data = nui->getUILayoutData();

	ImGui::Text("NUI UI Layout");
	ImGui::Separator();
	ImGui::Text("Dialog: %s (%d, %d) | Elements: %u | Quads: %u", data.name, data.pos_x, data.pos_y, data.element_count, data.quad_count);
	if(data.music[0]) ImGui::Text("Music: %s", data.music);

	if(data.quad_count > 0 && ImGui::TreeNode("Quads")) {
		for(uint32_t i = 0; i < data.quad_count; ++i) {
			const auto &q = data.quads[i];
			ImGui::BulletText("%s pos=(%d,%d) size=%dx%d", q.material, q.pos_x, q.pos_y, q.width, q.height);
		}
		ImGui::TreePop();
	}

	if(data.element_count > 0 &&
	   ImGui::BeginTable("nui_elems", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Position");
		ImGui::TableSetupColumn("Size");
		ImGui::TableSetupColumn("Font");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.element_count; ++i) {
			const auto &e = data.elements[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", e.name);
			ImGui::TableNextColumn(); ImGui::Text("%s", NUIParser::elementTypeToString(e.type));
			ImGui::TableNextColumn(); ImGui::Text("(%d, %d)", e.pos_x, e.pos_y);
			ImGui::TableNextColumn(); ImGui::Text("%d x %d", e.width, e.height);
			ImGui::TableNextColumn(); ImGui::Text("%s", e.font[0] ? e.font : "—");
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNLT(IAssetParser *p, InspectorContext &) {
	auto *nlt = static_cast<NLTParser *>(p);
	const auto &data = nlt->getTextData();

	ImGui::Text("NLT Localization Text");
	ImGui::Separator();
	ImGui::Text("Size: %zu bytes", data.text_len);

	if(data.text && data.text_len > 0) {
		ImGui::Separator();
		const std::size_t showLen = data.text_len < 4096 ? data.text_len : 4096;
		std::string preview(data.text, showLen);
		int lineCount = 1;
		for(std::size_t i = 0; i < showLen; ++i)
			if(preview[i] == '\n') lineCount++;
		const float h = ImGui::GetTextLineHeight() * (lineCount + 1) +
		                ImGui::GetStyle().FramePadding.y * 2;
		ImGui::InputTextMultiline("##nlt_text", &preview[0], preview.size() + 1, ImVec2(-1, h), ImGuiInputTextFlags_ReadOnly);
		if(data.text_len > 4096) {
			ImGui::TextDisabled("(showing first 4096 of %zu bytes)", data.text_len);
		}
	}
}

/*
 */
void inspectNLS(IAssetParser *p, InspectorContext &) {
	auto *nls = static_cast<NLSParser *>(p);
	const auto &data = nls->getLocalizationData();

	ImGui::Text("NLS Localization Strings");
	ImGui::Separator();
	ImGui::Text("Entries: %u", data.entry_count);

	if(data.entry_count > 0 &&
	   ImGui::BeginTable("nls_strings", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Key");
		ImGui::TableSetupColumn("Value");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.entry_count; ++i) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", data.entries[i].key);
			ImGui::TableNextColumn(); ImGui::TextWrapped("%s", data.entries[i].value);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNPD(IAssetParser *p, InspectorContext &) {
	auto *npd = static_cast<NPDParser *>(p);
	const auto &data = npd->getPlacementData();

	ImGui::Text("NPD Placement Data");
	ImGui::Separator();
	ImGui::Text("Records: %u", data.record_count);

	if(data.record_count > 0 &&
	   ImGui::BeginTable("npd_recs", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Entity ID");
		ImGui::TableSetupColumn("Position");
		ImGui::TableSetupColumn("Flags");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.record_count; ++i) {
			const auto &r = data.records[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%u", r.entity_id);
			ImGui::TableNextColumn(); ImGui::Text("(%.1f, %.1f, %.1f)", r.position.x, r.position.y, r.position.z);
			ImGui::TableNextColumn(); ImGui::Text("0x%04X", r.flags);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNAK(IAssetParser *p, InspectorContext &) {
	auto *nak = static_cast<NAKParser *>(p);
	const auto &data = nak->getCameraTrackData();

	ImGui::Text("NAK Camera Track");
	ImGui::Separator();
	ImGui::Text("Keyframes: %u", data.keyframe_count);

	if(data.keyframe_count > 0 &&
	   ImGui::BeginTable("nak_keys", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Time");
		ImGui::TableSetupColumn("FOV");
		ImGui::TableSetupColumn("Near");
		ImGui::TableSetupColumn("Far");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.keyframe_count; ++i) {
			const auto &k = data.keyframes[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%.3f", k.time);
			ImGui::TableNextColumn(); ImGui::Text("%.1f", k.fov);
			ImGui::TableNextColumn(); ImGui::Text("%.2f", k.near_clip);
			ImGui::TableNextColumn(); ImGui::Text("%.1f", k.far_clip);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNCE(IAssetParser *p, InspectorContext &) {
	auto *nce = static_cast<NCEParser *>(p);
	const auto &data = nce->getCutsceneEventData();

	ImGui::Text("NCE Cutscene Events");
	ImGui::Separator();
	if(ImGui::BeginTable("nce_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Version");
		ImGui::TableNextColumn(); ImGui::Text("%u", data.version);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Map");
		ImGui::TableNextColumn(); ImGui::Text("%s", data.map_name);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Frames");
		ImGui::TableNextColumn(); ImGui::Text("%u", data.num_frames);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Time Scale");
		ImGui::TableNextColumn(); ImGui::Text("%.2f", data.time_scale);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Shots / Events");
		ImGui::TableNextColumn(); ImGui::Text("%u / %u", data.shot_count, data.event_count);
		ImGui::EndTable();
	}

	if(ImGui::TreeNode("Camera")) {
		const auto &cam = data.camera;
		ImGui::Text("Position: (%.1f, %.1f, %.1f)", cam.pos_x, cam.pos_y, cam.pos_z);
		ImGui::Text("Target: (%.1f, %.1f, %.1f)", cam.target_x, cam.target_y, cam.target_z);
		ImGui::Text("FOV: %.1f", cam.fov);
		if(cam.has_camera_nad) ImGui::Text("Camera NAD: %s", cam.camera_nad);
		ImGui::TreePop();
	}

	if(data.actor_count > 0 && ImGui::TreeNode("Actors")) {
		for(uint32_t i = 0; i < data.actor_count; ++i) {
			const auto &a = data.actors[i];
			char label[128];
			std::snprintf(label, sizeof(label), "%s (%s)##nce_actor_%u", a.instance_name, a.model_name, i);
			if(ImGui::TreeNode(label)) {
				ImGui::Text("NAD: %s", a.nad_path);
				ImGui::Text("Visibility: %u  |  Anim Indicator: %u", a.visibility, a.anim_indicator);
				for(uint32_t j = 0; j < a.anim_state_count; ++j)
					ImGui::BulletText("Frame %.1f: %s (type %u)", a.anim_states[j].frame_time, a.anim_states[j].name, a.anim_states[j].type);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	if(data.sound_cue_count > 0 && ImGui::TreeNode("Sound Cues")) {
		for(uint32_t i = 0; i < data.sound_cue_count; ++i) {
			const auto &sc = data.sound_cues[i];
			ImGui::BulletText("Frame %.1f: %s (vol %.2f)", sc.frame_time, sc.name, sc.volume);
		}
		ImGui::TreePop();
	}

	if(data.conversation_count > 0 && ImGui::TreeNode("Conversations")) {
		for(uint32_t i = 0; i < data.conversation_count; ++i) {
			const auto &cv = data.conversations[i];
			ImGui::BulletText("Frame %.1f: %s", cv.frame_time, cv.name);
		}
		ImGui::TreePop();
	}
}

/*
 */
void inspectNCS(IAssetParser *p, InspectorContext &) {
	auto *ncs = static_cast<NCSParser *>(p);
	const auto &data = ncs->getCutsceneScriptData();

	ImGui::Text("NCS Cutscene Script");
	ImGui::Separator();
	if(ImGui::BeginTable("ncs_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Version");
		ImGui::TableNextColumn(); ImGui::Text("%u", data.version);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Variants");
		ImGui::TableNextColumn(); ImGui::Text("%u / %u / %u", data.variant1, data.variant2, data.variant3);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Shots / Events");
		ImGui::TableNextColumn(); ImGui::Text("%u / %u", data.shot_count, data.event_count);
		ImGui::EndTable();
	}
	if(data.has_camera_nad) ImGui::Text("Camera NAD: %s", data.camera_nad);

	if(data.actor_count > 0 && ImGui::TreeNode("Actors")) {
		for(uint32_t i = 0; i < data.actor_count; ++i) {
			const auto &a = data.actors[i];
			char label[128];
			std::snprintf(label, sizeof(label), "%s (%s)##ncs_actor_%u", a.instance_name, a.model_name, i);
			if(ImGui::TreeNode(label)) {
				ImGui::Text("NAD: %s", a.nad_path);
				ImGui::Text("Marker2: %u", a.marker2);
				for(uint32_t j = 0; j < a.anim_state_count; ++j)
					ImGui::BulletText("Frame %.1f: %s (type %u)", a.anim_states[j].frame_time, a.anim_states[j].name, a.anim_states[j].type);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	if(data.sound_cue_count > 0 && ImGui::TreeNode("Sound Cues")) {
		for(uint32_t i = 0; i < data.sound_cue_count; ++i) {
			const auto &sc = data.sound_cues[i];
			ImGui::BulletText("Frame %.1f: %s (vol %.2f)", sc.frame_time, sc.name, sc.volume);
		}
		ImGui::TreePop();
	}
}

/*
 */
void inspectNCO(IAssetParser *p, InspectorContext &) {
	auto *nco = static_cast<NCOParser *>(p);
	const auto &data = nco->getConversationData();

	ImGui::Text("NCO Conversation");
	ImGui::Separator();
	ImGui::Text("Lines: %u  |  Speakers: %u", data.line_count, data.speaker_count);

	if(data.speaker_count > 0 && ImGui::TreeNode("Speakers")) {
		for(uint32_t i = 0; i < data.speaker_count && i < NCOParser::MAX_SPEAKERS; ++i)
			ImGui::BulletText("[%u] %s", i, data.speaker_names[i]);
		ImGui::TreePop();
	}

	if(data.line_count > 0) {
		ImGui::Separator();
		for(uint32_t i = 0; i < data.line_count; ++i) {
			const auto &line = data.lines[i];
			ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "[%s] (%.1fs)", line.speaker_name, line.duration_ms / 1000.0f);
			ImGui::TextWrapped("%s", line.text);
			ImGui::Spacing();
		}
	}
}

/*
 */
void inspectNNB(IAssetParser *p, InspectorContext &) {
	auto *nnb = static_cast<NNBParser *>(p);
	const auto &data = nnb->getBundleData();

	ImGui::Text("NNB Animation Bundle");
	ImGui::Separator();
	ImGui::Text("Animations: %u  |  Names Found: %u", data.anim_count, data.names_found);

	if(data.names_found > 0 &&
	   ImGui::BeginTable("nnb_entries", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Offset");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < data.names_found && i < NNBParser::MAX_ANIMS; ++i) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", data.anims[i].name);
			ImGui::TableNextColumn(); ImGui::Text("0x%zX", data.anims[i].offset);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectTerrain(IAssetParser *p, InspectorContext &) {
	auto *ter = static_cast<TerrainParser *>(p);
	const auto &grid = ter->getTerrainGrid();

	ImGui::Text("Terrain Grid");
	ImGui::Separator();
	ImGui::Text("Version: %u | Grid: %u x %u | Patches: %u", grid.version, grid.grid_width, grid.grid_height, grid.patch_count);

	if(grid.patch_count > 0 &&
	   ImGui::BeginTable("ter_patches", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Patch");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Data Size");
		ImGui::TableHeadersRow();
		for(uint32_t i = 0; i < grid.patch_count && i < 128; ++i) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%u", i);
			ImGui::TableNextColumn(); ImGui::Text("%s", grid.patches[i].name);
			ImGui::TableNextColumn(); ImGui::Text("%zu bytes", grid.patches[i].data_size);
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectNLFX(IAssetParser *p, InspectorContext &) {
	auto *nlfx = static_cast<NLFXParser *>(p);
	const auto &data = nlfx->getData();

	ImGui::Text("NLFX Level Effects");
	ImGui::Separator();
	ImGui::Text("Effects: %u", data.effect_count);
	for(uint32_t i = 0; i < data.effect_count && i < NLFXParser::MAX_EFFECTS; ++i)
		ImGui::BulletText("[%u] %s", i, data.effect_names[i]);
}


/*
 */
void inspectXWB(IAssetParser *p, InspectorContext &ctx) {
	auto *xwb = static_cast<XWBParser *>(p);
	const auto &wb = xwb->getWaveBankData();

	ImGui::Text("XWB Wave Bank");
	ImGui::Separator();

	if(ImGui::BeginTable("xwb_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Bank Name");
		ImGui::TableNextColumn(); ImGui::Text("%s", wb.bank_name);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Version");
		ImGui::TableNextColumn(); ImGui::Text("%u", wb.version);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Entries");
		ImGui::TableNextColumn(); ImGui::Text("%u", wb.entry_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Wave Data");
		ImGui::TableNextColumn(); ImGui::Text("%.1f KB", wb.wave_data_size / 1024.0f);
		ImGui::EndTable();
	}

	if(ctx.audio && ctx.audio->isPlaying()) {
		if(ImGui::Button("Stop")) {
			ctx.audio->stop();
			if(ctx.playingEntry) *ctx.playingEntry = -1;
		}
		ImGui::SameLine();
		ImGui::Text("Playing: Wave %d", ctx.playingEntry ? *ctx.playingEntry : -1);
		ImGui::SameLine();
		ImGui::ProgressBar(ctx.audio->getProgress(), ImVec2(120, 0));
	}
	ImGui::Separator();

	if(wb.entry_count > 0 &&
	   ImGui::BeginTable("xwb_entries", 7,
	                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed, 32);
		ImGui::TableSetupColumn("Codec");
		ImGui::TableSetupColumn("Ch",       ImGuiTableColumnFlags_WidthFixed, 24);
		ImGui::TableSetupColumn("Rate",     ImGuiTableColumnFlags_WidthFixed, 50);
		ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed, 64);
		ImGui::TableSetupColumn("Play",     ImGuiTableColumnFlags_WidthFixed, 50);
		ImGui::TableHeadersRow();

		for(uint32_t i = 0; i < wb.entry_count; ++i) {
			const auto &e = wb.entries[i];
			const float dur = xwb->getEntryDuration(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%u", i);
			ImGui::TableNextColumn(); ImGui::Text("%s", XWBParser::codecName(e.codec));
			ImGui::TableNextColumn(); ImGui::Text("%u", e.channels);
			ImGui::TableNextColumn(); ImGui::Text("%u", e.sample_rate);
			ImGui::TableNextColumn(); ImGui::Text("%.2fs", dur);
			ImGui::TableNextColumn(); ImGui::Text("%.1fK", e.data_length / 1024.0f);

			ImGui::TableNextColumn();
			const bool isThis = ctx.audio && ctx.audio->isPlaying() &&
			                    ctx.playingEntry && *ctx.playingEntry == static_cast<int>(i);

			if(e.codec == XWBParser::CODEC_WMA) {
				ImGui::TextDisabled("n/a");
			} else if(isThis) {
				ImGui::PushID(static_cast<int>(i));
				if(ImGui::SmallButton("Stop")) {
					ctx.audio->stop();
					if(ctx.playingEntry) *ctx.playingEntry = -1;
				}
				ImGui::PopID();
			} else {
				ImGui::PushID(static_cast<int>(i));
				if(ImGui::SmallButton("Play")) {
					uint32_t frames, ch, rate;
					int16_t *pcm = xwb->decodeToPCM16(i, &frames, &ch, &rate);
					if(pcm && ctx.audio) {
						ctx.audio->play(pcm, frames, ch, rate);
						if(ctx.playingEntry) *ctx.playingEntry = static_cast<int>(i);
						std::free(pcm);
					}
				}
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}
}

/*
 */
void inspectXSB(IAssetParser *p, InspectorContext &ctx) {
	auto *xsb = static_cast<XSBParser *>(p);
	const auto &sb = xsb->getSoundBankData();

	ImGui::Text("XSB Sound Bank");
	ImGui::Separator();
	if(ImGui::BeginTable("xsb_info", 2, ImGuiTableFlags_Borders)) {
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Bank Name");
		ImGui::TableNextColumn(); ImGui::Text("%s", sb.bank_name);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Version");
		ImGui::TableNextColumn(); ImGui::Text("%u", sb.version);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Simple Cues");
		ImGui::TableNextColumn(); ImGui::Text("%u", sb.simple_cue_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Complex Cues");
		ImGui::TableNextColumn(); ImGui::Text("%u", sb.complex_cue_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Total Cues");
		ImGui::TableNextColumn(); ImGui::Text("%u", sb.total_cue_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Named Cues");
		ImGui::TableNextColumn(); ImGui::Text("%u", sb.cue_name_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("Wave Banks");
		ImGui::TableNextColumn(); ImGui::Text("%u", sb.wave_bank_count);
		ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("File Size");
		ImGui::TableNextColumn(); ImGui::Text("%zu bytes", sb.file_size);
		ImGui::EndTable();
	}

	if(!sb.wave_bank_names.empty()) {
		ImGui::Spacing();
		ImGui::Text("Wave Bank References:");
		for(std::size_t i = 0; i < sb.wave_bank_names.size(); ++i)
			ImGui::BulletText("[%zu] %s.xwb", i, sb.wave_bank_names[i].c_str());
	}

	if(ctx.audio && ctx.audio->isPlaying()) {
		ImGui::Spacing();
		if(ImGui::Button("Stop##xsb")) {
			ctx.audio->stop();
			if(ctx.playingEntry) *ctx.playingEntry = -1;
		}
		ImGui::SameLine();
		ImGui::Text("Playing entry %d", ctx.playingEntry ? *ctx.playingEntry : -1);
		ImGui::SameLine();
		ImGui::ProgressBar(ctx.audio->getProgress(), ImVec2(120, 0));
	}

	if(!sb.cue_mappings.empty()) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Cue Mappings (%zu cues):", sb.cue_mappings.size());

		if(ImGui::BeginTable("xsb_map", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed, 32);
			ImGui::TableSetupColumn("Cue Name");
			ImGui::TableSetupColumn("Bank",     ImGuiTableColumnFlags_WidthFixed, 80);
			ImGui::TableSetupColumn("Wave(s)",  ImGuiTableColumnFlags_WidthFixed, 80);
			ImGui::TableSetupColumn("Play",     ImGuiTableColumnFlags_WidthFixed, 50);
			ImGui::TableHeadersRow();

			std::string xsbDir;
			if(ctx.db && ctx.selectedAssetIdx >= 0 &&
			   static_cast<std::size_t>(ctx.selectedAssetIdx) < ctx.db->entries().size()) {
				const std::string &rp = ctx.db->entries()[ctx.selectedAssetIdx].relativePath;
				const std::size_t sep = rp.find_last_of("/\\");
				xsbDir = (sep != std::string::npos)
				           ? ctx.db->rootPath() + "/" + rp.substr(0, sep)
				           : ctx.db->rootPath();
			}

			int rowId = 0;
			for(std::size_t c = 0; c < sb.cue_mappings.size(); ++c) {
				const auto &cm = sb.cue_mappings[c];

				if(cm.waves.empty()) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::Text("%d", rowId++);
					ImGui::TableNextColumn(); ImGui::Text("%s", cm.name.empty() ? "(unnamed)" : cm.name.c_str());
					ImGui::TableNextColumn(); ImGui::TextDisabled("-");
					ImGui::TableNextColumn(); ImGui::TextDisabled("-");
					ImGui::TableNextColumn(); ImGui::TextDisabled("-");
					continue;
				}

				for(std::size_t w = 0; w < cm.waves.size(); ++w) {
					const auto &wr = cm.waves[w];
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::Text("%d", rowId);

					ImGui::TableNextColumn();
					if(cm.waves.size() == 1) {
						ImGui::Text("%s", cm.name.empty() ? "(unnamed)" : cm.name.c_str());
					}
					else {
						ImGui::Text("%s [%zu]", cm.name.empty() ? "(unnamed)" : cm.name.c_str(), w);
					}

					ImGui::TableNextColumn();
					if(wr.wave_bank_index < sb.wave_bank_names.size()) {
						ImGui::Text("%s", sb.wave_bank_names[wr.wave_bank_index].c_str());
					}
					else {
						ImGui::Text("bank%u", wr.wave_bank_index);
					}

					ImGui::TableNextColumn();
					ImGui::Text("%u", wr.wave_index);

					ImGui::TableNextColumn();
					const bool isThis = ctx.audio && ctx.audio->isPlaying() &&
					                    ctx.playingEntry && *ctx.playingEntry == rowId;

					ImGui::PushID(rowId);
					if(isThis) {
						if(ImGui::SmallButton("Stop")) {
							ctx.audio->stop();
							if(ctx.playingEntry) *ctx.playingEntry = -1;
						}
					} else {
						if(ImGui::SmallButton("Play")) {
							std::string wbName;
							if(wr.wave_bank_index < sb.wave_bank_names.size()) {
								wbName = sb.wave_bank_names[wr.wave_bank_index];
							}

							if(!wbName.empty() && !xsbDir.empty()) {
								const std::string xwbPath = xsbDir + "/" + wbName + ".xwb";
								IAssetParser *xwbP = AssetParserFactory::parseFile(xwbPath.c_str());
								if(xwbP) {
									auto *xwbObj = static_cast<XWBParser *>(xwbP);
									const auto &wbd = xwbObj->getWaveBankData();
									if(wr.wave_index < wbd.entry_count) {
										uint32_t frames, ch, rate;
										int16_t *pcm = xwbObj->decodeToPCM16(wr.wave_index, &frames, &ch, &rate);
										if(pcm && ctx.audio) {
											ctx.audio->play(pcm, frames, ch, rate);
											if(ctx.playingEntry) *ctx.playingEntry = rowId;
											std::free(pcm);
										}
									}
									delete xwbP;
								}
							}
						}
					}
					ImGui::PopID();
					++rowId;
				}
			}
			ImGui::EndTable();
		}
	} else if(!sb.cue_names.empty()) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Cue Names (%zu):", sb.cue_names.size());

		if(ImGui::BeginTable("xsb_cues", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40);
			ImGui::TableSetupColumn("Cue Name");
			ImGui::TableHeadersRow();
			for(std::size_t i = 0; i < sb.cue_names.size(); ++i) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%zu", i);
				ImGui::TableNextColumn(); ImGui::Text("%s", sb.cue_names[i].c_str());
			}
			ImGui::EndTable();
		}
	} else {
		ImGui::Spacing();
		ImGui::TextDisabled("No cue data found in this sound bank.");
	}
}

} // namespace


/*
 */
void drawInspector(IAssetParser *parser, InspectorContext &ctx) {
	if(!parser) {
		ImGui::TextDisabled("No parser available for this format.");
		return;
	}
	const char *type = parser->getFileType();

	if     (std::strcmp(type, "DDS")     == 0) inspectDDS(parser, ctx);
	else if(std::strcmp(type, "TGA")     == 0) inspectTGA(parser, ctx);
	else if(std::strcmp(type, "XPR")     == 0) inspectXPR(parser, ctx);
	else if(std::strcmp(type, "NOD")     == 0) inspectNOD(parser, ctx);
	else if(std::strcmp(type, "NMB")     == 0) inspectNMB(parser, ctx);
	else if(std::strcmp(type, "NAD")     == 0) inspectNAD(parser, ctx);
	else if(std::strcmp(type, "NMS")     == 0) inspectNMS(parser, ctx);
	else if(std::strcmp(type, "NIL")     == 0) inspectNIL(parser, ctx);
	else if(std::strcmp(type, "NSD")     == 0) inspectNSD(parser, ctx);
	else if(std::strcmp(type, "NOT")     == 0) inspectNOT(parser, ctx);
	else if(std::strcmp(type, "NSA")     == 0) inspectNSA(parser, ctx);
	else if(std::strcmp(type, "NFX")     == 0) inspectNFX(parser, ctx);
	else if(std::strcmp(type, "NUI")     == 0) inspectNUI(parser, ctx);
	else if(std::strcmp(type, "NLT")     == 0) inspectNLT(parser, ctx);
	else if(std::strcmp(type, "NLS")     == 0) inspectNLS(parser, ctx);
	else if(std::strcmp(type, "NOC")     == 0) inspectNOC(parser, ctx);
	else if(std::strcmp(type, "NPD")     == 0) inspectNPD(parser, ctx);
	else if(std::strcmp(type, "NAK")     == 0) inspectNAK(parser, ctx);
	else if(std::strcmp(type, "NCE")     == 0) inspectNCE(parser, ctx);
	else if(std::strcmp(type, "NCS")     == 0) inspectNCS(parser, ctx);
	else if(std::strcmp(type, "NCO")     == 0) inspectNCO(parser, ctx);
	else if(std::strcmp(type, "NNB")     == 0) inspectNNB(parser, ctx);
	else if(std::strcmp(type, "TERRAIN") == 0) inspectTerrain(parser, ctx);
	else if(std::strcmp(type, "NLFX")    == 0) inspectNLFX(parser, ctx);
	else if(std::strcmp(type, "XWB")     == 0) inspectXWB(parser, ctx);
	else if(std::strcmp(type, "XSB")     == 0) inspectXSB(parser, ctx);
	else {
		ImGui::Text("Parsed as: %s", type);
		ImGui::TextDisabled("No inspector implemented for this type.");
	}
}

} // namespace viewer::ui
