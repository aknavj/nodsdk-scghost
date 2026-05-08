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

#include "render/PreviewRenderer.h"
#include "render/Shaders.h"
#include "render/TextureUpload.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cstring>
#include <algorithm>

#include "parsers/NMBParser.h"
#include "parsers/NOCParser.h"

namespace viewer::render {

/*
 */
bool PreviewRenderer::init() {
	m_prog = buildProgram(kPreviewVertexShader, kPreviewFragmentShader);
	if(!m_prog) return false;

	m_locMVP = glGetUniformLocation(m_prog, "uMVP");
	m_locModel = glGetUniformLocation(m_prog, "uModel");
	m_locNormalMat = glGetUniformLocation(m_prog, "uNormalMat");
	m_locLightDir = glGetUniformLocation(m_prog, "uLightDir");
	m_locViewPos = glGetUniformLocation(m_prog, "uViewPos");
	m_locColor = glGetUniformLocation(m_prog, "uColor");
	m_locWireframe = glGetUniformLocation(m_prog, "uWireframe");
	m_locHasTexture = glGetUniformLocation(m_prog, "uHasTexture");
	m_locDiffuseTex = glGetUniformLocation(m_prog, "uDiffuseTex");
	m_locBakedLighting = glGetUniformLocation(m_prog, "uBakedLighting");

	buildGrid();
	std::printf("[PreviewRenderer] Initialized\n");
	return true;
}

/*
 */
void PreviewRenderer::shutdown() {
	clearMesh();
	if(m_gridVao) { glDeleteVertexArrays(1, &m_gridVao); m_gridVao = 0; }
	if(m_gridVbo) { glDeleteBuffers(1, &m_gridVbo); m_gridVbo = 0; }
	if(m_fbo)      { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
	if(m_fboColor) { glDeleteTextures(1, &m_fboColor); m_fboColor = 0; }
	if(m_fboDepth) { glDeleteRenderbuffers(1, &m_fboDepth); m_fboDepth = 0; }
	if(m_prog)     { glDeleteProgram(m_prog); m_prog = 0; }
}

/*
 */
GLuint PreviewRenderer::uploadDDS(const DDSParser *dds) { return render::uploadDDS(dds); }
GLuint PreviewRenderer::uploadTGA(const TGAParser *tga) { return render::uploadTGA(tga); }

/*
 */
void PreviewRenderer::drawTexturePreview(GLuint texId, int texW, int texH) {
	render::drawTexturePreview(texId, texW, texH);
}

/*
 */
void PreviewRenderer::ensureFbo(int w, int h) {
	if(w < 1) w = 1;
	if(h < 1) h = 1;
	if(m_fbo && m_fboW == w && m_fboH == h) return;

	if(m_fbo)      glDeleteFramebuffers(1, &m_fbo);
	if(m_fboColor) glDeleteTextures(1, &m_fboColor);
	if(m_fboDepth) glDeleteRenderbuffers(1, &m_fboDepth);

	m_fboW = w; m_fboH = h;

	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	glGenTextures(1, &m_fboColor);
	glBindTexture(GL_TEXTURE_2D, m_fboColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboColor, 0);

	glGenRenderbuffers(1, &m_fboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, m_fboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_fboDepth);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/*
 */
void PreviewRenderer::buildGrid() {
	const int half = 10;
	std::vector<float> lines;
	lines.reserve((half * 2 + 1) * 4 * 6);

	auto push = [&](float x, float y, float z, float r, float g, float b) {
		lines.push_back(x); lines.push_back(y); lines.push_back(z);
		lines.push_back(r); lines.push_back(g); lines.push_back(b);
	};
	for(int i = -half; i <= half; ++i) {
		const float t = static_cast<float>(i);
		push(t, 0, -static_cast<float>(half),  0, 1, 0);
		push(t, 0,  static_cast<float>(half),  0, 1, 0);
		push(-static_cast<float>(half), 0, t,  0, 1, 0);
		push( static_cast<float>(half), 0, t,  0, 1, 0);
	}

	m_gridVertCount = static_cast<int>(lines.size() / 6);

	glGenVertexArrays(1, &m_gridVao);
	glGenBuffers(1, &m_gridVbo);
	glBindVertexArray(m_gridVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
	glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<void *>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<void *>(12));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

/*
 */
bool PreviewRenderer::buildMesh(const MeshBuildInfo &info) {
	clearMesh();

	if(!info.vertex_data || info.vertex_data_size == 0) return false;
	if(!info.indices || info.index_count == 0)          return false;
	if(!info.mesh_groups || info.mesh_group_count == 0) return false;
	if(info.vertex_set_count == 0)                       return false;

	uint32_t totalVerts = 0;
	uint32_t vsOffset[NODParser::MAX_VERTEX_SETS] = {};
	for(uint32_t i = 0; i < info.vertex_set_count && i < NODParser::MAX_VERTEX_SETS; ++i) {
		vsOffset[i] = totalVerts;
		totalVerts += info.vertex_sets[i].count;
	}
	if(totalVerts == 0) return false;

	const uint32_t floatsPerVert = 8; // pos(3) + norm(3) + uv(2)
	std::vector<float> vbuf(totalVerts * floatsPerVert, 0.0f);

	for(uint32_t vs = 0; vs < info.vertex_set_count && vs < NODParser::MAX_VERTEX_SETS; ++vs) {
		const auto &set = info.vertex_sets[vs];

		uint32_t stride = set.base_stride;
		if(!stride) stride = set.stride;
		if(!stride) {
			switch(set.type) {
				case 0: case 3: stride = 32; break;
				case 1: case 4: stride = 36; break;
				case 2: case 5: stride = 48; break;
				default:        stride = 32; break;
			}
		}
		if(stride < 24) continue;

		const uint8_t *base = info.vertex_data + set.data_offset;
		const std::size_t maxBytes =
		    info.vertex_data_size > set.data_offset ? info.vertex_data_size - set.data_offset : 0;

		const bool hasUV = (stride >= 32);

		for(uint32_t v = 0; v < set.count; ++v) {
			const std::size_t byteOff = static_cast<std::size_t>(v) * stride;
			if(byteOff + 24 > maxBytes) break;

			const float *src = reinterpret_cast<const float *>(base + byteOff);
			const uint32_t idx = (vsOffset[vs] + v) * floatsPerVert;

			// D3D -> GL: (x,y,z) -> (x,z,-y)
			vbuf[idx + 0] = src[0];
			vbuf[idx + 1] = src[2];
			vbuf[idx + 2] = -src[1];
			vbuf[idx + 3] = src[3];
			vbuf[idx + 4] = src[5];
			vbuf[idx + 5] = -src[4];
			if(hasUV && byteOff + 32 <= maxBytes) {
				vbuf[idx + 6] = src[6];
				vbuf[idx + 7] = src[7];
			}
		}
	}

	std::vector<uint32_t> ibuf;
	ibuf.reserve(info.index_count);
	m_drawGroups.clear();

	for(uint32_t g = 0; g < info.mesh_group_count; ++g) {
		const auto &mg = info.mesh_groups[g];
		const auto &lod = mg.lods[0];
		const uint32_t vsIdx = mg.vertex_set_index < info.vertex_set_count ? mg.vertex_set_index : 0;
		const uint32_t groupBase = vsOffset[vsIdx] + mg.base_vertex;
		const uint32_t groupStart = static_cast<uint32_t>(ibuf.size());

		// Triangle list
		for(uint32_t i = 0; i < lod.list_count; ++i) {
			const uint32_t si = lod.list_start + i;
			if(si >= info.index_count) break;
			const uint32_t vi = groupBase + info.indices[si];
			if(vi < totalVerts) ibuf.push_back(vi);
		}
		// Triangle strip
		if(lod.strip_count >= 3) {
			for(uint32_t i = 0; i + 2 < static_cast<uint32_t>(lod.strip_count); ++i) {
				const uint32_t ai = lod.strip_start + i;
				const uint32_t bi = lod.strip_start + i + 1;
				const uint32_t ci = lod.strip_start + i + 2;
				if(ci >= info.index_count) break;
				const uint16_t ra = info.indices[ai];
				const uint16_t rb = info.indices[bi];
				const uint16_t rc = info.indices[ci];
				if(ra == 0xFFFF || rb == 0xFFFF || rc == 0xFFFF) continue;
				if(ra == rb || rb == rc || ra == rc) continue;
				const uint32_t a = groupBase + ra;
				const uint32_t b = groupBase + rb;
				const uint32_t c = groupBase + rc;
				if(a >= totalVerts || b >= totalVerts || c >= totalVerts) continue;
				if(i & 1) { ibuf.push_back(b); ibuf.push_back(a); ibuf.push_back(c); }
				else      { ibuf.push_back(a); ibuf.push_back(b); ibuf.push_back(c); }
			}
		}

		const uint32_t cnt = static_cast<uint32_t>(ibuf.size()) - groupStart;
		if(cnt > 0) {
			DrawGroup dg{};
			dg.index_offset = static_cast<GLsizei>(groupStart * sizeof(uint32_t));
			dg.index_count = static_cast<GLsizei>(cnt);
			dg.diffuse_tex = (info.group_textures && g < info.group_texture_count) ? info.group_textures[g] : 0;
			dg.use_vcol = false;
			m_drawGroups.push_back(dg);
		}
	}

	if(ibuf.empty()) return false;

	// CPU skinning prep
	m_hasSkinning = false;
	m_skinVerts.clear();
	if(info.bones && info.bone_count > 0) {
		m_skinVerts.resize(totalVerts);

		for(uint32_t vs = 0; vs < info.vertex_set_count && vs < NODParser::MAX_VERTEX_SETS; ++vs) {
			const auto &set = info.vertex_sets[vs];
			uint32_t stride = set.base_stride ? set.base_stride : set.stride;
			if(!stride) continue;
			const uint8_t *base = info.vertex_data + set.data_offset;
			const bool hasUV = (stride >= 32);
			for(uint32_t v = 0; v < set.count; ++v) {
				const std::size_t byteOff = static_cast<std::size_t>(v) * stride;
				if(byteOff + 12 > info.vertex_data_size) break;
				const float *src = reinterpret_cast<const float *>(base + byteOff);
				auto &sv = m_skinVerts[vsOffset[vs] + v];
				sv.bind_pos = glm::vec3(src[0], src[1], src[2]);
				sv.bind_norm = glm::vec3(src[3], src[4], src[5]);
				sv.uv = (hasUV && byteOff + 32 <= info.vertex_data_size)
				          ? glm::vec2(src[6], src[7]) : glm::vec2(0.0f);
				sv.bone_idx[0] = sv.bone_idx[1] = sv.bone_idx[2] = sv.bone_idx[3] = 0;
				sv.bone_w[0] = 1.0f; sv.bone_w[1] = sv.bone_w[2] = sv.bone_w[3] = 0.0f;
			}
		}

		for(uint32_t g = 0; g < info.mesh_group_count; ++g) {
			const auto &mg = info.mesh_groups[g];
			const uint32_t vsIdx = mg.vertex_set_index < info.vertex_set_count ? mg.vertex_set_index : 0;
			const auto &set = info.vertex_sets[vsIdx];
			const uint32_t stride = set.base_stride ? set.base_stride : set.stride;
			const uint32_t gBase = vsOffset[vsIdx] + mg.base_vertex;
			const uint32_t nV = mg.lods[0].num_verts;
			const uint8_t *raw = info.vertex_data + set.data_offset;
			const uint32_t baseType = (set.type >= 3 && set.type <= 5) ? set.type - 3 : set.type;

			for(uint32_t v = 0; v < nV; ++v) {
				const uint32_t ui = gBase + v;
				if(ui >= m_skinVerts.size()) break;
				auto &sv = m_skinVerts[ui];
				const std::size_t vbase = static_cast<std::size_t>(mg.base_vertex + v) * stride;

				if(baseType == 1 && stride >= 36) {
					if(vbase + 33 <= static_cast<std::size_t>(set.count) * stride) {
						const uint8_t li = raw[vbase + 32];
						sv.bone_idx[0] = (li < mg.num_bones) ? mg.a_bones[li] : 0;
						sv.bone_w[0] = 1.0f;
						sv.bone_idx[1] = sv.bone_idx[2] = sv.bone_idx[3] = sv.bone_idx[0];
						sv.bone_w[1] = sv.bone_w[2] = sv.bone_w[3] = 0.0f;
					}
				} else if(baseType == 2 && stride >= 48) {
					if(vbase + 48 <= static_cast<std::size_t>(set.count) * stride) {
						float w0, w1, w2;
						std::memcpy(&w0, raw + vbase + 32, 4);
						std::memcpy(&w1, raw + vbase + 36, 4);
						std::memcpy(&w2, raw + vbase + 40, 4);
						float w3 = 1.0f - w0 - w1 - w2;
						if(w3 < 0.0f) w3 = 0.0f;
						const uint8_t li0 = raw[vbase + 44];
						const uint8_t li1 = raw[vbase + 45];
						const uint8_t li2 = raw[vbase + 46];
						const uint8_t li3 = raw[vbase + 47];
						sv.bone_idx[0] = (li0 < mg.num_bones) ? mg.a_bones[li0] : 0;
						sv.bone_idx[1] = (li1 < mg.num_bones) ? mg.a_bones[li1] : 0;
						sv.bone_idx[2] = (li2 < mg.num_bones) ? mg.a_bones[li2] : 0;
						sv.bone_idx[3] = (li3 < mg.num_bones) ? mg.a_bones[li3] : 0;
						sv.bone_w[0] = w0; sv.bone_w[1] = w1; sv.bone_w[2] = w2; sv.bone_w[3] = w3;
					}
				} else {
					sv.bone_idx[0] = (mg.num_bones > 0) ? mg.a_bones[0] : 0;
					sv.bone_w[0] = 1.0f;
					sv.bone_idx[1] = sv.bone_idx[2] = sv.bone_idx[3] = sv.bone_idx[0];
					sv.bone_w[1] = sv.bone_w[2] = sv.bone_w[3] = 0.0f;
				}
			}
		}
		m_hasSkinning = true;
	}

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, vbuf.size() * sizeof(float), vbuf.data(), m_hasSkinning ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

	const GLsizei vstride = floatsPerVert * sizeof(float);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void *>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void *>(12));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vstride, reinterpret_cast<void *>(24));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibuf.size() * sizeof(uint32_t), ibuf.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	m_idxCount = static_cast<GLsizei>(ibuf.size());
	m_hasMesh = true;

	const glm::vec3 a(info.bounds_min.x, info.bounds_min.z, -info.bounds_min.y);
	const glm::vec3 b(info.bounds_max.x, info.bounds_max.z, -info.bounds_max.y);
	m_camera.fitTo(glm::min(a, b), glm::max(a, b));

	std::printf("[PreviewRenderer] Mesh: %u verts, %u tris, %zu groups\n", totalVerts, static_cast<uint32_t>(ibuf.size() / 3), m_drawGroups.size());
	return true;
}

/*
 */
bool PreviewRenderer::buildMeshSimple(const SimpleMeshBuildInfo &info) {
	clearMesh();
	if(!info.vertex_data || info.vertex_count == 0) return false;
	if(!info.indices || info.index_count == 0)      return false;
	if(info.vertex_stride < info.pos_offset    + 12) return false;
	if(info.vertex_stride < info.normal_offset + 12) return false;

	const uint32_t nv = info.vertex_count;
	std::vector<float> vbuf(nv * 6, 0.0f);

	const uint8_t *raw = static_cast<const uint8_t *>(info.vertex_data);
	for(uint32_t v = 0; v < nv; ++v) {
		const float *pos = reinterpret_cast<const float *>(raw + static_cast<std::size_t>(v) * info.vertex_stride + info.pos_offset);
		const float *norm = reinterpret_cast<const float *>(raw + static_cast<std::size_t>(v) * info.vertex_stride + info.normal_offset);
		const uint32_t idx = v * 6;
		vbuf[idx + 0] = pos[0];  vbuf[idx + 1] = pos[2];  vbuf[idx + 2] = -pos[1];
		vbuf[idx + 3] = norm[0]; vbuf[idx + 4] = norm[2]; vbuf[idx + 5] = -norm[1];
	}

	std::vector<uint32_t> ibuf;
	ibuf.reserve(info.index_count);

	if(info.is_strip) {
		for(uint32_t i = 0; i + 2 < info.index_count; ++i) {
			const uint16_t ra = info.indices[i];
			const uint16_t rb = info.indices[i + 1];
			const uint16_t rc = info.indices[i + 2];
			if(ra == 0xFFFF || rb == 0xFFFF || rc == 0xFFFF) continue;
			if(ra == rb || rb == rc || ra == rc) continue;
			if(ra >= nv || rb >= nv || rc >= nv) continue;
			if(i & 1) { ibuf.push_back(rb); ibuf.push_back(ra); ibuf.push_back(rc); }
			else      { ibuf.push_back(ra); ibuf.push_back(rb); ibuf.push_back(rc); }
		}
	} else {
		for(uint32_t i = 0; i < info.index_count; ++i) {
			const uint32_t vi = info.indices[i];
			if(vi < nv) ibuf.push_back(vi);
		}
	}
	if(ibuf.empty()) return false;

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, vbuf.size() * sizeof(float), vbuf.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<void *>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 24, reinterpret_cast<void *>(12));
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibuf.size() * sizeof(uint32_t), ibuf.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	m_idxCount = static_cast<GLsizei>(ibuf.size());
	m_hasMesh = true;

	const glm::vec3 a(info.bounds_min.x, info.bounds_min.z, -info.bounds_min.y);
	const glm::vec3 b(info.bounds_max.x, info.bounds_max.z, -info.bounds_max.y);
	m_camera.fitTo(glm::min(a, b), glm::max(a, b));

	std::printf("[PreviewRenderer] SimpleMesh: %u verts, %u tris\n", nv, static_cast<uint32_t>(ibuf.size() / 3));
	return true;
}

/*
 */
bool PreviewRenderer::buildMeshNIL(const NILParser *nil, const GLuint *groupTextures, uint32_t groupTextureCount) {
	clearMesh();
	if(!nil) return false;
	const auto &lev = nil->getLevelData();
	if(!lev.sectors || lev.num_sectors == 0) return false;
	if(lev.total_verts == 0) return false;

	uint32_t totalVerts = 0;
	for(uint32_t si = 0; si < lev.num_sectors; ++si) totalVerts += lev.sectors[si].num_verts;
	if(totalVerts == 0) return false;

	constexpr uint32_t NFLOAT = 11;
	constexpr uint32_t BSTRIDE = NFLOAT * sizeof(float);
	constexpr uint32_t NIL_VSTRIDE = 36;

	std::vector<float>    vbuf(totalVerts * NFLOAT, 0.0f);
	std::vector<uint32_t> ibuf;
	ibuf.reserve(lev.total_indices);
	m_drawGroups.clear();

	glm::vec3 allMin( 1e18f), allMax(-1e18f);
	uint32_t  vertBase = 0;
	uint32_t  groupIdx = 0;

	for(uint32_t si = 0; si < lev.num_sectors; ++si) {
		const auto &s = lev.sectors[si];

		if(!s.vertex_data || s.num_verts == 0) {
			vertBase += s.num_verts;
			continue;
		}

		for(uint16_t vi = 0; vi < s.num_verts; ++vi) {
			const uint8_t *vp = s.vertex_data + static_cast<std::size_t>(vi) * NIL_VSTRIDE;
			float px, py, pz, nx, ny, nz, u, v;
			std::memcpy(&px, vp + 0,  4);  std::memcpy(&py, vp + 4,  4);  std::memcpy(&pz, vp + 8,  4);
			std::memcpy(&nx, vp + 12, 4);  std::memcpy(&ny, vp + 16, 4);  std::memcpy(&nz, vp + 20, 4);
			const uint8_t dr = vp[24], dg = vp[25], db = vp[26];
			std::memcpy(&u,  vp + 28, 4);  std::memcpy(&v,  vp + 32, 4);

			const uint32_t idx = (vertBase + vi) * NFLOAT;
			vbuf[idx + 0] = px;  vbuf[idx + 1] = pz;  vbuf[idx + 2] = -py;
			vbuf[idx + 3] = nx;  vbuf[idx + 4] = nz;  vbuf[idx + 5] = -ny;
			vbuf[idx + 6] = u;   vbuf[idx + 7] = v;
			vbuf[idx + 8] = static_cast<float>(dr) / 128.0f;
			vbuf[idx + 9] = static_cast<float>(dg) / 128.0f;
			vbuf[idx + 10] = static_cast<float>(db) / 128.0f;

			const glm::vec3 gp(px, pz, -py);
			allMin = glm::min(allMin, gp);
			allMax = glm::max(allMax, gp);
		}

		if(s.indices && s.num_indices > 0) {
			for(uint16_t gi = 0; gi < s.num_groups; ++gi) {
				const auto &g = s.groups[gi];
				const uint32_t nStrip = g.num_strip_indices;
				const uint32_t nList = g.num_list_indices;
				const uint32_t base = g.start_idx;

				uint32_t maxTris = 0;
				if(nStrip >= 3) maxTris += nStrip - 2;
				maxTris += nList / 3;
				if(maxTris == 0) { ++groupIdx; continue; }

				const uint32_t ibufStart = static_cast<uint32_t>(ibuf.size());

				if(nStrip >= 3) {
					for(uint32_t n = 0; n + 2 < nStrip; ++n) {
						const uint32_t i0 = base + n, i1 = base + n + 1, i2 = base + n + 2;
						if(i2 >= s.num_indices) break;
						const uint16_t ra = s.indices[i0], rb = s.indices[i1], rc = s.indices[i2];
						if(ra == 0xFFFF || rb == 0xFFFF || rc == 0xFFFF) continue;
						if(ra == rb || rb == rc || ra == rc) continue;
						const uint32_t a = vertBase + ra, b = vertBase + rb, c = vertBase + rc;
						if(a >= totalVerts || b >= totalVerts || c >= totalVerts) continue;
						if(n & 1) { ibuf.push_back(a); ibuf.push_back(c); ibuf.push_back(b); }
						else      { ibuf.push_back(a); ibuf.push_back(b); ibuf.push_back(c); }
					}
				}

				const uint32_t listStart = base + nStrip;
				for(uint32_t n = 0; n + 2 < nList; n += 3) {
					const uint32_t i0 = listStart + n, i1 = listStart + n + 1, i2 = listStart + n + 2;
					if(i2 >= s.num_indices) break;
					const uint16_t r0 = s.indices[i0], r1 = s.indices[i1], r2 = s.indices[i2];
					if(r0 == r1 || r1 == r2 || r0 == r2) continue;
					const uint32_t a = vertBase + r0, b = vertBase + r1, c = vertBase + r2;
					if(a >= totalVerts || b >= totalVerts || c >= totalVerts) continue;
					ibuf.push_back(a); ibuf.push_back(b); ibuf.push_back(c);
				}

				const uint32_t ibufEnd = static_cast<uint32_t>(ibuf.size());
				if(ibufEnd == ibufStart) { ++groupIdx; continue; }

				DrawGroup dg{};
				dg.index_offset = static_cast<GLsizei>(ibufStart * sizeof(uint32_t));
				dg.index_count = static_cast<GLsizei>(ibufEnd - ibufStart);
				dg.diffuse_tex = (groupTextures && groupIdx < groupTextureCount) ? groupTextures[groupIdx] : 0;
				dg.use_vcol = (g.vert_type == 2);
				m_drawGroups.push_back(dg);
				++groupIdx;
			}
		}

		vertBase += s.num_verts;
	}

	if(ibuf.empty()) return false;

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, vbuf.size() * sizeof(float), vbuf.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(12));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(24));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(32));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibuf.size() * sizeof(uint32_t), ibuf.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	m_idxCount = static_cast<GLsizei>(ibuf.size());
	m_hasMesh = true;
	m_camera.fitTo(allMin, allMax);

	uint32_t resolved = 0;
	for(const auto &dg : m_drawGroups) if(dg.diffuse_tex) ++resolved;
	std::printf("[PreviewRenderer] NIL: %u sectors, %u verts, %zu groups (%u textured), %u tris\n", lev.num_sectors, totalVerts, m_drawGroups.size(), resolved, static_cast<uint32_t>(ibuf.size() / 3));
	return true;
}

