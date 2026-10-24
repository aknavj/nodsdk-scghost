/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: document.h
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

#ifndef NOD_TOOLS_EDITOR_DOCUMENT_H
#define NOD_TOOLS_EDITOR_DOCUMENT_H

#include "NOTDatabase.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
	
/*
 */
namespace nod::edit {

/*
 */
bool encode_modifier(uint32_t id, const std::string &value, NODModifier &modifier, std::string &error);
	
/*
 */
enum class Format { 
	nsd, 
	not_file 
};
	
/*
 */
enum class ValueKind { 
	float32, 
	uint32, 
	vector3, 
	text, 
	raw 
};
	
/*
 */
struct Field {
	std::string name;
	ValueKind kind;
	size_t offset;
	size_t size;
	bool editable;
	std::string description;
	uint32_t property_id = UINT32_MAX;
	bool native_modifier = false;
};
	
/*
 */
struct Record {
	std::string name;
	std::string type;
	std::string parent;
	size_t offset = 0;
	size_t size = 0;
	int parent_index = -1;
	std::vector<Field> fields;
	struct EffectiveProperty {
		Field field;
		std::string source;
		std::string external_value;
		size_t record;
		size_t field_index;
		bool external;
		std::string archive{};
	};
	std::vector<EffectiveProperty> effective;
};
	
/*
 */
class Document {

	public:
		
		bool load(const std::filesystem::path &path, Format format, std::string &error);
		bool create_not_archive(const std::string &root_name, std::string &error);
		bool configure_workspace(const std::filesystem::path &workspace, const std::filesystem::path &assets,
								std::string &error);
		std::filesystem::path suggested_save_path() const;
		const auto &workspace() const { return workspace_; }
		bool load_bytes(const std::vector<uint8_t> &bytes, Format format, std::string &error);
		bool set_template_bytes(const std::vector<uint8_t> &bytes, std::string &error);
		bool set_template_archives(const std::vector<NOTDatabase::Snapshot> &archives, std::string &error);
		const auto &scene_archives() const { return referenced_templates_; }
		bool has_template_context() const { return !template_archives_.empty(); }

		// size_t(-1) means the end of the declared NSD archive list.
		bool insert_scene_archive(size_t before, const std::string &name, std::string &error);
		bool remove_scene_archive(size_t slot, std::string &error);
		bool move_scene_archive(size_t slot, size_t before, std::string &error);
		struct PlacementTemplate { std::string name, archive; };
		bool placement_templates(std::vector<PlacementTemplate> &choices, std::string &error) const;
		bool reassign_placement_template(size_t record, const std::string &name, std::string &error);
		struct PlacementModifier { Field field; bool protected_prefix; };
		bool placement_modifiers(size_t record, std::vector<PlacementModifier> &modifiers, std::string &error) const;
		
		// Slots are serialized modifier ordinals; size_t(-1) means the end.
		bool insert_placement_reference(size_t record, size_t before, const std::string &name, std::string &error);
		bool replace_placement_reference(size_t record, size_t slot, const std::string &name, std::string &error);
		bool remove_placement_reference(size_t record, size_t slot, std::string &error);
		bool move_placement_reference(size_t record, size_t slot, size_t before, std::string &error);
		bool edit(size_t record, size_t field, const std::string &value, std::string &error);
		
		// Defer local inheritance checks only when the caller validates the complete ordered NOT context before saving.
		bool insert_property(size_t record, uint32_t property_id, const std::string &value, std::string &error,
							bool check_inheritance = true);
		
		// Insert a complete typed NOT modifier without re-encoding its payload; size_t(-1) appends.
		bool insert_property(size_t record, size_t before, const std::vector<uint8_t> &property, std::string &error,
							bool check_inheritance = true);
		bool override_property(size_t record, uint32_t property_id, std::string &error);
		bool remove_property(size_t record, size_t field, std::string &error);
		bool removable_property(size_t record, size_t field) const;
		bool create_derived_template(size_t parent, const std::string &name, std::string &error);
		bool duplicate_template(size_t source, const std::string &name, std::string &error);
		bool remove_template(size_t record, std::string &error);
		bool rename_template(size_t record, const std::string &name, std::string &error);
		
