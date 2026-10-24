/*
 * Copyright (C) 2026, OpenNod SDK & StaRev contributors
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef NOD_NIL_COLLISION_H
#define NOD_NIL_COLLISION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/*
 */
namespace nod::nil {

/*
 */
struct CollisionBody {
	std::vector<std::array<float, 3>> vertices;
	std::vector<uint16_t> indices;
	std::vector<std::array<uint8_t, 8>> material_records;
	std::array<float, 4> mopp_info{};
	std::vector<uint8_t> mopp_code;
	uint16_t owner_type = 0;
	uint16_t owner_index = 0;
};

/*
 */
struct CollisionGroup {
	uint32_t present = 0;
	uint32_t offset = 0;
};

/*
 */
struct CollisionSector {
	uint32_t present = 0;
	std::vector<CollisionGroup> groups;
};

/*
 */
struct CollisionLayout {
	size_t post_sector_offset = 0;
	uint16_t setup = 0;
	std::array<float, 6> bounds{};
	std::vector<CollisionBody> bodies;
	std::vector<CollisionSector> sectors;
};

// Native lookups: offsets are inclusive triangle-key starts; material records
// use exclusive limits. Absence is normal (collision-only surfaces/default sound).
std::optional<size_t> surfaceGroupForTriangle(const CollisionSector &sector, uint32_t key);
std::optional<uint16_t> soundTypeForTriangle(const CollisionBody &body, uint32_t key);

// Reads the verified no-terrain NIL v35 layout. MOPP bytecode remains opaque;
// successful framing is not proof of collision correctness or runtime readiness.
bool readCollisionLayout(const uint8_t *bytes, size_t size, CollisionLayout &layout, std::string &error);

}

#endif /* NOD_NIL_COLLISION_H */
