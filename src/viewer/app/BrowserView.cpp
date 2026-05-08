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

#include "app/BrowserView.h"

#include "core/Strings.h"
#include "scene/PreviewBuilders.h"
#include "ui/Inspectors.h"
#include "ui/DetailsPanels.h"

#include "parsers/AssetParser.h"
#include "parsers/AssetParserFactory.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

/*
 */
namespace viewer::app {

/*
 */
using viewer::core::formatSize;
using viewer::core::getFilename;
using viewer::core::toLower;

/*
 */
BrowserView::BrowserView() {
	m_renderer.init();
}

/*
 */
BrowserView::~BrowserView() {
	for(auto &kv : m_texCache)
		if(kv.second.gl_texture) glDeleteTextures(1, &kv.second.gl_texture);
	m_texCache.clear();

	m_renderer.shutdown();
	delete m_activeParser;
	m_activeParser = nullptr;
}

/*
 */
bool BrowserView::mount(const char *rootPath) {
	const bool ok = m_db.mount(rootPath);
	m_ddsCatalog.setAssetDir(rootPath);
	m_materials.setAssetDir(rootPath);
	std::printf("[BrowserView] Mounted '%s': %d files (%d parsable)\n", rootPath, m_db.totalFiles(), m_db.parsableFiles());
	return ok;
}

/*
 */
void BrowserView::parseSelected() {
	const auto &assets = m_db.entries();
	if(m_selectedAsset < 0 || m_selectedAsset >= static_cast<int>(assets.size())) return;

	auto &entries = m_db.entriesMut();
	auto &e = entries[m_selectedAsset];
	const std::string fullPath = m_db.fullPath(m_selectedAsset);

	delete m_activeParser;
	m_activeParser = nullptr;

	m_animation.unload();

	m_renderer.clearMesh();
	m_renderer.clearEntityMesh();
	m_previewModelIdx = 0;

	m_audio.stop();
	m_playingEntry = -1;

	// Cache eviction: keep <= 64 GPU textures alive
	if(m_texCache.size() > 64) {
		for(auto &kv : m_texCache)
			if(kv.second.gl_texture) glDeleteTextures(1, &kv.second.gl_texture);
		m_texCache.clear();
	}

	m_activeParser = AssetParserFactory::parseFile(fullPath.c_str());
	if(!m_activeParser) {
		e.isParsed = false;
		e.isFailed = true;
		return;
	}
	e.isParsed = true;
	e.isFailed = false;

	const char *type = m_activeParser->getFileType();
	if(std::strcmp(type, "NOD") == 0)
		viewer::scene::buildNodPreview(static_cast<NODParser *>(m_activeParser), m_renderer, m_materials, m_ddsCatalog);
	else if(std::strcmp(type, "NMB") == 0)
		viewer::scene::buildNmbPreview(static_cast<NMBParser *>(m_activeParser), 0, m_renderer, m_materials, m_ddsCatalog);
	else if(std::strcmp(type, "NIL") == 0)
		viewer::scene::buildNilPreview(static_cast<NILParser *>(m_activeParser), fullPath, m_db.rootPath(), m_renderer, m_materials, m_ddsCatalog);
	else if(std::strcmp(type, "NOC") == 0) {
		viewer::scene::buildNocPreview(static_cast<NOCParser *>(m_activeParser), m_renderer);
	}
}

/*
 */
void BrowserView::draw() {
	// drive the animation timeline.
	if(m_animation.isLoaded() && m_animation.playing()) {
		m_animation.tick(ImGui::GetTime());
		m_animation.apply(m_activeParser, m_previewModelIdx, m_renderer);
	} else if(m_animation.isLoaded()) {
		// allow the frame slider to scrub even when paused.
		m_animation.apply(m_activeParser, m_previewModelIdx, m_renderer);
	}

	// main menu bar
	if(ImGui::BeginMainMenuBar()) {
		if(ImGui::BeginMenu("File")) {
			if(ImGui::MenuItem("Quit", "Ctrl+Q")) {} // handled in ViewerApp
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Filter by extension", nullptr, false, false);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		ImGui::TextDisabled("| %d files | %d parsable |", m_db.totalFiles(), m_db.parsableFiles());
		ImGui::EndMainMenuBar();
	}

	const float panelW = 360.0f;
	const auto &assets = m_db.entries();
	const auto &categories = m_db.categories();

	ImGui::SetNextWindowPos(ImVec2(0, 20));
	ImGui::SetNextWindowSize(ImVec2(panelW, ImGui::GetIO().DisplaySize.y - 20));
	ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	{
		ImGui::Text("Filter:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-60);
		ImGui::InputText("##filter", m_filterBuf, sizeof(m_filterBuf));
		ImGui::SameLine();
		if(ImGui::SmallButton("Clear")) {
			m_filterBuf[0] = '\0';
			m_filterExt.clear();
		}

		ImGui::Spacing();
		static const char *kQuickFilters[] = {"All", "nod", "nad", "nil", "nsd", "dds", "tga",
		                                      "xpr", "nsa", "nui", "not", "nms", "nfx"};
		for(int i = 0; i < static_cast<int>(sizeof(kQuickFilters) / sizeof(kQuickFilters[0])); ++i) {
			if(i > 0) ImGui::SameLine();
			const bool active = (i == 0 && m_filterExt.empty()) ||
			                    (i > 0 && m_filterExt == kQuickFilters[i]);
			if(active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
			if(ImGui::SmallButton(kQuickFilters[i])) {
				m_filterExt = (i == 0) ? "" : kQuickFilters[i];
			}
			if(active) ImGui::PopStyleColor();
		}

		ImGui::Separator();

		const std::string filterStr = toLower(m_filterBuf);

		ImGui::BeginChild("FileList", ImVec2(0, 0), ImGuiChildFlags_None);
		for(const auto &cat : categories) {
			int visCount = 0;
			for(int idx : cat.assetIndices) {
				const auto &e = assets[idx];
				if(!m_filterExt.empty() && e.extension != m_filterExt) continue;
				if(!filterStr.empty() && toLower(e.relativePath).find(filterStr) == std::string::npos) continue;
				++visCount;
			}
			if(visCount == 0) continue;

			char catLabel[256];
			std::snprintf(catLabel, sizeof(catLabel), "%s (%d)", cat.name.c_str(), visCount);
			if(!ImGui::TreeNode(catLabel)) continue;

			for(int idx : cat.assetIndices) {
				const auto &e = assets[idx];
				if(!m_filterExt.empty() && e.extension != m_filterExt) continue;
				if(!filterStr.empty() && toLower(e.relativePath).find(filterStr) == std::string::npos) continue;

				const std::string label =
				    getFilename(e.relativePath) + "  (" + formatSize(e.fileSize) + ")";

				bool pushed = false;
				if(e.isFailed) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
					pushed = true;
				} else if(e.isParsed) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
					pushed = true;
				}

				const bool selected = (m_selectedAsset == idx);
				if(ImGui::Selectable(label.c_str(), selected)) {
					m_selectedAsset = idx;
					parseSelected();
				}
				if(pushed) ImGui::PopStyleColor();

				if(ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
					ImGui::Text("%s", e.relativePath.c_str());
					ImGui::Text("Type: .%s | Size: %s", e.extension.c_str(), formatSize(e.fileSize).c_str());
					ImGui::EndTooltip();
				}
			}
			ImGui::TreePop();
		}
		ImGui::EndChild();
	}
	ImGui::End();

	const bool  showDetails = viewer::ui::has3dPreview(m_activeParser);
	const float availW = ImGui::GetIO().DisplaySize.x - panelW;
	const float availH = ImGui::GetIO().DisplaySize.y - 20;
	const float inspectorW = showDetails ? availW * 0.55f : availW;
	const float detailsW = availW - inspectorW;

	viewer::ui::InspectorContext ctx;
	ctx.renderer = &m_renderer;
	ctx.audio = &m_audio;
	ctx.db = &m_db;
	ctx.materials = &m_materials;
	ctx.ddsCatalog = &m_ddsCatalog;
	ctx.animation = &m_animation;
	ctx.texCache = &m_texCache;
	ctx.playingEntry = &m_playingEntry;
	ctx.previewModelIndex = &m_previewModelIdx;
	ctx.selectedAssetIdx = m_selectedAsset;

	ImGui::SetNextWindowPos(ImVec2(panelW, 20));
	ImGui::SetNextWindowSize(ImVec2(inspectorW, availH));
	ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	{
		if(m_selectedAsset < 0) {
			ImGui::TextDisabled("Select an asset from the browser panel.");
		} else {
			const auto &e = assets[m_selectedAsset];
			ImGui::Text("File: %s", e.relativePath.c_str());
			ImGui::Text("Type: .%s | Size: %s", e.extension.c_str(), formatSize(e.fileSize).c_str());
			ImGui::Separator();
			if(e.isFailed) {
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Parse failed.");
			}
			else {
				viewer::ui::drawInspector(m_activeParser, ctx);
			}
		}
	}
	ImGui::End();

	if(showDetails) {
		ImGui::SetNextWindowPos(ImVec2(panelW + inspectorW, 20));
		ImGui::SetNextWindowSize(ImVec2(detailsW, availH));
		ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		{
			viewer::ui::drawDetailsPane(m_activeParser, ctx);
		}
		ImGui::End();
	}
}

} // namespace viewer::app
