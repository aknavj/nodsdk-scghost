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



#include "render/TextureUpload.h"

#include "parsers/DDSParser.h"
#include "parsers/TGAParser.h"

#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace viewer::render {

/*
 */
GLuint uploadDDS(const DDSParser *dds) noexcept {
	if(!dds) return 0;
	const auto &tex = dds->getTextureData();
	const auto &h = tex.header;

	if(!tex.pixel_data || tex.pixel_data_size == 0 || h.width == 0 || h.height == 0) return 0;

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	const bool compressed = (h.pixel_format_flags & 0x4) != 0;

	if(compressed) {
		GLenum   fmt;
		uint32_t blockSize;
		switch(h.four_cc) {
			case 0x31545844: fmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; blockSize = 8;  break; // DXT1
			case 0x33545844: fmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; blockSize = 16; break; // DXT3
			case 0x35545844: fmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; blockSize = 16; break; // DXT5
			default: glDeleteTextures(1, &texId); return 0;
		}

		uint32_t mw = h.width, mh = h.height;
		uint32_t offset = 0;
		uint32_t levels = h.mip_map_count > 0 ? h.mip_map_count : 1;
		uint32_t uploaded = 0;

		for(uint32_t lev = 0; lev < levels; ++lev) {
			const uint32_t sz = ((mw + 3) / 4) * ((mh + 3) / 4) * blockSize;
			if(offset + sz > tex.pixel_data_size) break;
			glCompressedTexImage2D(GL_TEXTURE_2D, lev, fmt, mw, mh, 0, sz, tex.pixel_data + offset);
			offset += sz;
			++uploaded;
			mw = mw > 1 ? mw / 2 : 1;
			mh = mh > 1 ? mh / 2 : 1;
		}

		if(uploaded > 1) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, uploaded - 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, uploaded > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	} else {
		GLenum glFmt, glInternal;
		if(h.rgb_bit_count == 32) {
			glFmt = (h.r_bit_mask == 0x00ff0000) ? GL_BGRA : GL_RGBA;
			glInternal = GL_RGBA8;
		} else if(h.rgb_bit_count == 24) {
			glFmt = GL_RGB;  glInternal = GL_RGB8;
		} else if(h.rgb_bit_count == 8) {
			glFmt = GL_RED;  glInternal = GL_R8;
		} else {
			glDeleteTextures(1, &texId);
			return 0;
		}
		glTexImage2D(GL_TEXTURE_2D, 0, glInternal, h.width, h.height, 0, glFmt, GL_UNSIGNED_BYTE, tex.pixel_data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
	return texId;
}

/*
 */
GLuint uploadTGA(const TGAParser *tga) noexcept {
	if(!tga) return 0;
	const auto &img = tga->getImageData();
	const auto &hdr = img.header;

	if(!img.pixel_data || img.pixel_data_size == 0 || hdr.width == 0 || hdr.height == 0) return 0;

	const int w = hdr.width;
	const int h = hdr.height;
	const int bpp = hdr.bit_depth / 8;
	const int n = w * h;

	uint8_t *rgba = static_cast<uint8_t *>(std::malloc(n * 4));
	if(!rgba) return 0;

	const bool topOrigin = (hdr.image_descriptor & 0x20) != 0;

	for(int y = 0; y < h; ++y) {
		const int srcY = topOrigin ? y : (h - 1 - y);
		for(int x = 0; x < w; ++x) {
			const int srcIdx = (srcY * w + x) * bpp;
			const int dstIdx = (y * w + x) * 4;
			if(srcIdx + bpp > static_cast<int>(img.pixel_data_size)) {
				rgba[dstIdx + 0] = rgba[dstIdx + 1] = rgba[dstIdx + 2] = 0;
				rgba[dstIdx + 3] = 255;
				continue;
			}
			if(bpp >= 3) {
				rgba[dstIdx + 0] = img.pixel_data[srcIdx + 2];
				rgba[dstIdx + 1] = img.pixel_data[srcIdx + 1];
				rgba[dstIdx + 2] = img.pixel_data[srcIdx + 0];
				rgba[dstIdx + 3] = bpp >= 4 ? img.pixel_data[srcIdx + 3] : 255;
			} else if(bpp == 2) {
				const uint16_t p = img.pixel_data[srcIdx] | (img.pixel_data[srcIdx + 1] << 8);
				rgba[dstIdx + 0] = static_cast<uint8_t>(((p >> 10) & 0x1F) * 255 / 31);
				rgba[dstIdx + 1] = static_cast<uint8_t>(((p >>  5) & 0x1F) * 255 / 31);
				rgba[dstIdx + 2] = static_cast<uint8_t>(( p        & 0x1F) * 255 / 31);
				rgba[dstIdx + 3] = (p & 0x8000) ? 255 : 0;
			} else {
				rgba[dstIdx + 0] = rgba[dstIdx + 1] = rgba[dstIdx + 2] = img.pixel_data[srcIdx];
				rgba[dstIdx + 3] = 255;
			}
		}
	}

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	std::free(rgba);
	return texId;
}

/*
 */
void drawTexturePreview(GLuint texId, int texW, int texH) {
	if(!texId || texW <= 0 || texH <= 0) return;
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float maxW = avail.x, maxH = avail.y;
	if(maxW < 32 || maxH < 32) return;

	float scale = std::min(maxW / static_cast<float>(texW), maxH / static_cast<float>(texH));
	if(scale > 2.0f) scale = 2.0f;

	const float dw = texW * scale;
	const float dh = texH * scale;

	const float offX = (maxW - dw) * 0.5f;
	if(offX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offX);

	ImGui::Image(static_cast<ImTextureID>(texId), ImVec2(dw, dh));
}

} // namespace viewer::render
