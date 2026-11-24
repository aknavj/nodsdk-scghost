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

// NODSurvey: read every file of one kind with the SDK parsers and report, as
// JSON, what the files hold and where they do not fit the layout.
//
//   NODSurvey nod <folder> <out.json>   every .nod: groups, palettes, bones
//   NODSurvey nad <folder> <out.json>   every .nad: tracks, keys, key joins
//   NODSurvey nsa <build> <out.json>    every .nsa and .nas, and how each
//                                       material name of 3D/Models is found
//
// Exit 0 when every file was read; 1 when a file was refused or left bytes
// over (nsa: a block not closed, or a property dropped); 2 on a usage or
// output error. The parsers' own log lines go to the console; the report
// goes only to <out.json>.

#include "NADParser.h"
#include "NODParser.h"
#include "report.h"

namespace fs = std::filesystem;
using survey::filesOf;
using survey::histogram;
using survey::list;
using survey::num;
using survey::quote;

namespace {

	uint16_t u16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }

	// ---- NOD --------------------------------------------------------------

	struct NodReport {
		uint32_t files = 0, read = 0, groups = 0, emptyGroups = 0, blendFiles = 0, colorFiles = 0;
		std::map<uint32_t, uint32_t> versions, vertexTypes, byte1E, byte1F, byte20, byte37, groupBones;
		std::vector<std::string> refused, leftover, faults;
	};

	void fault(NodReport &r, const std::string &file, int group, const std::string &what) {
		r.faults.push_back("{\"file\":" + quote(file) + ",\"group\":" + std::to_string(group) + ",\"fault\":" + quote(what) + "}");
	}

	void surveyNod(const fs::path &path, NodReport &r) {
		const std::string name = path.filename().string();
		++r.files;
		NODParser parser;
		if(!parser.parse(path.string().c_str())) {
			r.refused.push_back("{\"file\":" + quote(name) + ",\"error\":" + quote(parser.getLastError()) + "}");
			return;
		}
		const auto &m = parser.getModelData();
		++r.read;
		++r.versions[m.header.flags];
		if(m.bytes_read != m.file_size)
			r.leftover.push_back("{\"file\":" + quote(name) + ",\"size\":" + std::to_string(m.file_size) +
								 ",\"read\":" + std::to_string(m.bytes_read) + "}");
		if(m.blend_count) ++r.blendFiles;
		if(m.color_count) ++r.colorFiles;
		for(uint32_t s = 0; s < m.vertex_set_count; ++s) ++r.vertexTypes[m.vertex_sets[s].type];

		// bones: a parent is an earlier bone, or 0xFF
		for(uint32_t b = 0; b < m.bone_count; ++b) {
			const uint8_t parent = m.bones[b].parent;
			if(parent != 0xFF && parent >= b) fault(r, name, -1, "bone " + std::to_string(b) + " has parent " + std::to_string(parent));
		}

		uint32_t indexSum = 0;
		uint32_t advance[NODParser::MAX_VERTEX_SETS] = {0};
		for(uint32_t g = 0; g < m.mesh_group_count; ++g) {
			++r.groups;
			const uint8_t *raw = m.raw_data + m.mesh_group_offset + size_t(g) * 56;
			const auto &group = m.mesh_groups[g];
			for(uint32_t lod = 0; lod < NODParser::MAX_LODS; ++lod) indexSum += u16(raw + 4 + lod * 6) + u16(raw + 6 + lod * 6);
			const uint16_t groupVerts = u16(raw + 0x1C);
			advance[group.vertex_set_index] += groupVerts;
			++r.byte1E[raw[0x1E]];
			++r.byte1F[raw[0x1F]];
			++r.byte20[raw[0x20]];
			++r.byte37[raw[0x37]];
			++r.groupBones[group.num_bones];

			if(group.shader_index >= m.shader_count) fault(r, name, int(g), "material " + std::to_string(group.shader_index));
			for(uint32_t p = 0; p < group.num_bones; ++p)
				if(group.a_bones[p] >= m.bone_count) fault(r, name, int(g), "palette entry " + std::to_string(group.a_bones[p]));
			// an empty group (no indices, no vertices) draws nothing; its bones say nothing
			const bool empty = groupVerts == 0 && group.lods[0].strip_count + group.lods[0].list_count == 0;
			if(empty) ++r.emptyGroups;
			// a group with one bone draws rigid vertices; more bones, skinned ones
			const uint32_t baseType = m.vertex_sets[group.vertex_set_index].type % 3;
			if(!empty && (group.num_bones <= 1) != (baseType == 0))
				fault(r, name, int(g), std::to_string(group.num_bones) + " bones on vertex type " + std::to_string(m.vertex_sets[group.vertex_set_index].type));

			// detail level 0: every index names a vertex of this group
			const auto &lod = group.lods[0];
			const uint32_t end = lod.list_start + lod.list_count;
			uint32_t highest = 0;
			for(uint32_t i = lod.strip_start; i < end && i < m.index_count; ++i) highest = std::max<uint32_t>(highest, m.indices[i]);
			if(end > lod.strip_start && highest >= groupVerts)
				fault(r, name, int(g), "index " + std::to_string(highest) + " of " + std::to_string(groupVerts) + " vertices");
		}
		if(indexSum != m.index_count)
			fault(r, name, -1, "groups hold " + std::to_string(indexSum) + " of " + std::to_string(m.index_count) + " indices");
		for(uint32_t s = 0; s < m.vertex_set_count; ++s)
			if(advance[s] != m.vertex_sets[s].count)
				fault(r, name, -1, "groups hold " + std::to_string(advance[s]) + " of " + std::to_string(m.vertex_sets[s].count) +
									   " vertices of set " + std::to_string(s));
	}

	std::string nodJson(const NodReport &r) {
		return "{\"format\":\"nod\",\"files\":" + std::to_string(r.files) + ",\"read\":" + std::to_string(r.read) +
			   ",\"groups\":" + std::to_string(r.groups) + ",\"emptyGroups\":" + std::to_string(r.emptyGroups) +
			   ",\"versions\":" + histogram(r.versions) +
			   ",\"vertexTypes\":" + histogram(r.vertexTypes) + ",\"groupBones\":" + histogram(r.groupBones) +
			   ",\"groupByte1E\":" + histogram(r.byte1E) + ",\"groupByte1F\":" + histogram(r.byte1F) +
			   ",\"groupByte20\":" + histogram(r.byte20) + ",\"groupByte37\":" + histogram(r.byte37) +
			   ",\"withBlendData\":" + std::to_string(r.blendFiles) + ",\"withColors\":" + std::to_string(r.colorFiles) +
			   ",\"refused\":" + list(r.refused) + ",\"leftover\":" + list(r.leftover) + ",\"faults\":" + list(r.faults) + "}\n";
	}

	// ---- NAD --------------------------------------------------------------

	// One reading of `t` inside a key: how far a key's curve, at its end,
	// lands from the next key's value.
	struct Join {
		const char *name;
		double maxError = 0;
		uint32_t within = 0, joins = 0;
	};

	struct NadReport {
		uint32_t files = 0, read = 0, tracks = 0, keys = 0, tags = 0, maxBone = 0;
		uint32_t lastKeyNaN = 0, innerNaN = 0, frameOrder = 0, frameRange = 0;
		std::map<uint32_t, uint32_t> versions, kinds, flags;
		std::vector<std::string> refused, leftover, withShapes;
		// [kind][reading]: kind 0 turn, 1 position; reading frames, 0..1, seconds
		Join joins[2][3] = {{{"frames"}, {"unit"}, {"seconds"}}, {{"frames"}, {"unit"}, {"seconds"}}};
	};

	bool finite3(const glm::vec3 &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

	double curveGap(const NADParser::BoneKey &k, const NADParser::BoneKey &next, double t) {
		double gap = 0;
		for(int a = 0; a < 3; ++a) {
			const double v = k.value[a] + t * k.b_coeff[a] + t * t * k.c_coeff[a] + t * t * t * k.d_coeff[a];
			gap = std::max(gap, std::fabs(v - next.value[a]) / (1.0 + std::fabs(next.value[a])));
		}
		return gap;
	}

	void surveyNad(const fs::path &path, NadReport &r) {
		const std::string name = path.filename().string();
		++r.files;
		NADParser parser;
		if(!parser.parse(path.string().c_str())) {
			r.refused.push_back("{\"file\":" + quote(name) + ",\"error\":\"truncated\"}");
			return;
		}
		const auto &clip = parser.getAnimationClip();
		++r.read;
		++r.versions[clip.version];
		++r.flags[clip.flags];
		r.tags += clip.num_key_tags;
		if(clip.bytes_read != clip.file_size)
			r.leftover.push_back("{\"file\":" + quote(name) + ",\"size\":" + std::to_string(clip.file_size) +
								 ",\"read\":" + std::to_string(clip.bytes_read) + "}");
		if(clip.num_shape_tracks) r.withShapes.push_back(quote(name));
		for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) {
			const auto &track = clip.bone_tracks[i];
			++r.tracks;
			++r.kinds[track.type];
			r.maxBone = std::max(r.maxBone, track.bone_number);
			r.keys += track.num_keys;
			for(uint32_t k = 0; k < track.num_keys; ++k) {
				const auto &key = track.keys[k];
				const bool last = k + 1 == track.num_keys;
				const bool whole = std::isfinite(key.frame) && finite3(key.value) && finite3(key.b_coeff) && finite3(key.c_coeff) && finite3(key.d_coeff);
				if(!whole) ++(last ? r.lastKeyNaN : r.innerNaN);
				if(key.frame < 0 || key.frame > clip.duration + 1e-3f) ++r.frameRange;
				if(last || track.type > 1) continue;
				const auto &next = track.keys[k + 1];
				if(!(next.frame > key.frame)) ++r.frameOrder;
				if(!whole || !finite3(next.value)) continue;
				const double dt = double(next.frame) - double(key.frame);
				const double readings[3] = {dt, 1.0, dt / 30.0};
				for(int m = 0; m < 3; ++m) {
					Join &j = r.joins[track.type][m];
					const double gap = curveGap(key, next, readings[m]);
					++j.joins;
					if(gap < 1e-3) ++j.within;
					j.maxError = std::max(j.maxError, gap);
				}
			}
		}
	}

	std::string joinJson(const Join (&joins)[3]) {
		std::string out = "{";
		for(int m = 0; m < 3; ++m)
			out += std::string(m ? "," : "") + "\"" + joins[m].name + "\":{\"joins\":" + std::to_string(joins[m].joins) +
				   ",\"within\":" + std::to_string(joins[m].within) + ",\"maxError\":" + num(joins[m].maxError) + "}";
		return out + "}";
	}

	std::string nadJson(const NadReport &r) {
		return "{\"format\":\"nad\",\"files\":" + std::to_string(r.files) + ",\"read\":" + std::to_string(r.read) +
			   ",\"tracks\":" + std::to_string(r.tracks) + ",\"keys\":" + std::to_string(r.keys) +
			   ",\"tags\":" + std::to_string(r.tags) + ",\"maxBone\":" + std::to_string(r.maxBone) +
			   ",\"versions\":" + histogram(r.versions) + ",\"kinds\":" + histogram(r.kinds) + ",\"flags\":" + histogram(r.flags) +
			   ",\"lastKeyNaN\":" + std::to_string(r.lastKeyNaN) + ",\"innerNaN\":" + std::to_string(r.innerNaN) +
			   ",\"frameOrder\":" + std::to_string(r.frameOrder) + ",\"frameRange\":" + std::to_string(r.frameRange) +
			   ",\"joinsTurn\":" + joinJson(r.joins[0]) + ",\"joinsPosition\":" + joinJson(r.joins[1]) +
			   ",\"withShapeTracks\":" + list(r.withShapes) + ",\"refused\":" + list(r.refused) + ",\"leftover\":" + list(r.leftover) + "}\n";
	}

	int usage() {
		std::fprintf(stderr, "usage: NODSurvey nod|nad <folder> <out.json>\n       NODSurvey nsa|nms <build> <out.json>\n");
		return 2;
	}

} // namespace

int main(int argc, char **argv) {
	if(argc != 4) return usage();
	const std::string kind = argv[1];
	const fs::path dir	   = argv[2];
	if((kind != "nod" && kind != "nad" && kind != "nsa" && kind != "nms") || !fs::is_directory(dir)) return usage();

	std::string json;
	bool clean = false;
	if(kind == "nod") {
		NodReport r;
		for(const auto &path : filesOf(dir, ".nod")) surveyNod(path, r);
		json  = nodJson(r);
		clean = r.refused.empty() && r.leftover.empty();
	} else if(kind == "nad") {
		NadReport r;
		for(const auto &path : filesOf(dir, ".nad")) surveyNad(path, r);
		json  = nadJson(r);
		clean = r.refused.empty() && r.leftover.empty();
	} else if(kind == "nsa") {
		clean = survey::surveyNsa(dir, json);
	} else {
		clean = survey::surveyNms(dir, json);
	}

	if(!survey::writeFile(argv[3], json)) {
		std::fprintf(stderr, "NODSurvey: cannot write %s\n", argv[3]);
		return 2;
	}
	return clean ? 0 : 1;
}
