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

#include "XWBParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

/*
 */
static const int s_imaIndexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

/*
 */
static const int s_imaStepTable[89] = {
	7,	  8,	 9,		10,	   11,	  12,	 13,	14,	   16,	  17,	 19,	21,	   23,	  25,	28,
	31,	  34,	 37,	41,	   45,	  50,	 55,	60,	   66,	  73,	 80,	88,	   97,	  107,	118,
	130,  143,	 157,	173,   190,	  209,	 230,	253,   279,	  307,	 337,	371,   408,	  449,	494,
	544,  598,	 658,	724,   796,	  876,	 963,	1060,  1166,  1282,	 1411,	1552,  1707,  1878, 2066,
	2272, 2499,	 2749,	3024,  3327,  3660,	 4026,	4428,  4871,  5358,	 5894,	6484,  7132,  7845, 8630,
	9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

/*
 */
static inline int16_t ClampS16(int v) {
	if(v < -32768) return -32768;
	if(v > 32767) return 32767;
	return (int16_t)v;
}

/*
 */
static inline int clamp_index(int v) {
	if(v < 0) return 0;
	if(v > 88) return 88;
	return v;
}

/*
 */
static inline int16_t decode_nibble(int nibble, int &predictor, int &stepIndex) {
	int step = s_imaStepTable[stepIndex];
	int diff = step >> 3;
	if(nibble & 1) diff += step >> 2;
	if(nibble & 2) diff += step >> 1;
	if(nibble & 4) diff += step;
	if(nibble & 8) diff = -diff;
	predictor += diff;
	predictor = (int)ClampS16(predictor);
	stepIndex += s_imaIndexTable[nibble & 0xF];
	stepIndex = clamp_index(stepIndex);
	return (int16_t)predictor;
}

/*
 */
XWBParser::~XWBParser() {
	free(m_data.entries);
	free(m_data.raw_data);
}

/*
 */
const char *XWBParser::codecName(uint32_t codec) {
	switch(codec) {
		case CODEC_PCM:
			return "PCM";
		case CODEC_XBOX_ADPCM:
			return "Xbox ADPCM";
		case CODEC_WMA:
			return "WMA";
		case CODEC_XMA:
			return "XMA";
		default:
			return "Unknown";
	}
}

/*
 */
float XWBParser::getEntryDuration(uint32_t entryIndex) const {
	if(entryIndex >= m_data.entry_count) return 0.0f;

	const WaveEntry &e = m_data.entries[entryIndex];
	if(e.sample_rate == 0 || e.channels == 0) return 0.0f;

	// calculate from data length and format
	uint32_t frames = 0;
	switch(e.codec) {
		case CODEC_PCM: {
			uint32_t bytesPerFrame = e.channels * (e.bits_per_sample / 8);
			if(bytesPerFrame > 0) frames = e.data_length / bytesPerFrame;
			break;
		}
		case CODEC_XBOX_ADPCM: {
			// xbox ADPCM: 36 bytes/block/channel, 65 samples/block
			uint32_t block_size = 36 * e.channels;
			if(block_size > 0) {
				uint32_t numBlocks = e.data_length / block_size;
				frames			   = numBlocks * 65;
			}
			break;
		}
		default:
			// use duration field from entry if available
			frames = e.duration;
			break;
	}

	return (float)frames / (float)e.sample_rate;
}

/*
 */
int XWBParser::parse(const char *filePath) {
	size_t file_size = 0;
	uint8_t *data	 = FileUtils::readBinaryFile(filePath, &file_size);
	if(!data || file_size < 40) {
		free(data);
		return 0;
	}

	if(data[0] != 'W' || data[1] != 'B' || data[2] != 'N' || data[3] != 'D') {
		free(data);
		return 0;
	}

	m_data.raw_data		 = data;
	m_data.raw_data_size = file_size;
	m_data.file_size	 = file_size;
	m_data.version		 = FileUtils::readUInt32(data, 4);

	// segment table: 4 segments for v3
	struct {
		uint32_t offset, length;
	} segs[4];
	for(int i = 0; i < 4; i++) {
		segs[i].offset = FileUtils::readUInt32(data, 8 + i * 8);
		segs[i].length = FileUtils::readUInt32(data, 12 + i * 8);
	}

	// segment 0: Bank data
	if(segs[0].offset + 36 > file_size) return 0;

	const uint8_t *bd  = data + segs[0].offset;
	m_data.flags	   = FileUtils::readUInt32(bd, 0);
	m_data.entry_count = FileUtils::readUInt32(bd, 4);
	memcpy(m_data.bank_name, bd + 8, 16);
	m_data.bank_name[16] = '\0';

	uint32_t entryMetaSize = FileUtils::readUInt32(bd, 24);
	// uint32_t entryNameSize = FileUtils::readUInt32(bd, 28);
	m_data.alignment = FileUtils::readUInt32(bd, 32);

	if(m_data.entry_count > MAX_ENTRIES) m_data.entry_count = MAX_ENTRIES;

	if(entryMetaSize == 0) entryMetaSize = 24; // default

	// segment 1: Entry metadata
	m_data.entries = (WaveEntry *)calloc(m_data.entry_count, sizeof(WaveEntry));
	if(!m_data.entries) return 0;

	const uint8_t *em = data + segs[1].offset;

	for(uint32_t i = 0; i < m_data.entry_count; i++) {
		size_t eOff = (size_t)i * entryMetaSize;
		if(segs[1].offset + eOff + entryMetaSize > file_size) break;

		const uint8_t *e = em + eOff;
		WaveEntry &we	 = m_data.entries[i];

		uint32_t flagsDur = FileUtils::readUInt32(e, 0);
		uint32_t format	  = FileUtils::readUInt32(e, 4);
		we.data_offset	  = FileUtils::readUInt32(e, 8);
		we.data_length	  = FileUtils::readUInt32(e, 12);
		we.loop_start	  = FileUtils::readUInt32(e, 16);
		we.loop_length	  = FileUtils::readUInt32(e, 20);
		we.duration		  = (flagsDur >> 4) & 0x0FFFFFFF;

		we.codec		   = format & 0x3;
		we.channels		   = (format >> 2) & 0x7;
		we.sample_rate	   = (format >> 5) & 0x3FFFF;
		we.block_align	   = (format >> 23) & 0xFF;
		we.bits_per_sample = (format >> 31) & 0x1;

		if(we.codec == CODEC_PCM)
			we.bits_per_sample = we.bits_per_sample ? 16 : 8;
		else if(we.codec == CODEC_XBOX_ADPCM)
			we.bits_per_sample = 4;

		// fixup block_align for ADPCM (36 bytes per channel)
		if(we.codec == CODEC_XBOX_ADPCM)
			we.block_align = 36 * we.channels;
		else if(we.codec == CODEC_PCM)
			we.block_align = we.channels * (we.bits_per_sample / 8);

		if(we.channels == 0) we.channels = 1;
	}

	// segment 3: Wave data
	if(segs[3].offset + segs[3].length <= file_size) {
		m_data.wave_data	  = data + segs[3].offset;
		m_data.wave_data_size = segs[3].length;
	}

	printf("[XWB] %s: v%u, bank='%s', %u entries, wave_data=%.1fKB\n", filePath, m_data.version, m_data.bank_name,
		   m_data.entry_count, m_data.wave_data_size / 1024.0);

	return 1;
}

/*
 */
int XWBParser::serialize(const char *filePath) const {
	if(!m_data.raw_data || m_data.raw_data_size == 0) return 0;
	return FileUtils::writeBinaryFile(filePath, m_data.raw_data, m_data.raw_data_size);
}

/*
 */
int16_t *XWBParser::decode_xbox_adpcm(const uint8_t *data, size_t dataLen, uint32_t channels, uint32_t *outFrameCount) {
	if(!data || dataLen == 0 || channels == 0) return nullptr;

	// xbox IMA ADPCM — MS interleaved format.
	// block size = 36 bytes per channel.
	// per channel per block: 4-byte preamble + 32 bytes data = 65 samples.
	// mono layout (1 channel):
	// [hdr0(4)] [data0(32)]
	// stereo / multi-channel layout (MS IMA interleaved):
	// [hdr0(4)] [hdr1(4)]                       ← all headers first
	// [data0(4)] [data1(4)] [data0(4)] [data1(4)] ...  ← 4-byte sub-blocks
	// each 4-byte data sub-block = 8 nibbles = 8 samples (low nibble first).
	// 32 data bytes / 4 = 8 sub-blocks per channel.
	// samples per block per channel: 1 (predictor) + 8*8 = 65.
	const uint32_t BLOCK_BYTES_PER_CH  = 36;
	const uint32_t HEADER_BYTES_PER_CH = 4;
	const uint32_t DATA_BYTES_PER_CH   = 32;
	const uint32_t SAMPLES_PER_BLOCK   = 65;
	const uint32_t SUB_BLOCK_BYTES	   = 4; // nibble sub-block size
	const uint32_t SAMPLES_PER_SUB	   = 8; // 4 bytes × 2 nibbles
	uint32_t block_size				   = BLOCK_BYTES_PER_CH * channels;

	if(block_size == 0) return nullptr;
	uint32_t numBlocks = (uint32_t)(dataLen / block_size);
	if(numBlocks == 0) return nullptr;

	uint32_t totalFrames = numBlocks * SAMPLES_PER_BLOCK;
	*outFrameCount		 = totalFrames;

	int16_t *output = (int16_t *)malloc(totalFrames * channels * sizeof(int16_t));
	if(!output) return nullptr;

	for(uint32_t b = 0; b < numBlocks; b++) {
		const uint8_t *block = data + (size_t)b * block_size;
		uint32_t frameBase	 = b * SAMPLES_PER_BLOCK;

		// per-channel state
		int predictor[8]; // max 8 channels
		int stepIndex[8];

		// read all preambles (headers packed at the front)
		for(uint32_t ch = 0; ch < channels; ch++) {
			const uint8_t *hdr = block + ch * HEADER_BYTES_PER_CH;
			predictor[ch]	   = (int16_t)(hdr[0] | (hdr[1] << 8));
			stepIndex[ch]	   = clamp_index(hdr[2]);

			// first sample = predictor
			output[(frameBase * channels) + ch] = (int16_t)predictor[ch];
		}

		if(channels == 1) {
			// mono: data follows the single header
			const uint8_t *adpcmData = block + HEADER_BYTES_PER_CH;
			uint32_t sampleIdx		 = 1;

			for(uint32_t byteIdx = 0; byteIdx < DATA_BYTES_PER_CH && sampleIdx < SAMPLES_PER_BLOCK; byteIdx++) {
				uint8_t byte = adpcmData[byteIdx];

				int16_t s								  = decode_nibble(byte & 0x0F, predictor[0], stepIndex[0]);
				output[((frameBase + sampleIdx) * 1) + 0] = s;
				sampleIdx++;

				if(sampleIdx < SAMPLES_PER_BLOCK) {
					s = decode_nibble((byte >> 4) & 0x0F, predictor[0], stepIndex[0]);
					output[((frameBase + sampleIdx) * 1) + 0] = s;
					sampleIdx++;
				}
			}
		} else {
			// multi-channel: MS IMA interleaved sub-blocks
			const uint8_t *dataStart	  = block + channels * HEADER_BYTES_PER_CH;
			const uint32_t subBlocksPerCh = DATA_BYTES_PER_CH / SUB_BLOCK_BYTES; // 8
			uint32_t sampleIdx[8];
			for(uint32_t ch = 0; ch < channels; ch++) sampleIdx[ch] = 1;

			for(uint32_t sb = 0; sb < subBlocksPerCh; sb++) {
				for(uint32_t ch = 0; ch < channels; ch++) {
					// offset into the interleaved data region
					const uint8_t *sub = dataStart + (sb * channels + ch) * SUB_BLOCK_BYTES;

					for(uint32_t byteIdx = 0; byteIdx < SUB_BLOCK_BYTES; byteIdx++) {
						uint8_t byte = sub[byteIdx];

						int16_t s = decode_nibble(byte & 0x0F, predictor[ch], stepIndex[ch]);
						output[((frameBase + sampleIdx[ch]) * channels) + ch] = s;
						sampleIdx[ch]++;

						s = decode_nibble((byte >> 4) & 0x0F, predictor[ch], stepIndex[ch]);
						output[((frameBase + sampleIdx[ch]) * channels) + ch] = s;
						sampleIdx[ch]++;
					}
				}
			}
		}
	}

	return output;
}

/*
 */
int16_t *XWBParser::decodeToPCM16(uint32_t entryIndex, uint32_t *outFrameCount, uint32_t *outChannels,
								  uint32_t *outSampleRate) const {
	if(entryIndex >= m_data.entry_count) return nullptr;
	if(!m_data.wave_data) return nullptr;

	const WaveEntry &e = m_data.entries[entryIndex];
	*outChannels	   = e.channels;
	*outSampleRate	   = e.sample_rate;

	if((size_t)e.data_offset + e.data_length > m_data.wave_data_size) return nullptr;

	const uint8_t *src = m_data.wave_data + e.data_offset;

	switch(e.codec) {
		case CODEC_PCM: {
			if(e.bits_per_sample == 16) {
				uint32_t frames = e.data_length / (e.channels * 2);
				*outFrameCount	= frames;
				size_t bytes	= (size_t)frames * e.channels * sizeof(int16_t);
				int16_t *out	= (int16_t *)malloc(bytes);
				if(out) memcpy(out, src, bytes);
				return out;
			} else if(e.bits_per_sample == 8) {
				// convert 8-bit unsigned to 16-bit signed
				uint32_t frames		  = e.data_length / e.channels;
				*outFrameCount		  = frames;
				uint32_t totalSamples = frames * e.channels;
				int16_t *out		  = (int16_t *)malloc(totalSamples * sizeof(int16_t));
				if(out) {
					for(uint32_t i = 0; i < totalSamples; i++) out[i] = ((int16_t)src[i] - 128) << 8;
				}
				return out;
			}
			return nullptr;
		}

		case CODEC_XBOX_ADPCM:
			return decode_xbox_adpcm(src, e.data_length, e.channels, outFrameCount);

		default:
			// WMA, XMA not supported
			printf("[XWB] Codec %s not supported for decoding\n", codecName(e.codec));
			return nullptr;
	}
}
