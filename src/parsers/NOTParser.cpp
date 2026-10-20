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

#include "NOTParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <functional>

/*
 */
NOTParser::~NOTParser() {
	clear();
}

void NOTParser::clear() {
	if(m.templates) {
		for(uint32_t i = 0; i < m.num_templates; ++i) {
			if(m.templates[i].properties) {
				free(m.templates[i].properties);
				m.templates[i].properties = nullptr;
			}
		}
		free(m.templates);
		m.templates = nullptr;
	}
	if(m.raw_data) {
		free(m.raw_data);
		m.raw_data = nullptr;
	}
	memset(&m, 0, sizeof(m));
}

/*
 */
namespace {

	/*
	*/
	uint32_t ReadU32(const uint8_t *p) {

		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}

	/*
	*/
	bool same_name(const char *a, const char *b) {
		while(*a && *b) {
			const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
			if(lower(static_cast<unsigned char>(*a++)) != lower(static_cast<unsigned char>(*b++))) return false;
		}
		return *a == *b;
	}

} // namespace

/*
 */
int NOTParser::parse(const char *filePath) {
	size_t size = 0;
	uint8_t *data = FileUtils::readBinaryFile(filePath, &size);
	if(!data) return reject("cannot read NOT file");
	const int result = parseBuffer(data, size);
	free(data);
	return result;
}

int NOTParser::reject(const char *message) {
	clear();
	last_error = message;
	return 0;
}

int NOTParser::parseBuffer(const uint8_t *data, size_t size) {
	last_error = "";
	if(!data || size < 12 || ReadU32(data) != MAGIC)
		return reject("expected NOT header with magic NOT\\x10");
	const uint32_t version = ReadU32(data + 4);
	const uint32_t count = ReadU32(data + 8);
	if(version != 1) return reject("unsupported NOT version");
	if(count == 0 || count > MAX_TEMPLATES || count > (size - 12) / 36)
		return reject("invalid or truncated NOT table of contents");
	auto *copy = static_cast<uint8_t *>(malloc(size));
	if(!copy) return reject("out of memory reading NOT");
	memcpy(copy, data, size);
	clear();
	m.raw_data = copy;
	m.raw_data_size = m.file_size = size;
	m.version = version;
	m.num_templates = count;
	m.templates = static_cast<Template *>(calloc(count, sizeof(Template)));
	if(!m.templates) return reject("out of memory reading NOT templates");
	if(!parse_toc(copy, size)) return reject("invalid NOT template offsets");
	for(uint32_t i = 0; i < count; ++i) {
		for(uint32_t j = 0; j < i; ++j)
			if(same_name(m.templates[i].name, m.templates[j].name)) return reject("duplicate NOT template name");
		if(!parse_template_data(i, copy, size)) return reject("invalid NOT modifier array or editor-data block");
	}
	return 1;
}

/*
 */
int NOTParser::parse_toc(const uint8_t *data, size_t size) {

	for(uint32_t i = 0; i < m.num_templates; ++i) {
		size_t off			 = 12 + (size_t)i * 36;
		const uint8_t *entry = data + off;

		// name: 32 bytes, null-terminated
		memset(m.templates[i].name, 0, 33);
		for(int j = 0; j < 32 && entry[j] != 0; ++j) {
			m.templates[i].name[j] = (char)entry[j];
		}

		m.templates[i].data_offset = ReadU32(entry + 32);

		if(m.templates[i].data_offset < 12 + size_t(m.num_templates) * 36 ||
		   m.templates[i].data_offset >= size ||
		   (i && m.templates[i].data_offset <= m.templates[i - 1].data_offset)) return 0;
	}
	return 1;
}

/*
 */
int NOTParser::parse_template_data(uint32_t index, const uint8_t *data, size_t size) {

	Template &tmpl = m.templates[index];
	uint32_t start = tmpl.data_offset;

	size_t end;
	if(index + 1 < m.num_templates) {
		end = m.templates[index + 1].data_offset;
	} else {
		end = size;
	}

	if(end - start < 8 || end - start > UINT32_MAX) return 0;
	tmpl.data_size = static_cast<uint32_t>(end - start);

	const uint8_t *tdata = data + start;

	tmpl.num_properties = ReadU32(tdata);
	if(tmpl.num_properties > MAX_PROPERTIES || tmpl.num_properties > (tmpl.data_size - 8) / 36) return 0;
	if(!parse_properties(tmpl, tdata + 4, size_t(tmpl.num_properties) * 36)) return 0;
	const size_t extra_header = size_t(start) + 4 + size_t(tmpl.num_properties) * 36;
	tmpl.extra_size = ReadU32(data + extra_header);
	tmpl.extra_offset = extra_header + 4;
	if(tmpl.extra_size > end - tmpl.extra_offset) return 0;
	tmpl.trailing_size = end - tmpl.extra_offset - tmpl.extra_size;
	return 1;
}

/*
 */
