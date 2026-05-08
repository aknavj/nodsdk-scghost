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

#include "animation/AnimationController.h"
#include "animation/BoneEvaluator.h"
#include "core/AssetDatabase.h"
#include "render/PreviewRenderer.h"

#include "parsers/NMBParser.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <glm/glm.hpp>

/*
 */
namespace viewer::anim {

/*
 */
bool AnimationController::loadAnimation(int assetIdx, const viewer::core::AssetDatabase &db) {
	const auto &entries = db.entries();
	if(assetIdx < 0 || static_cast<std::size_t>(assetIdx) >= entries.size()) return false;

	const std::string path = db.fullPath(assetIdx);
	auto *nad = new NADParser();
	if(!nad->parse(path.c_str())) {
		std::fprintf(stderr, "[AnimationController] Failed to parse NAD: %s\n", path.c_str());
		delete nad;
		return false;
	}

	delete m_active;
	m_active = nad;
	m_assetIdx = assetIdx;
	m_frame = 0.0f;
	m_playing = false;
	return true;
}

/*
 */
void AnimationController::unload() {
	delete m_active;
	m_active = nullptr;
	m_assetIdx = -1;
	m_frame = 0.0f;
	m_playing = false;
}

/*
 */
void AnimationController::tick(double nowSeconds) {
	if(!m_active || !m_playing) { m_lastTime = nowSeconds; return; }

	const auto &clip = m_active->getAnimationClip();
	const float dt = static_cast<float>(nowSeconds - m_lastTime);
	m_lastTime = nowSeconds;

	m_frame += dt * m_fps;
	if(clip.duration > 0.0f) {
		while(m_frame >= clip.duration) m_frame -= clip.duration;
		while(m_frame < 0.0f)           m_frame += clip.duration;
	}
}

/*
 */
void AnimationController::apply(IAssetParser *modelParser, int modelIndex, viewer::render::PreviewRenderer &renderer) {
	if(!m_active || !modelParser) return;

	const auto &clip = m_active->getAnimationClip();
	if(!clip.bone_tracks || clip.num_bone_tracks == 0) return;

	const NODParser::Bone *bones = nullptr;
	uint32_t               count = 0;

	const char *type = modelParser->getFileType();
	if(std::strcmp(type, "NOD") == 0) {
		auto *nod = static_cast<NODParser *>(modelParser);
		bones = nod->getModelData().bones;
		count = nod->getModelData().bone_count;
	} else if(std::strcmp(type, "NMB") == 0) {
		auto *nmb = static_cast<NMBParser *>(modelParser);
		const auto &data = nmb->getBundleData();
		if(modelIndex >= 0 && static_cast<uint32_t>(modelIndex) < data.model_count) {
			bones = data.models[modelIndex].bones;
			count = data.models[modelIndex].bone_count;
		}
	}
	if(!bones || count == 0) return;

	std::vector<glm::mat4> mats;
	computeBoneMatrices(clip, bones, count, m_frame, mats);
	renderer.updateSkinning(mats);
}

} // namespace viewer::anim
