#ifndef NOD_PROPERTIES_H
#define NOD_PROPERTIES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct NODModifier {
	uint32_t prop_id;
	uint8_t value[32];
	uint32_t value_size;
	size_t value_offset;
};

struct NODPropertyInfo {
	const char *name;
	uint32_t handler;
	uint32_t native_type;
};

// Verified engine table: name[32], handler, native type; 177 entries.
const NODPropertyInfo *nodPropertyInfo(uint32_t id);
bool nodPropertyMapEditable(uint32_t id, bool preview);

bool nodModifierString(const NODModifier &modifier, std::string &value);
inline size_t nodPlacementPrefix(const std::vector<NODModifier> &modifiers) {
	return modifiers.size() > 3 && (modifiers[3].prop_id == 3 || modifiers[3].prop_id == 15) ? 4 : 3;
}

#endif
