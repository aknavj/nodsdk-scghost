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

// NODSurvey dds <build> <out.json>: every .dds under the build, read by
// DDSParser. For each: the pixel format (four-character code, or bit count
// and masks), mip levels, cube faces, depth, and whether the payload is the
// size those imply (short, exact, or with bytes over). Bytes over are counted
// by amount; what they hold is not known.

#include "DDSParser.h"
#include "report.h"

namespace survey {

	namespace {

		constexpr uint32_t FOURCC	= 0x4;
		constexpr uint32_t CUBEMAP	= 0x200;
		constexpr uint32_t ALLFACES = 0xFC00;
		constexpr uint32_t VOLUME	= 0x200000;

		std::string fourcc(uint32_t code) {
			std::string s;
			for(int i = 0; i < 4; ++i) {
				const char c = char((code >> (8 * i)) & 0xFF);
				s += (c >= 32 && c <= 126) ? c : '?';
			}
			return s;
		}

		// Bytes of one level; 0 when the format is not one this survey sizes.
		uint64_t levelBytes(const DDSParser::Header &h, uint64_t w, uint64_t ht, uint64_t d) {
			if(h.pixel_format_flags & FOURCC) {
				const std::string code = fourcc(h.four_cc);
				uint64_t block		   = 0;
				if(code == "DXT1") block = 8;
				if(code == "DXT2" || code == "DXT3" || code == "DXT4" || code == "DXT5") block = 16;
				return block * std::max<uint64_t>(1, (w + 3) / 4) * std::max<uint64_t>(1, (ht + 3) / 4) * d;
			}
			if(h.rgb_bit_count == 0 || h.rgb_bit_count % 8) return 0;
			return w * ht * d * (h.rgb_bit_count / 8);
		}

		std::string formatOf(const DDSParser::Header &h) {
			if(h.pixel_format_flags & FOURCC) return fourcc(h.four_cc);
			char buf[96];
			std::snprintf(buf, sizeof buf, "bits%u flags%X r%X g%X b%X a%X", h.rgb_bit_count, h.pixel_format_flags,
						  h.r_bit_mask, h.g_bit_mask, h.b_bit_mask, h.a_bit_mask);
			return buf;
		}

	} // namespace

	bool surveyDds(const fs::path &buildRoot, std::string &json) {
		std::vector<fs::path> files;
		for(const auto &entry : fs::recursive_directory_iterator(buildRoot))
			if(entry.is_regular_file() && lower(entry.path().extension().string()) == ".dds")
				files.push_back(entry.path());
		std::sort(files.begin(), files.end());

		uint32_t read = 0, cubes = 0, partialCubes = 0, volumes = 0, notPow2 = 0, exact = 0;
		std::map<std::string, uint32_t> formats, mips, folders, overBytes;
		std::vector<std::string> refused, unsized, shortFiles, over;
		for(const auto &path : files) {
			const std::string rel = fs::relative(path, buildRoot).generic_string();
			DDSParser parser;
			if(!parser.parse(path.string().c_str())) {
				refused.push_back(quote(rel));
				continue;
			}
			++read;
			++folders[fs::path(rel).parent_path().generic_string()];
			const auto &tex = parser.getTextureData();
			const auto &h	= tex.header;
			++formats[formatOf(h)];
			const uint32_t levels = std::max<uint32_t>(1, h.mip_map_count);
			++mips[std::to_string(h.mip_map_count)];
			const auto pow2 = [](uint32_t v) { return v && !(v & (v - 1)); };
			if(!pow2(h.width) || !pow2(h.height)) ++notPow2;
			uint32_t faces = 1;
			if(h.caps2 & CUBEMAP) {
				++cubes;
				faces = 0;
				for(uint32_t bit = 0x400; bit <= 0x8000; bit <<= 1)
					if(h.caps2 & bit) ++faces;
				if((h.caps2 & ALLFACES) != ALLFACES) ++partialCubes;
			}
			const bool volume = (h.caps2 & VOLUME) && h.depth > 1;
			if(volume) ++volumes;
			uint64_t expected = 0;
			bool sized		  = true;
			for(uint32_t l = 0; l < levels; ++l) {
				const uint64_t bytes = levelBytes(h, std::max<uint64_t>(1, h.width >> l),
												  std::max<uint64_t>(1, h.height >> l),
												  volume ? std::max<uint64_t>(1, h.depth >> l) : 1);
				if(!bytes) sized = false;
				expected += bytes;
			}
			expected *= faces;
			if(!sized) {
				unsized.push_back(quote(rel));
				continue;
			}
			const std::string note = quote(rel + " " + std::to_string(tex.pixel_data_size) + "/" + std::to_string(expected));
			if(tex.pixel_data_size < expected)
				shortFiles.push_back(note);
			else if(tex.pixel_data_size > expected) {
				over.push_back(note);
				++overBytes[std::to_string(tex.pixel_data_size - expected)];
			}
			else
				++exact;
		}
		json = "{\"format\":\"dds\",\"files\":" + std::to_string(files.size()) + ",\"read\":" + std::to_string(read) +
			   ",\"folders\":" + histogram(folders) + ",\"formats\":" + histogram(formats) + ",\"mipCounts\":" + histogram(mips) +
			   ",\"cubes\":" + std::to_string(cubes) + ",\"partialCubes\":" + std::to_string(partialCubes) +
			   ",\"volumes\":" + std::to_string(volumes) + ",\"notPowerOfTwo\":" + std::to_string(notPow2) +
			   ",\"sizeExact\":" + std::to_string(exact) + ",\"short\":" + list(shortFiles) + ",\"over\":" + list(over) + ",\"overBytes\":" + histogram(overBytes) +
			   ",\"unsized\":" + list(unsized) + ",\"refused\":" + list(refused) + "}\n";
		return refused.empty() && unsized.empty() && shortFiles.empty();
	}

} // namespace survey