/*
 */
bool PreviewRenderer::buildEntityGeometry(const float *vbuf, uint32_t vcount, const uint32_t *ibuf, uint32_t icount, const DrawGroup *groups, uint32_t groupCount) {
	clearEntityMesh();
	if(!vbuf || vcount == 0 || !ibuf || icount == 0 || !groups || groupCount == 0) return false;

	constexpr uint32_t NFLOAT = 11;
	constexpr uint32_t BSTRIDE = NFLOAT * sizeof(float);

	glGenVertexArrays(1, &m_entityVao);
	glGenBuffers(1, &m_entityVbo);
	glGenBuffers(1, &m_entityEbo);
	glBindVertexArray(m_entityVao);

	glBindBuffer(GL_ARRAY_BUFFER, m_entityVbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vcount * NFLOAT * sizeof(float)), vbuf, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(12));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(24));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, BSTRIDE, reinterpret_cast<void *>(32));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_entityEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(icount * sizeof(uint32_t)), ibuf, GL_STATIC_DRAW);
	glBindVertexArray(0);

	m_entityDrawGroups.assign(groups, groups + groupCount);
	m_hasEntityMesh = true;

	std::printf("[PreviewRenderer] Entity mesh: %u verts, %u tris, %u groups\n", vcount, icount / 3, groupCount);
	return true;
}

