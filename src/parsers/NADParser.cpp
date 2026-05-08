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

#include "NADParser.h"

/*
 */
NADParser::~NADParser() {

	if(clip.bone_tracks) {
		for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) free(clip.bone_tracks[i].keys);
		free(clip.bone_tracks);
	}
	free(clip.key_tags);
	if(clip.shape_tracks) {
		for(uint32_t i = 0; i < clip.num_shape_tracks; ++i) free(clip.shape_tracks[i].keys);
		free(clip.shape_tracks);
	}
}

/*
 */
int NADParser::parse(const char *filePath) {

	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 16) {
		free(data);
		return 0;
	}

	clip.file_size = file_size;
	clip.version		 = FileUtils::readUInt32(data, 0);
	clip.num_bone_tracks = FileUtils::readUInt32(data, 4);
	clip.flags			 = FileUtils::readUInt32(data, 8);
	clip.duration		 = FileUtils::readFloat(data, 12);

	if(clip.version != 4) {
		printf("[NAD] Warning: version %u (expected 4)\n", clip.version);
	}

	size_t off = 16;

	// bone tracks
	// file order per track (from tRenderKeyTrack::LoadData):
	// num_keys (uint32)
	// bone_number (uint32) — read into offset 0 of tBoneTrack
	// type (uint32) — 0=rotation, 1=translation
	// keys[num_keys] × 52 bytes each (tBoneKey)
	clip.bone_tracks = (BoneTrack *)calloc(clip.num_bone_tracks, sizeof(BoneTrack));

	for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) {
		if(off + 12 > file_size) {
			free(data);
			return 0;
		}

		BoneTrack *t   = &clip.bone_tracks[i];
		t->num_keys	   = FileUtils::readUInt32(data, off);
		t->bone_number = FileUtils::readUInt32(data, off + 4);
		t->type		   = FileUtils::readUInt32(data, off + 8);
		off += 12;

		size_t keyBytes = (size_t)t->num_keys * 52;
		if(off + keyBytes > file_size) {
			free(data);
			return 0;
		}

		t->keys = (BoneKey *)calloc(t->num_keys, sizeof(BoneKey));
		for(uint32_t k = 0; k < t->num_keys; ++k) {
			BoneKey *key   = &t->keys[k];
			key->frame	   = FileUtils::readFloat(data, off);
			key->value.x   = FileUtils::readFloat(data, off + 4);
			key->value.y   = FileUtils::readFloat(data, off + 8);
			key->value.z   = FileUtils::readFloat(data, off + 12);
			key->b_coeff.x = FileUtils::readFloat(data, off + 16);
			key->b_coeff.y = FileUtils::readFloat(data, off + 20);
			key->b_coeff.z = FileUtils::readFloat(data, off + 24);
			key->c_coeff.x = FileUtils::readFloat(data, off + 28);
			key->c_coeff.y = FileUtils::readFloat(data, off + 32);
			key->c_coeff.z = FileUtils::readFloat(data, off + 36);
			key->d_coeff.x = FileUtils::readFloat(data, off + 40);
			key->d_coeff.y = FileUtils::readFloat(data, off + 44);
			key->d_coeff.z = FileUtils::readFloat(data, off + 48);
			off += 52;
		}
	}

	// key tags
	clip.num_key_tags = 0;
	clip.key_tags	  = nullptr;
	if(off + 4 <= file_size) {
		clip.num_key_tags = FileUtils::readUInt32(data, off);
		off += 4;
		if(clip.num_key_tags > 0 && off + clip.num_key_tags * 8 <= file_size) {
			clip.key_tags = (KeyTag *)calloc(clip.num_key_tags, sizeof(KeyTag));
			for(uint32_t i = 0; i < clip.num_key_tags; ++i) {
				clip.key_tags[i].tag_type  = FileUtils::readUInt32(data, off);
				clip.key_tags[i].frame_num = FileUtils::readFloat(data, off + 4);
				off += 8;
			}
		}
	}

	// shape tracks
	clip.num_shape_tracks = 0;
	clip.shape_tracks	  = nullptr;
	if(off + 4 <= file_size) {
		clip.num_shape_tracks = FileUtils::readUInt32(data, off);
		off += 4;
		if(clip.num_shape_tracks > 0) {
			clip.shape_tracks = (ShapeTrack *)calloc(clip.num_shape_tracks, sizeof(ShapeTrack));
			for(uint32_t i = 0; i < clip.num_shape_tracks; ++i) {
				if(off + 4 > file_size) break;
				ShapeTrack *st = &clip.shape_tracks[i];
				st->group_num  = data[off];
				st->shape_num  = data[off + 1];
				st->num_keys   = FileUtils::readUInt16(data, off + 2);
				off += 4;
				if(st->num_keys > 0 && off + st->num_keys * 8 <= file_size) {
					st->keys = (ShapeKey *)calloc(st->num_keys, sizeof(ShapeKey));
					for(uint16_t k = 0; k < st->num_keys; ++k) {
						st->keys[k].frame  = FileUtils::readFloat(data, off);
						st->keys[k].weight = FileUtils::readFloat(data, off + 4);
						off += 8;
					}
				}
			}
		}
	}

	uint32_t rotCount = 0, posCount = 0, totalKeys = 0;
	for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) {
		totalKeys += clip.bone_tracks[i].num_keys;
		if(clip.bone_tracks[i].type == TRACK_ROTATION)
			rotCount++;
		else
			posCount++;
	}
	printf("[NAD] %s: v%u, %u tracks (%u rot + %u pos), %.0f frames, %u keys, %u tags, %u shapes\n", filePath,
		   clip.version, clip.num_bone_tracks, rotCount, posCount, clip.duration, totalKeys, clip.num_key_tags,
		   clip.num_shape_tracks);

	free(data);
	return 1;
}

