/*
 * Copyright (C) 2026, OpenNod SDK & StaRev contributors
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "NILCollision.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

/*
 */
namespace nod::nil {
namespace {

/*
 */
class DecodeError : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
};

/*
 */
class Reader {

	public:
		Reader(const uint8_t *data, size_t length) : data_(data), size_(length) {}

		size_t position() const { return offset_; }
		size_t remaining() const { return size_ - offset_; }

		[[noreturn]] void fail(const std::string &message) const {
			throw DecodeError(message + " at byte " + std::to_string(offset_));
		}

		const uint8_t *take(size_t count, size_t stride = 1) {
			if(!stride || count > remaining() / stride) {
				fail("Truncated NIL");
			}

			const auto *result = data_ + offset_;
			offset_ += count * stride;
			return result;
		}

		uint8_t byte() { return *take(1); }
		uint16_t half() {
			const auto *p = take(2);
			return static_cast<uint16_t>(p[0] | (uint16_t(p[1]) << 8));
		}

		uint32_t word() {
			const auto *p = take(4);
			return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
		}

		float real() {
			const uint32_t bits = word();
			float value;
			static_assert(sizeof(value) == sizeof(bits), "NIL requires binary32 floats");
			std::memcpy(&value, &bits, sizeof(value));
			if(!std::isfinite(value)) {
				fail("Non-finite collision value");
			}

			return value;
		}

	private:
		const uint8_t *data_;
		size_t size_;
		size_t offset_ = 0;
};

/*
 */
void skipSectors(Reader &reader, CollisionLayout &layout) {
	if(std::memcmp(reader.take(4), "NIL\x10", 4) || reader.word() != 35) {
		reader.fail("Expected NIL v35");
	}

	reader.take(84);
	const uint32_t shaders = reader.word();
	if(shaders > 256) {
		reader.fail("Too many NIL materials");
	}

	reader.take(shaders, 32);
	const auto envs = reader.half();
	reader.take(envs, 32);
	const uint32_t count = reader.word();
	if(!count || count > 512) {
		reader.fail("Expected 1-512 sectors");
	}

	layout.sectors.resize(count);
	for(auto &sector : layout.sectors) {
		reader.take(16);
		const auto planes = reader.byte();
		reader.take(planes, 16);
		const auto pools = reader.byte();
		if(pools > 2) {
			reader.fail("Unsupported sector vertex pool count");
		}

		int previous_type = -1;
		for(size_t i = 0; i < pools; ++i) {
			const auto type = reader.byte();
			if(type > 1 || type <= previous_type) {
				reader.fail("Unsupported sector vertex pool sequence");
			}

			previous_type = type;
			const auto vertices = reader.word();
			if(vertices > 65535) {
				reader.fail("Too many sector vertices");
			}

			reader.take(vertices, type == 1 ? 72 : 36);
		}

		const auto groups = reader.half();
		reader.take(groups, 47);
		sector.groups.resize(groups);
		const auto indices = reader.word();
		reader.take(indices, 2);
		const auto things = reader.byte();
		reader.take(things, 68);
		const auto lights = reader.half();
		reader.take(lights, 72);
		const auto references = reader.half();
		reader.take(references, 32);
		reader.take(6);
	}

	layout.post_sector_offset = reader.position();
}

/*
 */
CollisionBody readBody(Reader &reader, size_t sectors) {
	CollisionBody body;
	const auto vertices = reader.half();
	if(!vertices) {
		reader.fail("Empty collision mesh");
	}

	body.vertices.resize(vertices);
	for(auto &vertex : body.vertices) {
		for(float &component : vertex) {
			component = reader.real();
		}
	}

	const auto indices = reader.word();
	if(!indices || indices % 3) {
		reader.fail("Collision indices must form triangles");
	}

	// Check the entire allocation before accepting an untrusted count.
	const auto *index_data = reader.take(indices, 2);
	body.indices.reserve(indices);
	for(uint32_t i = 0; i < indices; ++i) {
		const uint16_t index = static_cast<uint16_t>(index_data[i * 2] | (uint16_t(index_data[i * 2 + 1]) << 8));
		if(index >= vertices) {
			reader.fail("Collision index outside vertex buffer");
		}

		body.indices.push_back(index);
	}

	const auto materials = reader.half();
	const auto *records = reader.take(materials, 8);
	body.material_records.resize(materials);
	for(size_t i = 0; i < materials; ++i) {
		std::memcpy(body.material_records[i].data(), records + i * 8, 8);
	}

	for(float &value : body.mopp_info) {
		value = reader.real();
	}

	const auto code_size = reader.word();
	if(!code_size) {
		reader.fail("Empty MOPP code");
	}

	const auto *code = reader.take(code_size);
	body.mopp_code.assign(code, code + code_size);
	body.owner_type = reader.half();
	body.owner_index = reader.half();
	if(body.owner_type > 1) {
		reader.fail("Unsupported collision owner type");
	}

	if(body.owner_type == 1 && body.owner_index >= sectors) {
		reader.fail("Collision sector index out of range");
	}

	return body;
}

}

/*
 */
std::optional<size_t> surfaceGroupForTriangle(const CollisionSector &sector, uint32_t key) {
	for(size_t i = sector.groups.size(); i > 0; --i) {
		const auto &group = sector.groups[i - 1];
		if(group.present && key >= group.offset) {
			return i - 1;
	}
	}

	return std::nullopt;
}

/*
 */
std::optional<uint16_t> soundTypeForTriangle(const CollisionBody &body, uint32_t key) {
	for(const auto &record : body.material_records) {
		const uint32_t limit = uint32_t(record[0]) | (uint32_t(record[1]) << 8) |
			(uint32_t(record[2]) << 16) | (uint32_t(record[3]) << 24);
		if(key < limit) {
			return static_cast<uint16_t>(record[4] | (uint16_t(record[5]) << 8));
	}
	}

	return std::nullopt;
}

/*
 */
bool readCollisionLayout(const uint8_t *bytes, size_t size, CollisionLayout &layout, std::string &error) {
	error.clear();
	if(!bytes || size < 102 || size > 256u * 1024u * 1024u) {
		error = "NIL input must contain 102 bytes through 256 MiB";
		return false;
	}

	Reader reader(bytes, size);
	CollisionLayout result;
	try {
		skipSectors(reader, result);
		if(reader.half() != 0) {
			reader.fail("Terrain collision layout is unsupported");
		}

		result.setup = reader.half();
		for(float &bound : result.bounds) {
			bound = reader.real();
		}

		for(size_t axis = 0; axis < 3; ++axis) {
			if(result.bounds[axis] > result.bounds[axis + 3]) {
				reader.fail("Inverted physics bounds");
			}
		}

		if(reader.half() != 1) {
			reader.fail("Unsupported Havok section version");
		}

		for(;;) {
			const auto segment = reader.byte();
			if(!segment) {
				break;
			}

			if(segment != 1) {
				reader.fail("Unsupported Havok segment type");
			}

			result.bodies.push_back(readBody(reader, result.sectors.size()));
		}

		for(auto &sector : result.sectors) {
			sector.present = reader.word();
			for(auto &group : sector.groups) {
				group.present = reader.word();
				if(group.present) {
					group.offset = reader.word();
			}
		}
		}

		if(reader.remaining()) {
			reader.fail("Unexpected bytes after collision mappings");
	}
	} catch(const DecodeError &failure) {
		error = failure.what();
		return false;
	}

	layout = std::move(result);
	return true;
}

}