int NOTParser::parse_properties(Template &tmpl, const uint8_t *propData, size_t propDataSize) {

	if(propDataSize != size_t(tmpl.num_properties) * 36) return 0;
	if(tmpl.num_properties) {
		tmpl.properties = static_cast<Property *>(calloc(tmpl.num_properties, sizeof(Property)));
		if(!tmpl.properties) return 0;
	}
	for(uint32_t i = 0; i < tmpl.num_properties; ++i) {
		auto &prop = tmpl.properties[i];
		const auto *record = propData + size_t(i) * 36;
		prop.prop_id = ReadU32(record);
		prop.value_size = 32;
		prop.value_offset = size_t(record - m.raw_data) + 4;
		memcpy(prop.value, record + 4, 32);
		if(prop.prop_id == 0 && !*tmpl.parent) {
			memcpy(tmpl.parent, prop.value, 32);
			tmpl.parent[32] = 0;
		}
	}
	tmpl.property_count = tmpl.num_properties;
	tmpl.properties_complete = true;
	return 1;
}

/*
 */
int NOTParser::serialize(const char * /*filePath*/) const {

	return 0; // read-only
}

/*
 */
const NOTParser::Template *NOTParser::findTemplate(const char *name) const {

	if(!m.templates || !name) return nullptr;
	for(uint32_t i = 0; i < m.num_templates; ++i) {
		if(same_name(m.templates[i].name, name))
			return &m.templates[i];
	}
	return nullptr;
}

/*
 */
uint32_t NOTParser::countChildren(const char *parentName) const {

	if(!m.templates || !parentName) return 0;
	uint32_t count = 0;
	for(uint32_t i = 0; i < m.num_templates; ++i) {
		const auto &entry = m.templates[i];
		for(uint32_t j = 0; j < entry.property_count; ++j) {
			char name[33] = {};
			if(entry.properties[j].prop_id != 0) continue;
			memcpy(name, entry.properties[j].value, 32);
			if(same_name(name, parentName)) { ++count; break; }
		}
	}
	return count;
}

NOTParser::ResolveStatus NOTParser::resolveModifiers(const char *name, std::vector<ResolvedProperty> &out,
													std::string &error, const TemplateLookup &lookup) const {
	const auto *entry = lookup ? lookup(name) : findTemplate(name);
	if(!entry) {
		out.clear();
		error = std::string("Missing template: ") + (name ? name : "(null)");
		return ResolveStatus::missing_template;
	}
	return resolveModifierList(entry->properties, entry->property_count, entry, out, error, lookup);
}

NOTParser::ResolveStatus NOTParser::resolveModifiers(const Property *properties, size_t count,
													std::vector<ResolvedProperty> &out, std::string &error,
													const TemplateLookup &lookup) const {
	return resolveModifierList(properties, count, nullptr, out, error, lookup);
}

NOTParser::ResolveStatus NOTParser::resolveModifierList(const Property *properties, size_t count, const Template *owner,
													 std::vector<ResolvedProperty> &out, std::string &error,
													 const TemplateLookup &lookup) const {
	out.clear();
	error.clear();
	std::vector<const Template *> active;
	std::vector<ResolvedProperty> result;
	std::function<ResolveStatus(const Property *, size_t, const Template *)> expand =
		[&](const Property *list, size_t size, const Template *entry) {
		if((size && !list) || size > MAX_PROPERTIES) {
			error = "Invalid modifier array or count";
			return ResolveStatus::invalid_modifiers;
		}
		if(entry && std::find(active.begin(), active.end(), entry) != active.end()) {
			error = std::string("Cycle in template inheritance: ") + entry->name;
			return ResolveStatus::cycle;
		}
		if(entry) active.push_back(entry);
		for(size_t i = 0; i < size; ++i) {
			const auto &property = list[i];
			if(property.value_size != PROP_VALUE_SIZE) {
				error = "Invalid modifier payload size";
				return ResolveStatus::invalid_modifiers;
			}
			if(property.prop_id == 0) {
				std::string parent;
				if(!nodModifierString(property, parent)) {
					error = "Invalid basedon reference";
					return ResolveStatus::invalid_modifiers;
				}
				const auto *dependency = lookup ? lookup(parent.c_str()) : findTemplate(parent.c_str());
				if(!dependency) {
					error = "Missing template: " + parent;
					return ResolveStatus::missing_template;
				}
				const auto status = expand(dependency->properties, dependency->property_count, dependency);
				if(status != ResolveStatus::ok) return status;
				continue;
			}
			const auto existing = std::find_if(result.begin(), result.end(), [&](const ResolvedProperty &value) {
				return value.property->prop_id == property.prop_id;
			});
			if(existing != result.end()) *existing = {&property, entry};
			else if(result.size() < MAX_PROPERTIES) result.push_back({&property, entry});
			else { error = "Expanded modifier count exceeds 128"; return ResolveStatus::too_many; }
		}
		if(entry) active.pop_back();
		return ResolveStatus::ok;
	};
	const auto status = expand(properties, count, owner);
	if(status == ResolveStatus::ok) out = std::move(result);
	return status;
}