		// size_t(-1) inserts a first basedon before local modifiers; otherwise only that field is changed.
		bool reparent_template(size_t record, size_t reference_field, size_t parent, std::string &error);
		
		// size_t(-1) means the end of the modifier array for insertion and movement.
		bool insert_template_reference(size_t record, size_t before_field, size_t parent, std::string &error);
		bool remove_template_reference(size_t record, size_t field, std::string &error);
		bool move_template_reference(size_t record, size_t field, size_t before_field, std::string &error);
		bool save(const std::filesystem::path &path, bool overwrite, std::string &error);
		bool undo(std::string *error = nullptr);
		bool redo(std::string *error = nullptr);
		bool dirty() const { return bytes_ != saved_; }
		bool can_undo() const { return cursor_ != 0; }
		bool can_redo() const { return cursor_ < history_.size(); }

		std::string value(const Field &field) const;
		Field modifier_field(const NODModifier &modifier) const;
		
		bool validate_value(const Field &field, const std::string &value, std::string &error) const;

		const auto &bytes() const { return bytes_; }
		const auto &saved_bytes() const { return saved_; }
		const auto &records() const { return records_; }
		const auto &warnings() const { return warnings_; }
		const auto &path() const { return path_; }
		Format format() const { return format_; }

	private:
		struct Change {
			size_t offset;
			std::vector<uint8_t> before;
			std::vector<uint8_t> after;
			bool structural = false;
			std::vector<NOTDatabase::Snapshot> archives_before{}, archives_after{};
		};

		bool commit_scene_archives(std::vector<uint8_t> candidate, std::string &error);
		bool commit_nsd_structure(Document &&validation);
		bool nsd_modifiers(size_t record, std::vector<NODModifier> &modifiers, std::string &error) const;
		bool placement_reference(const std::string &name, NODModifier &modifier, std::string &error) const;
		bool validate_placement_inheritance(const std::vector<NODModifier> &before,
										const std::vector<NODModifier> &after, std::string &error) const;
		bool commit_placement_references(size_t record, const std::vector<NODModifier> &before,
										const std::vector<NODModifier> &after, std::string &error);
		bool commit_nsd_placement_change(size_t record, std::vector<uint8_t> candidate, std::string &error);
		bool splice_nsd_property(size_t record, size_t offset, const std::vector<uint8_t> &property, std::string &error);
		bool splice_not_property(size_t record, size_t offset, const std::vector<uint8_t> &property, std::string &error,
								bool check_inheritance = false);
		bool append_template(const std::string &name, const std::vector<uint8_t> &payload, std::string &error);
		bool commit_not_structure(std::vector<uint8_t> candidate, std::string &error, size_t inheritance_root = size_t(-1));
		bool validate_local_inheritance(size_t root, std::string &error) const;
		bool not_modifier_offset(size_t record, size_t field, size_t &offset, std::string &error) const;
		bool apply_history(const Change &change, bool forward, std::string *error);
		bool index_nsd(std::string &error);
		bool index_not(std::string &error);
		void link_parents();
		bool same_layout(const Document &other) const;

		Format format_ = Format::nsd;
		std::filesystem::path path_;
		std::filesystem::path workspace_, assets_;
		std::vector<uint8_t> bytes_;
		std::vector<uint8_t> saved_;
		std::vector<NOTDatabase::Snapshot> template_archives_;
		std::vector<std::string> referenced_templates_;
		std::vector<Record> records_;
		std::vector<std::string> warnings_;
		std::vector<Change> history_;
		size_t cursor_ = 0;
};

} // namespace nod::edit
#endif
