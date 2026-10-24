/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: template_edit.h
Version: 0.6.7

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

#ifndef NOD_TEMPLATE_EDIT_H
#define NOD_TEMPLATE_EDIT_H

#include "metadata_xml.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>

/*
 */
namespace nod::metadata {

/*
 */
enum class SharedOperation { 
	set, 
	add_override, 
	remove_override, 
	preview_remove, 
	restore_override 
};

/*
 */
struct SharedRemoval {
	std::string slot, payload, order;
};

/*
 */
inline std::string shared_hex(const uint8_t *bytes, size_t count) {
	const char *digits = "0123456789abcdef";
	std::string result;
	for(size_t i = 0; i < count; ++i) {
		result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15];
	}
	return result;
}

/*
 */
inline std::string shared_order(const edit::Record &record) {
	std::string result;
	for(const auto &field : record.fields) {
		if(!field.native_modifier) continue;
		uint8_t bytes[4];
		for(unsigned i = 0; i < 4; ++i) bytes[i] = uint8_t(field.property_id >> (8 * i));
		result += shared_hex(bytes, 4);
	}
	return result;
}

/*
 */
inline bool shared_removal(const SharedRemoval &removal, uint32_t id, size_t &slot,
						  std::vector<uint8_t> &payload, std::string &error) {
	uint64_t number = 0;
	std::istringstream input(removal.slot);
	if(!(input >> number) || !input.eof() || number > 127 || std::to_string(number) != removal.slot ||
	   removal.payload.size() != 72 || removal.payload.find_first_not_of("0123456789abcdef") != std::string::npos ||
	   removal.order.size() % 8 || removal.order.size() > 127 * 8 ||
	   removal.order.find_first_not_of("0123456789abcdef") != std::string::npos || number > removal.order.size() / 8) {
		error = "Invalid native removal/restoration proof"; return false;
	}
	payload.clear();
	for(size_t i = 0; i < removal.payload.size(); i += 2)
		payload.push_back(uint8_t(std::stoul(removal.payload.substr(i, 2), nullptr, 16)));
	if(FileUtils::readUInt32(payload.data(), 0) != id) { error = "Removal proof has a different native property ID"; return false; }
	slot = static_cast<size_t>(number);
	return true;
}

/*
 */