/*
 */
int NADParser::serialize(const char *filePath) const {

	size_t total_size = 16;
	for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) total_size += 12 + (size_t)clip.bone_tracks[i].num_keys * 52;
	total_size += 4 + clip.num_key_tags * 8;
	total_size += 4;
	for(uint32_t i = 0; i < clip.num_shape_tracks; ++i) total_size += 4 + (size_t)clip.shape_tracks[i].num_keys * 8;

	uint8_t *buf = (uint8_t *)malloc(total_size);
	size_t off	 = 0;

	auto wr32 = [&](uint32_t v) {
		memcpy(buf + off, &v, 4);
		off += 4;
	};
	auto wr16 = [&](uint16_t v) {
		memcpy(buf + off, &v, 2);
		off += 2;
	};
	auto wrF = [&](float v) {
		memcpy(buf + off, &v, 4);
		off += 4;
	};
	auto wr8 = [&](uint8_t v) { buf[off++] = v; };

	wr32(clip.version);
	wr32(clip.num_bone_tracks);
	wr32(clip.flags);
	wrF(clip.duration);

	for(uint32_t i = 0; i < clip.num_bone_tracks; ++i) {
		const BoneTrack *t = &clip.bone_tracks[i];
		wr32(t->num_keys);
		wr32(t->bone_number);
		wr32(t->type);
		for(uint32_t k = 0; k < t->num_keys; ++k) {
			const BoneKey *key = &t->keys[k];
			wrF(key->frame);
			wrF(key->value.x);
			wrF(key->value.y);
			wrF(key->value.z);
			wrF(key->b_coeff.x);
			wrF(key->b_coeff.y);
			wrF(key->b_coeff.z);
			wrF(key->c_coeff.x);
			wrF(key->c_coeff.y);
			wrF(key->c_coeff.z);
			wrF(key->d_coeff.x);
			wrF(key->d_coeff.y);
			wrF(key->d_coeff.z);
		}
	}

	wr32(clip.num_key_tags);
	for(uint32_t i = 0; i < clip.num_key_tags; ++i) {
		wr32(clip.key_tags[i].tag_type);
		wrF(clip.key_tags[i].frame_num);
	}

	wr32(clip.num_shape_tracks);
	for(uint32_t i = 0; i < clip.num_shape_tracks; ++i) {
		const ShapeTrack *st = &clip.shape_tracks[i];
		wr8(st->group_num);
		wr8(st->shape_num);
		wr16(st->num_keys);
		for(uint16_t k = 0; k < st->num_keys; ++k) {
			wrF(st->keys[k].frame);
			wrF(st->keys[k].weight);
		}
	}

	int ok = FileUtils::writeBinaryFile(filePath, buf, total_size);
	free(buf);
	return ok;
}