/*
 */
void PreviewRenderer::updateSkinning(const std::vector<glm::mat4> &boneSkinMats) {
	if(!m_hasSkinning || m_skinVerts.empty() || !m_vbo) return;

	constexpr uint32_t floatsPerVert = 8;
	std::vector<float> vbuf(m_skinVerts.size() * floatsPerVert);

	for(std::size_t i = 0; i < m_skinVerts.size(); ++i) {
		const auto &sv = m_skinVerts[i];
		const uint32_t idx = static_cast<uint32_t>(i * floatsPerVert);

		glm::vec4 p(0.0f), n(0.0f);
		for(int b = 0; b < 4; ++b) {
			const float w = sv.bone_w[b];
			if(w <= 0.0f) continue;
			if(static_cast<std::size_t>(sv.bone_idx[b]) >= boneSkinMats.size()) continue;
			const glm::mat4 &M = boneSkinMats[sv.bone_idx[b]];
			p += w * (M * glm::vec4(sv.bind_pos,  1.0f));
			n += w * (M * glm::vec4(sv.bind_norm, 0.0f));
		}

		vbuf[idx + 0] = p.x; vbuf[idx + 1] = p.z; vbuf[idx + 2] = -p.y;
		vbuf[idx + 3] = n.x; vbuf[idx + 4] = n.z; vbuf[idx + 5] = -n.y;
		vbuf[idx + 6] = sv.uv.x; vbuf[idx + 7] = sv.uv.y;
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vbuf.size() * sizeof(float), vbuf.data());
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*
 */
void PreviewRenderer::clearMesh() {
	if(m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
	if(m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
	if(m_ebo) { glDeleteBuffers(1, &m_ebo); m_ebo = 0; }
	m_idxCount = 0;
	m_hasMesh = false;
	m_hasSkinning = false;
	m_skinVerts.clear();
	m_drawGroups.clear();
	clearGroupTextures();
	clearEntityMesh();
}

/*
 */
void PreviewRenderer::clearEntityMesh() {
	if(m_entityVao) { glDeleteVertexArrays(1, &m_entityVao); m_entityVao = 0; }
	if(m_entityVbo) { glDeleteBuffers(1, &m_entityVbo); m_entityVbo = 0; }
	if(m_entityEbo) { glDeleteBuffers(1, &m_entityEbo); m_entityEbo = 0; }
	m_entityDrawGroups.clear();
	m_hasEntityMesh = false;
}

/*
 */
void PreviewRenderer::clearGroupTextures() {
	for(GLuint t : m_ownedTextures) if(t) glDeleteTextures(1, &t);
	m_ownedTextures.clear();
}

/*
 */
void PreviewRenderer::renderScene(int w, int h) {
	ensureFbo(w, h);

	GLint prevFbo;       glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
	GLint prevViewport[4]; glGetIntegerv(GL_VIEWPORT, prevViewport);
	GLboolean prevDepth; glGetBooleanv(GL_DEPTH_TEST, &prevDepth);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, w, h);
	glClearColor(0.15f, 0.15f, 0.19f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	const Camera::Frame f = m_camera.frame();
	const float aspect = static_cast<float>(w) / static_cast<float>(h);
	float nearClip = f.refDist * 0.001f;  if(nearClip < 0.001f) nearClip = 0.001f;
	const float farClip = f.refDist * 20.0f;
	const glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, nearClip, farClip);

	glUseProgram(m_prog);
	glUniform3f(m_locLightDir, -0.3f, -0.7f, -0.5f);
	glUniform3f(m_locViewPos, f.pos.x, f.pos.y, f.pos.z);

	// Grid
	if(m_gridVao) {
		const float extent = glm::length(m_camera.boundsMax - m_camera.boundsMin);
		const float gridScale = extent > 0.001f ? extent * 0.15f : 1.0f;
		const glm::vec3 gridCenter(m_camera.target.x, m_camera.boundsMin.y, m_camera.target.z);
		glm::mat4 gridModel = glm::translate(glm::mat4(1.0f), gridCenter);
		gridModel = glm::scale(gridModel, glm::vec3(gridScale));
		const glm::mat4 gridMvp = proj * f.view * gridModel;
		const glm::mat3 gridNm = glm::mat3(1.0f);

		glUniformMatrix4fv(m_locMVP,       1, GL_FALSE, glm::value_ptr(gridMvp));
		glUniformMatrix4fv(m_locModel,     1, GL_FALSE, glm::value_ptr(gridModel));
		glUniformMatrix3fv(m_locNormalMat, 1, GL_FALSE, glm::value_ptr(gridNm));
		glUniform3f(m_locColor, 0.35f, 0.35f, 0.4f);
		glUniform1i(m_locWireframe, 1);
		glUniform1i(m_locHasTexture, 0);
		glUniform1i(m_locBakedLighting, 0);

		glBindVertexArray(m_gridVao);
		glDrawArrays(GL_LINES, 0, m_gridVertCount);
		glBindVertexArray(0);
	}

	auto renderGroups = [&](GLuint vao, const std::vector<DrawGroup> &groups, GLsizei fallbackCount) {
		const glm::mat4 model = glm::mat4(1.0f);
		const glm::mat4 mvp = proj * f.view * model;
		const glm::mat3 nmat = glm::mat3(1.0f);

		glUniformMatrix4fv(m_locMVP,       1, GL_FALSE, glm::value_ptr(mvp));
		glUniformMatrix4fv(m_locModel,     1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix3fv(m_locNormalMat, 1, GL_FALSE, glm::value_ptr(nmat));
		glUniform1i(m_locWireframe, wireframe ? 1 : 0);

		if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBindVertexArray(vao);

		if(!groups.empty()) {
			for(const auto &dg : groups) {
				glUniform1i(m_locBakedLighting, (dg.use_vcol && !wireframe) ? 1 : 0);
				if(dg.diffuse_tex && !wireframe) {
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, dg.diffuse_tex);
					glUniform1i(m_locDiffuseTex, 0);
					glUniform1i(m_locHasTexture, 1);
					glUniform3f(m_locColor, 1.0f, 1.0f, 1.0f);
				} else {
					glUniform1i(m_locHasTexture, 0);
					glUniform3f(m_locColor, 0.7f, 0.75f, 0.8f);
				}
				glDrawElements(GL_TRIANGLES, dg.index_count, GL_UNSIGNED_INT, reinterpret_cast<void *>(static_cast<intptr_t>(dg.index_offset)));
			}
			glBindTexture(GL_TEXTURE_2D, 0);
		} else {
			glUniform1i(m_locHasTexture, 0);
			glUniform3f(m_locColor, 0.7f, 0.75f, 0.8f);
			glDrawElements(GL_TRIANGLES, fallbackCount, GL_UNSIGNED_INT, nullptr);
		}

		glBindVertexArray(0);
		if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	};

	if(m_hasMesh && m_vao) renderGroups(m_vao, m_drawGroups, m_idxCount);
	if(m_hasEntityMesh && m_entityVao) renderGroups(m_entityVao, m_entityDrawGroups, 0);

	glUseProgram(0);

	if(!prevDepth) glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

/*
 */
bool PreviewRenderer::drawMeshPreview() {
	if(!m_hasMesh) return false;

	ImGui::Checkbox("Wireframe", &wireframe);
	ImGui::SameLine();
	if(ImGui::Button("Reset Camera")) m_camera.reset();
	ImGui::SameLine();

	bool fpsFlag = (m_camera.mode == CameraMode::Fps);
	const bool toggled = ImGui::Checkbox("FPS Camera", &fpsFlag);
	const CameraMode newMode = fpsFlag ? CameraMode::Fps : CameraMode::Orbital;
	if(toggled && newMode == CameraMode::Fps) m_camera.switchToFps();
	m_camera.mode = newMode;

	ImGui::SameLine();
	if(m_camera.mode == CameraMode::Fps) {
		ImGui::TextDisabled("RMB+drag: look | WASD/QE: move | Scroll: speed");
	}
	else {
		ImGui::TextDisabled("LMB: orbit | MMB: pan | Scroll: zoom");
	}

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const int w = static_cast<int>(avail.x);
	const int h = static_cast<int>(avail.y);
	if(w < 64 || h < 64) return false;

	renderScene(w, h);

	ImGui::Image(static_cast<ImTextureID>(m_fboColor), ImVec2(static_cast<float>(w), static_cast<float>(h)), ImVec2(0, 1), ImVec2(1, 0));

	if(ImGui::IsItemHovered()) {
		ImGuiIO &io = ImGui::GetIO();
		if(m_camera.mode == CameraMode::Fps) {
			if(ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
				m_camera.fpsYaw   += io.MouseDelta.x * 0.3f;
				m_camera.fpsPitch -= io.MouseDelta.y * 0.3f;
				m_camera.fpsPitch = glm::clamp(m_camera.fpsPitch, -89.0f, 89.0f);
			}

			const float extent = glm::length(m_camera.boundsMax - m_camera.boundsMin);
			const float baseSpd = (extent > 0.001f ? extent : 5.0f) * 0.015f;
			static float spdMul = 1.0f;
			if(io.MouseWheel != 0.0f) {
				spdMul = glm::clamp(spdMul * (1.0f + io.MouseWheel * 0.2f), 0.05f, 50.0f);
			}
			const float spd = baseSpd * spdMul;

			const float yr = glm::radians(m_camera.fpsYaw);
			const float pr = glm::radians(m_camera.fpsPitch);
			const glm::vec3 fwd(std::sin(yr) * std::cos(pr), std::sin(pr), std::cos(yr) * std::cos(pr));
			const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
			const glm::vec3 up(0, 1, 0);

			if(ImGui::IsKeyDown(ImGuiKey_W)) m_camera.fpsPos += fwd   * spd;
			if(ImGui::IsKeyDown(ImGuiKey_S)) m_camera.fpsPos -= fwd   * spd;
			if(ImGui::IsKeyDown(ImGuiKey_A)) m_camera.fpsPos -= right * spd;
			if(ImGui::IsKeyDown(ImGuiKey_D)) m_camera.fpsPos += right * spd;
			if(ImGui::IsKeyDown(ImGuiKey_Q)) m_camera.fpsPos -= up    * spd;
			if(ImGui::IsKeyDown(ImGuiKey_E)) m_camera.fpsPos += up    * spd;
		} else {
			if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				m_camera.yaw   -= io.MouseDelta.x * 0.3f;
				m_camera.pitch += io.MouseDelta.y * 0.3f;
				m_camera.pitch = glm::clamp(m_camera.pitch, -89.0f, 89.0f);
			}
			if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
				const float panScale = m_camera.dist * 0.002f;
				const float yawR = glm::radians(m_camera.yaw);
				const glm::vec3 right(std::cos(yawR), 0, -std::sin(yawR));
				m_camera.target   -= right * io.MouseDelta.x * panScale;
				m_camera.target.y += io.MouseDelta.y * panScale;
			}
			if(io.MouseWheel != 0.0f) {
				m_camera.dist *= (1.0f - io.MouseWheel * 0.1f);
				const float extent = glm::length(m_camera.boundsMax - m_camera.boundsMin);
				float lo = extent * 0.05f, hi = extent * 20.0f;
				if(lo < 0.01f) lo = 0.01f;
				if(hi < 10.0f) hi = 100.0f;
				m_camera.dist = glm::clamp(m_camera.dist, lo, hi);
			}
		}
	}

	return true;
}

} // namespace viewer::render