inline bool prepare_shared_edit(NOTDatabase &database, const std::filesystem::path &workspace,
	const std::filesystem::path &assets, const std::string &name, const std::string &requested_archive,
	const std::string &id, const std::string &expected, const std::string &value,
	SharedOperation operation, const std::string &inherited_owner, const std::string &inherited_archive,
	SharedRemoval &removal, std::unique_ptr<edit::Document> &pending, std::string &error) 
{
	const bool preview = operation == SharedOperation::preview_remove;
	const bool restoring = operation == SharedOperation::restore_override;
	const bool inserting = operation == SharedOperation::add_override || restoring;
	const bool removing = operation == SharedOperation::remove_override || preview;
	const bool proved = !removal.payload.empty();

	std::string archive;
	if(!NOTDatabase::archiveName(requested_archive, archive, error)) {
		return false;
	}
	uint64_t number = 0;

	std::istringstream input(id);
	if(!(input >> number) || !input.eof() || number > UINT32_MAX || std::to_string(number) != id) {
		error = "Expected a canonical uint32 property ID"; return false;
	}

	const auto *selection = database.findTemplate(name.c_str());
	const auto *source = selection ? database.source(selection) : nullptr;
	if(!source || source->name != archive) { 
		error = "Shared template archive changed; reload metadata"; 
		return false; 
	}

	std::vector<NOTParser::ResolvedProperty> effective;
	if(database.resolveModifiers(name.c_str(), effective, error) != NOTParser::ResolveStatus::ok) {
		return false;
	}

	const NOTParser::ResolvedProperty *resolved = nullptr;
	for(const auto &entry : effective) {
		if(entry.property->prop_id == number) resolved = &entry;
	}

	if(!resolved || (inserting ? resolved->owner == selection : resolved->owner != selection)) {
		error = "Shared property ownership changed; reload metadata before editing"; return false;
	}

	const auto *property = resolved->property;
	std::string inherited_source;
	if(operation != SharedOperation::set && !preview &&
	   (!NOTDatabase::archiveName(inherited_archive, inherited_source, error) || inherited_owner.empty())) {
		if(error.empty()) error = "Shared overrides require inherited owner/archive provenance";
		return false;
	}

	const auto matches_inherited = [&](const NOTDatabase &db, const NOTParser::ResolvedProperty &entry) {
		const auto *owner_source = db.source(entry.owner);
		return owner_source && owner_source->name == inherited_source &&
			db.findTemplate(inherited_owner.c_str()) == entry.owner;
	};

	if(inserting && !matches_inherited(database, *resolved)) {
		error = "Inherited property owner/archive changed; reload metadata"; return false;
	}

	NOTDatabase::Snapshot current;
	std::filesystem::path path;
	if(!NOTDatabase::readArchive(workspace, archive, current, error, assets, &path)) {
		return false;
	}
	if(current.bytes != source->bytes) { 
		error = "Shared archive changed while preparing the edit"; 
		return false; 
	}

	auto document = std::make_unique<edit::Document>();
	if(!document->configure_workspace(workspace, assets, error) || !document->load(path, edit::Format::not_file, error)) {
		return false;
	}
	if(document->bytes() != source->bytes) { 
		error = "Shared archive changed while loading the edit"; 
		return false; 
	}
	
	size_t record = document->records().size();
	for(size_t r = 0; r < document->records().size(); ++r) {
		if(document->records()[r].offset == selection->data_offset && document->records()[r].name == selection->name) {
			record = r;
		}
	}
	if(record == document->records().size()) { 
		error = "Cannot bind the shared template to its native record"; 
		return false; 
	}

	const auto *value_document = document.get();
	edit::Document inherited;
	const auto *value_source = database.source(resolved->owner);
	if(!value_source) { 
		error = "Missing inherited property archive"; 
		return false; 
	}
	if(value_source->name != archive) {
		if(!inherited.load_bytes(value_source->bytes, edit::Format::not_file, error)) { 
			return false;
		}
		value_document = &inherited;
	}

	const auto field = value_document->modifier_field(*property);
	if(value_document->value(field) != expected) { 
		error = "Shared property changed on disk; reload before saving"; 
		return false; 
	}
	if(!editable(*value_document, field, expected, error) ||
	   (!preview && !editable(*value_document, field, value, error))) {
		return false;
	}

	size_t local_count = 0, local_field = 0;
	for(size_t f = 0; f < document->records()[record].fields.size(); ++f) {
		const auto &local = document->records()[record].fields[f];
		if(!local.native_modifier || local.property_id != number) { 
			continue;
		}
		++local_count;
		if(local.offset == property->value_offset) {
			local_field = f;
		}
	}

	size_t restore_slot = 0;
	std::vector<uint8_t> restore_payload;
	if((proved || restoring) && !shared_removal(removal, static_cast<uint32_t>(number), restore_slot, restore_payload, error)) { 
		return false;
	}

	if(inserting) {
		if(local_count) { 
			error = "A local modifier with this ID already exists, possibly shadowed by inheritance; edit it explicitly"; 
			return false; 
		}
		
		if(restoring) {
			if(shared_order(document->records()[record]) != removal.order) {
				error = "Native modifier ordering changed; reload before restoring the override"; 
				return false;
			}
			if(!document->insert_property(record, restore_slot, restore_payload, error, false)) { 
				return false;
			}
		} else if(!document->insert_property(record, static_cast<uint32_t>(number), value, error, false)) { 
			return false;
		}

	} else if(removing) {
		if(local_count != 1) {
			error = "Override structure changed; only a unique owned modifier can be undone"; return false;
		}

		const auto payload = shared_hex(document->bytes().data() + property->value_offset - 4, 36);
		const auto slot = (property->value_offset - selection->data_offset - 8) / 36;

		if(preview) {
			removal.payload = payload; removal.slot = std::to_string(slot);
		} else if(proved) {
			if(payload != removal.payload || slot != restore_slot) {
				error = "Override payload or position changed; reload before removing it"; 
				return false;
			}
		} else {
			const size_t used = field.kind == edit::ValueKind::text ? expected.size() + 1 : field.size;
			if(used > 32 || std::any_of(property->value + used, property->value + 32, [](uint8_t byte) { return byte != 0; })) {
				error = "Override contains additional native payload data; preview removal to preserve it"; 
				return false;
			}
		}

		if(!document->remove_property(record, local_field, error)) { 
			return false;
		}

		const auto order = shared_order(document->records()[record]);
		if(preview) { 
			removal.order = order; 
		} else if(proved && order != removal.order) {
			error = "Native modifier ordering changed; reload before removing the override"; 
			return false;
		}

	} else if(!document->edit(record, local_field, value, error)) { 
		return false;
	}

	auto snapshots = database.snapshots();
	for(auto &snapshot : snapshots) {
		if(snapshot.name == archive) {
			snapshot.bytes = document->bytes(); 
		}
	}

	NOTDatabase candidate;
	if(!candidate.loadBuffers(snapshots, error)) { 
		return false; 
	}
	
	const auto *changed = candidate.findTemplate(name.c_str());
	std::vector<NOTParser::ResolvedProperty> result;
	if(candidate.resolveModifiers(name.c_str(), result, error) != NOTParser::ResolveStatus::ok) { 
		return false; 
	}

	if(operation != SharedOperation::set) {

		bool verified = false;
		for(const auto &entry : result) {
			if(entry.property->prop_id != number) { 
				continue;
			}

			if(inserting) {
				verified = entry.owner == changed;
				if(verified && restoring) { 
					verified = document->value(document->modifier_field(*entry.property)) == value; 
				}
			} else if(entry.owner != changed && (preview || matches_inherited(candidate, entry))) {
				edit::Document parent;

				const auto *parent_source = candidate.source(entry.owner);
				if(!parent.load_bytes(parent_source->bytes, edit::Format::not_file, error)) { 
					return false; 
				}

				const auto parent_field = parent.modifier_field(*entry.property);
				const auto parent_value = parent.value(parent_field);
				verified = preview ? editable(parent, parent_field, parent_value, error) : parent_value == value;
			}
		}

		if(!verified) { 
			error = "Override result no longer matches the expected inherited owner/value; reload metadata"; 
			return false; 
		}
	}
	for(const auto &entry : database.templates()) {
		std::vector<NOTParser::ResolvedProperty> previous, properties;
		std::string issue;
		if(database.resolveModifiers(entry.first.c_str(), previous, issue) != NOTParser::ResolveStatus::ok) { 
			continue;
		}

		if(candidate.resolveModifiers(entry.first.c_str(), properties, issue) != NOTParser::ResolveStatus::ok) {
			error = "Shared change would make " + entry.first + " unresolved: " + issue; 
			return false;
		}

		if(number == 6 || number == 7) {
			const NODModifier *minimum = nullptr, *maximum = nullptr;
			bool affected = false;
			for(const auto &before : previous) {
				if(before.property->prop_id == number && before.owner == selection) { 
					affected = true;
				}
			}

			for(const auto &resolved : properties) {
				if(resolved.property->prop_id == 6) minimum = resolved.property;
				if(resolved.property->prop_id == 7) maximum = resolved.property;
				if(resolved.property->prop_id == number && resolved.owner == changed) affected = true;
			}

			if(!affected) continue;
			float mins[3], maxs[3];

			if(!minimum || !maximum) {
				error = "Shared bounds need both MIN and MAX on " + entry.first; 
				return false; 
			}

			memcpy(mins, minimum->value, sizeof(mins)); 
			memcpy(maxs, maximum->value, sizeof(maxs));

			for(unsigned axis = 0; axis < 3; ++axis)
				if(!std::isfinite(mins[axis]) || !std::isfinite(maxs[axis]) || mins[axis] >= maxs[axis] ||
				   mins[axis] < -65536 || maxs[axis] > 65536) {
					error = "Shared bounds would be invalid on " + entry.first; 
					return false;
				}
		}
	}

	database = std::move(candidate);
	
	if(document->dirty() && !preview) {
		pending = std::move(document);
	}

	return true;
}
}
#endif
