/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: editor_app.cpp
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

#include "editor_app.h"
#include "tool_cli.h"
#include "NSDParser.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace nod::edit {
namespace {

constexpr size_t no_selection = size_t(-1);

void copy_text(char *destination, size_t capacity, const std::string &text) {
	std::snprintf(destination, capacity, "%s", text.c_str());
}

const char *kind_name(ValueKind kind) {
	switch(kind) {
		case ValueKind::float32:
			return "float32";
		case ValueKind::uint32:
			return "uint32";
		case ValueKind::vector3:
			return "float32 x3";
		case ValueKind::text:
			return "ASCII";
		case ValueKind::raw:
			return "raw hex";
	}

	return "unknown";
}

class Editor {
	public:
		explicit Editor(Format format, const std::filesystem::path &workspace, const std::filesystem::path &assets,
						bool smoke = false)
			: format_(format)
			, smoke_(smoke) {
			if(!workspace.empty()) {
				std::string error;
				if(!document_.configure_workspace(workspace, assets, error)) {
					workspace_failed_ = true;
					report(error);
				}
			}
		}

		bool load(const std::string &path) {
			if(workspace_failed_) {
				report("The workspace policy could not be established; native editing is disabled.");
				return false;
			}

			std::string error;
			if(!document_.load(std::filesystem::u8path(path), format_, error)) {
				report(error);
				return false;
			}

			loaded_						 = true;
			inheritance_rendered_		 = false;
			archive_files_rendered_		 = false;
			effective_rendered_			 = false;
			property_add_rendered_		 = false;
			placement_template_rendered_ = false;
			placement_order_rendered_	 = false;
			show_archive_files_			 = smoke_ && format_ == Format::nsd;
			selected_archive_			 = no_selection;
			archive_remove_pending_		 = false;
			selected_record_			 = document_.records().empty() ? no_selection : 0;
			selected_field_				 = no_selection;
			selected_placement_modifier_ = no_selection;
			page_						 = 0;
			rebuild_tree();
			status_ = "Loaded " + std::to_string(document_.records().size()) + " records; original bytes retained.";
			error_.clear();
			return true;
		}

		void draw(GLFWwindow *window) {
			const auto &io = ImGui::GetIO();
			if(glfwWindowShouldClose(window)) {
				glfwSetWindowShouldClose(window, GLFW_FALSE);
				request(Action::quit);
			}

			if(io.KeyCtrl && !ImGui::IsAnyItemActive() && !show_remove_property_ && !show_new_archive_ &&
			   !show_add_property_ && !show_archive_files_ && !show_placement_template_ &&
			   template_action_ == TemplateAction::none) {
				if(format_ == Format::not_file && ImGui::IsKeyPressed(ImGuiKey_N)) {
					request(Action::new_archive);
				}

				if(ImGui::IsKeyPressed(ImGuiKey_O)) {
					request(Action::open);
				}

				if(ImGui::IsKeyPressed(ImGuiKey_S)) {
					begin_save(io.KeyShift);
				}

				if(ImGui::IsKeyPressed(ImGuiKey_Z)) {
					history(false);
				}

				if(ImGui::IsKeyPressed(ImGuiKey_Y)) {
					history(true);
				}
			}

			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(io.DisplaySize);
			ImGui::Begin(format_ == Format::nsd ? "NSD Scene Editor" : "NOT Template Editor", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
			if(format_ == Format::not_file) {
				ImGui::BeginDisabled(workspace_failed_);
				if(ImGui::Button("New archive...")) {
					request(Action::new_archive);
				}

				ImGui::EndDisabled();
				ImGui::SameLine();
			}

			if(ImGui::Button("Open...")) {
				request(Action::open);
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!loaded_);
			if(ImGui::Button(document_.workspace().empty() ? "Save" : "Save workspace copy")) {
				begin_save(false);
			}

			ImGui::SameLine();
			if(ImGui::Button("Save As...")) {
				begin_save(true);
			}

			if(format_ == Format::nsd) {
				ImGui::SameLine();
				if(ImGui::Button("Template files...")) {
					show_archive_files_ = true;
				}
			}

			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!document_.can_undo());
			if(ImGui::Button("Undo")) {
				history(false);
			}

			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!document_.can_redo());
			if(ImGui::Button("Redo")) {
				history(true);
			}

			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextUnformatted(document_.dirty() ? "* Modified" : "Unmodified");
			if(loaded_) {
				ImGui::TextUnformatted(document_.path().empty() ? "Untitled NOT archive (not yet saved)"
																: document_.path().u8string().c_str());
			} else {
				ImGui::TextUnformatted("Open a file using Open, a command-line argument, or drag and drop.");
			}

			if(!document_.workspace().empty()) {
				ImGui::TextWrapped("Save workspace: %s", document_.workspace().u8string().c_str());
			}

			ImGui::Separator();
			const float height = std::max(100.0f, ImGui::GetContentRegionAvail().y - 32.0f);
			ImGui::BeginChild("Tree", ImVec2(320, height), true);
			filter_.Draw("Search", -1);
			if(format_ == Format::not_file) {
				ImGui::BeginDisabled(!loaded_ || selected_record_ >= document_.records().size());
				if(ImGui::Button("New child...")) {
					begin_template(TemplateAction::derive);
				}

				ImGui::SameLine();
				if(ImGui::Button("Duplicate...")) {
					begin_template(TemplateAction::duplicate);
				}

				if(ImGui::Button("Delete template...")) {
					begin_template(TemplateAction::remove);
				}

				if(ImGui::Button("Rename...")) {
					begin_template(TemplateAction::rename);
				}

				ImGui::SameLine();
				if(ImGui::Button("Reparent...")) {
					begin_template(TemplateAction::reparent);
				}

				ImGui::EndDisabled();
			}

			if(loaded_) {
				draw_tree();
			}

			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("Details", ImVec2(0, height), true);
			if(loaded_ && selected_record_ < document_.records().size()) {
				draw_details();
			} else if(loaded_) {
				ImGui::TextUnformatted(
					"No verified placements were found; the file remains available for raw inspection.");
				for(const auto &warning : document_.warnings()) {
					ImGui::TextWrapped("%s", warning.c_str());
				}

				Record file;
				file.size = document_.bytes().size();
				draw_raw(file);
			}

			ImGui::EndChild();
			ImGui::TextUnformatted(status_.c_str());
			draw_dialogs(window);
			ImGui::End();
		}

		void drop(const char *path) {
			copy_text(open_path_, sizeof(open_path_), path);
			request(Action::drop);
		}

		bool quit() const { return quit_; }
		bool failed() const { return !error_.empty(); }
		bool smoke_view_ready() const {
			return format_ == Format::not_file
					   ? inheritance_rendered_
					   : archive_files_rendered_ &&
							 (document_.records().empty() ||
							  (effective_rendered_ && placement_order_rendered_ && property_add_rendered_ &&
							   (!document_.has_template_context() || placement_template_rendered_)));
		}

	private:
		enum class Action { none, new_archive, open, drop, quit };
		enum class TemplateAction {
			none,
			derive,
			duplicate,
			remove,
			rename,
			reparent,
			insert_reference,
			remove_reference
		};
		enum class PlacementAction { primary, insert, replace, remove };

		void report(const std::string &error) {
			error_ = error;
			std::fprintf(stderr, "Editor: %s\n", error.c_str());
		}

		void request(Action action) {
			if(pending_ != Action::none || show_open_ || show_save_ || confirm_overwrite_ || show_remove_property_ ||
			   show_new_archive_ || show_add_property_ || show_archive_files_ || show_placement_template_ ||
			   template_action_ != TemplateAction::none) {
				return;
			}

			if(action != Action::quit && !error_.empty()) {
				return;
			}

			if(action == Action::new_archive && workspace_failed_) {
				report("The workspace policy could not be established; native editing is disabled.");
				return;
			}

			pending_ = action;
			if(!document_.dirty()) {
				perform_action();
			}
		}

		void perform_action() {
			const auto action = pending_;
			pending_		  = Action::none;
			if(action == Action::quit) {
				quit_ = true;
			} else if(action == Action::new_archive) {
				copy_text(template_name_, sizeof(template_name_), "root");
				show_new_archive_ = true;
			} else if(action == Action::drop) {
				load(open_path_);
			} else if(action == Action::open) {
				show_open_ = true;
			}
		}

		void begin_save(bool save_as) {
			if(!loaded_ || pending_ != Action::none) {
				return;
			}

			const auto suggested = document_.suggested_save_path();
			if(save_as || document_.path().empty() || suggested != document_.path()) {
				auto path = suggested;
				if(save_as && path == document_.path() && !path.empty()) {
					path = path.parent_path() / (path.stem().u8string() + "_edited" + path.extension().u8string());
				}

				copy_text(save_path_, sizeof(save_path_), path.u8string());
				show_save_ = true;
			} else {
				std::string error;
				if(!document_.save(document_.path(), false, error)) {
					report(error);
				} else {
					status_ = "Saved native edits; unrelated data was preserved.";
				}
			}
		}

		bool save_as(bool overwrite) {
			std::string error;
			if(!document_.save(std::filesystem::u8path(save_path_), overwrite, error)) {
				report(error);
				return false;
			}

			status_ = "Saved " + document_.path().u8string();
			return true;
		}

		void refresh_edit() {
			if(selected_record_ < document_.records().size() &&
			   selected_field_ < document_.records()[selected_record_].fields.size()) {
				copy_text(edit_, sizeof(edit_),
						  document_.value(document_.records()[selected_record_].fields[selected_field_]));
			}
		}

		void history(bool redo) {
			const size_t size = document_.bytes().size();
			const std::string selected_name =
				selected_record_ < document_.records().size() ? document_.records()[selected_record_].name : "";
			std::string error;
			if(redo ? document_.redo(&error) : document_.undo(&error)) {
				selected_archive_			 = no_selection;
				selected_placement_modifier_ = no_selection;
				archive_remove_pending_		 = false;
				if(size != document_.bytes().size()) {
					selected_field_ = no_selection;
				}

				rebuild_tree(selected_name);
				refresh_edit();
				page_	= 0;
				status_ = redo ? "Redid native edit." : "Undid native edit.";
			} else if(!error.empty()) {
				report(error);
			}
		}

		void rebuild_tree(const std::string &selected_name = "") {
			const auto &records = document_.records();
			children_.assign(records.size(), {});
			for(size_t i = 0; i < records.size(); ++i) {
				if(records[i].parent_index >= 0) {
					children_[size_t(records[i].parent_index)].push_back(i);
				}

				if(!selected_name.empty() && records[i].name == selected_name) {
					selected_record_ = i;
				}
			}

			if(records.empty()) {
				selected_record_ = no_selection;
			} else if(selected_record_ >= records.size()) {
				selected_record_ = records.size() - 1;
			}
		}

		void begin_template(TemplateAction action) {
			if(template_action_ != TemplateAction::none || selected_record_ >= document_.records().size()) {
				return;
			}

			template_action_ = action;
			template_record_ = selected_record_;
			copy_text(template_name_, sizeof(template_name_),
					  action == TemplateAction::rename ? document_.records()[selected_record_].name : "");
			template_parent_	= no_selection;
			template_reference_ = no_selection;
			template_before_	= no_selection;
			const auto &fields	= document_.records()[selected_record_].fields;
			for(size_t i = 0; i < fields.size(); ++i) {
				if(fields[i].property_id == 0) {
					template_reference_ = i;
					break;
				}
			}

			external_references_acknowledged_ = false;
		}

		void select_record(size_t index) {
			selected_record_			 = index;
			selected_field_				 = no_selection;
			selected_placement_modifier_ = no_selection;
			page_						 = 0;
		}

		void draw_tree_node(size_t index, const std::vector<bool> &visible) {
			if(!visible[index]) {
				return;
			}

			const auto &record = document_.records()[index];
			ImGui::PushID(static_cast<int>(index));
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if(index == selected_record_) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			if(children_[index].empty()) {
				flags |= ImGuiTreeNodeFlags_Leaf;
			}

			if(filter_.IsActive()) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
			}

			const bool expanded = ImGui::TreeNodeEx(record.name.c_str(), flags);
			if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				select_record(index);
			}

			if(expanded) {
				for(const auto child : children_[index]) {
					draw_tree_node(child, visible);
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		void draw_tree() {
			const auto &records = document_.records();
			if(format_ == Format::nsd) {
				std::map<std::string, std::vector<size_t>> groups;
				for(size_t i = 0; i < records.size(); ++i) {
					if(filter_.PassFilter(records[i].name.c_str())) {
						groups[records[i].type].push_back(i);
					}
				}

				for(const auto &group : groups) {
					if(filter_.IsActive()) {
						ImGui::SetNextItemOpen(true, ImGuiCond_Always);
					}

					if(ImGui::TreeNode(group.first.c_str())) {
						for(const auto index : group.second) {
							ImGui::PushID(static_cast<int>(index));
							if(ImGui::Selectable(records[index].name.c_str(), index == selected_record_)) {
								select_record(index);
							}

							ImGui::PopID();
						}

						ImGui::TreePop();
					}
				}
			} else {
				std::vector<bool> visible(records.size(), false);
				for(size_t i = 0; i < records.size(); ++i) {
					if(!filter_.PassFilter(records[i].name.c_str())) {
						continue;
					}

					int current = static_cast<int>(i);
					while(current >= 0 && !visible[size_t(current)]) {
						visible[size_t(current)] = true;
						current					 = records[size_t(current)].parent_index;
					}
				}

				for(size_t i = 0; i < records.size(); ++i) {
					if(records[i].parent_index < 0) {
						draw_tree_node(i, visible);
					}
				}
			}
		}

		void draw_details() {
			const auto &record = document_.records()[selected_record_];
			ImGui::TextUnformatted(record.name.c_str());
			ImGui::Text("Offset 0x%zX | %zu bytes", record.offset, record.size);
			if(format_ == Format::nsd) {
				ImGui::BeginDisabled(!document_.has_template_context());
				if(ImGui::Button("Change placement template...")) {
					begin_placement_template();
				}

				ImGui::EndDisabled();
			}

			if(ImGui::BeginTabBar("Views")) {
				if(ImGui::BeginTabItem("Properties")) {
					draw_properties();
					ImGui::EndTabItem();
				}

				if(ImGui::BeginTabItem("Effective properties", nullptr,
									   smoke_ && format_ == Format::nsd && !effective_rendered_
										   ? ImGuiTabItemFlags_SetSelected
										   : ImGuiTabItemFlags_None)) {
					draw_inherited();
					ImGui::EndTabItem();
				}

				if(format_ == Format::not_file &&
				   ImGui::BeginTabItem("Inheritance order", nullptr,
									   smoke_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
					draw_inheritance_order();
					ImGui::EndTabItem();
				}

				if(format_ == Format::nsd &&
				   ImGui::BeginTabItem("Inheritance order", nullptr,
									   smoke_ && effective_rendered_ ? ImGuiTabItemFlags_SetSelected
																	 : ImGuiTabItemFlags_None)) {
					draw_placement_inheritance();
					ImGui::EndTabItem();
				}

				if(ImGui::BeginTabItem("Raw bytes (read-only)")) {
					draw_raw(record);
					ImGui::EndTabItem();
				}

				if(ImGui::BeginTabItem("Changes")) {
					draw_changes();
					ImGui::EndTabItem();
				}

				if(ImGui::BeginTabItem("Diagnostics")) {
					for(const auto &warning : document_.warnings()) {
						ImGui::BulletText("%s", warning.c_str());
					}

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}

		void begin_property() {
			add_record_			   = selected_record_;
			new_property_id_	   = UINT32_MAX;
			new_property_value_[0] = '\0';
			property_filter_.Clear();
			show_add_property_ = true;
		}

		void begin_placement_template(PlacementAction action = PlacementAction::primary, size_t slot = no_selection) {
			std::string error;
			if(!document_.placement_templates(placement_choices_, error)) {
				report(error);
				return;
			}

			placement_record_	 = selected_record_;
			placement_action_	 = action;
			placement_slot_		 = slot;
			placement_choice_	 = no_selection;
			placement_confirmed_ = false;
			placement_filter_.Clear();
			show_placement_template_ = true;
		}

		void draw_placement_inheritance() {
			placement_order_rendered_ = true;
			ImGui::TextWrapped(
				"Later modifiers win. Primary reference, transforms and sector/region prefix are protected.");
			std::vector<Document::PlacementModifier> modifiers;
			std::string error;
			if(!document_.placement_modifiers(selected_record_, modifiers, error)) {
				ImGui::TextWrapped("%s", error.c_str());
				return;
			}

			if(selected_placement_modifier_ >= modifiers.size()) {
				selected_placement_modifier_ = no_selection;
			}

			if(ImGui::BeginTable("Placement modifier order", 4,
								 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
									 ImGuiTableFlags_ScrollY,
								 ImVec2(0, std::max(120.0f, ImGui::GetContentRegionAvail().y - 160.0f)))) {
				ImGui::TableSetupColumn("Order");
				ImGui::TableSetupColumn("Modifier");
				ImGui::TableSetupColumn("Value");
				ImGui::TableSetupColumn("Role");
				ImGui::TableHeadersRow();
				for(size_t i = 0; i < modifiers.size(); ++i) {
					const auto &entry = modifiers[i];
					ImGui::PushID(static_cast<int>(i));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if(ImGui::Selectable(std::to_string(i + 1).c_str(), selected_placement_modifier_ == i,
										 ImGuiSelectableFlags_SpanAllColumns)) {
						selected_placement_modifier_ = i;
					}

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(entry.field.name.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(document_.value(entry.field).c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(entry.protected_prefix		  ? "Protected prefix"
										   : entry.field.property_id == 0 ? "Secondary basedon"
																		  : "Local value");
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			const auto slot = selected_placement_modifier_;
			const bool reference =
				slot < modifiers.size() && !modifiers[slot].protected_prefix && modifiers[slot].field.property_id == 0;
			ImGui::BeginDisabled(!document_.has_template_context());
			if(ImGui::Button("Insert secondary reference...")) {
				begin_placement_template(PlacementAction::insert,
										 slot < modifiers.size() && !modifiers[slot].protected_prefix ? slot
																									  : no_selection);
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!reference);
			if(ImGui::Button("Change reference...")) {
				begin_placement_template(PlacementAction::replace, slot);
			}

			ImGui::SameLine();
			if(ImGui::Button("Remove reference...")) {
				begin_placement_template(PlacementAction::remove, slot);
			}

			ImGui::EndDisabled();
			ImGui::BeginDisabled(!reference || slot == 0 ||
								 (slot < modifiers.size() && modifiers[slot - 1].protected_prefix));
			const bool earlier = ImGui::Button("Move reference earlier");
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!reference || slot + 1 >= modifiers.size());
			const bool later = ImGui::Button("Move reference later");
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			ImGui::TextWrapped(
				"Insert before selection; protected/no row appends. Requires resolved, same-class templates.");
			if(earlier || later) {
				const size_t before = earlier ? slot - 1 : slot + 2;
				if(!document_.move_placement_reference(selected_record_, slot, before, error)) {
					report(error);
				} else {
					selected_placement_modifier_ = earlier ? slot - 1 : slot + 1;
					selected_field_				 = no_selection;
					page_						 = 0;
					refresh_edit();
					status_ = "Moved complete basedon modifier; effective precedence refreshed.";
				}
			}
		}

		void draw_properties() {
			const auto &record = document_.records()[selected_record_];
			ImGui::Checkbox("Enable raw property payload editing (unknown semantics)", &raw_enabled_);
			if(ImGui::Button("Add local property...")) {
				begin_property();
			}

			const float table_height = std::max(120.0f, ImGui::GetContentRegionAvail().y - 210.0f);
			if(ImGui::BeginTable("Properties", 5,
								 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
									 ImGuiTableFlags_ScrollY,
								 ImVec2(0, table_height))) {
				ImGui::TableSetupColumn("Property");
				ImGui::TableSetupColumn("Value");
				ImGui::TableSetupColumn("Storage");
				ImGui::TableSetupColumn("Offset");
				ImGui::TableSetupColumn("Access");
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();
				for(size_t i = 0; i < record.fields.size(); ++i) {
					const auto &field = record.fields[i];
					ImGui::PushID(static_cast<int>(i));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if(ImGui::Selectable(field.name.c_str(), selected_field_ == i,
										 ImGuiSelectableFlags_SpanAllColumns)) {
						selected_field_ = i;
						refresh_edit();
					}

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(document_.value(field).c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%s [%zu]", kind_name(field.kind), field.size);
					ImGui::TableNextColumn();
					ImGui::Text("0x%zX", field.offset);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(field.editable ? "Editable" : "Read-only");
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if(selected_field_ >= record.fields.size()) {
				ImGui::TextUnformatted("Select a property to inspect or edit it. Edits require Apply.");
				return;
			}

			const auto &field = record.fields[selected_field_];
			ImGui::Separator();
			ImGui::TextUnformatted(field.name.c_str());
			ImGui::TextWrapped("%s", field.description.c_str());
			const bool editable = field.editable && (field.kind != ValueKind::raw || raw_enabled_);
			ImGui::BeginDisabled(!editable);
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##Value", edit_, sizeof(edit_));
			if(ImGui::Button("Apply")) {
				std::string error;
				if(!document_.edit(selected_record_, selected_field_, edit_, error)) {
					report(error);
				} else {
					status_ = "Applied " + field.name;
					refresh_edit();
				}
			}

			ImGui::EndDisabled();
			ImGui::SameLine();
			if(ImGui::Button("Reset input")) {
				refresh_edit();
			}

			if(document_.removable_property(selected_record_, selected_field_)) {
				ImGui::SameLine();
				if(ImGui::Button("Remove local property...")) {
					remove_record_		  = selected_record_;
					remove_field_		  = selected_field_;
					show_remove_property_ = true;
				}
			}
		}

		std::vector<size_t> modifier_fields(size_t record) const {
			std::vector<size_t> modifiers;
			const auto &fields = document_.records()[record].fields;
			for(size_t i = 0; i < fields.size(); ++i) {
				if(fields[i].native_modifier) {
					modifiers.push_back(i);
				}
			}

			return modifiers;
		}

		void draw_inheritance_order() {
			inheritance_rendered_ = true;
			ImGui::TextWrapped("Later values win. Move basedon references to change override order.");
			const auto modifiers = modifier_fields(selected_record_);
			const auto &fields	 = document_.records()[selected_record_].fields;
			const float height	 = std::max(120.0f, ImGui::GetContentRegionAvail().y - 130.0f);
			if(ImGui::BeginTable("Modifier order", 4,
								 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
									 ImGuiTableFlags_ScrollY,
								 ImVec2(0, height))) {
				ImGui::TableSetupColumn("Order");
				ImGui::TableSetupColumn("Modifier");
				ImGui::TableSetupColumn("Value");
				ImGui::TableSetupColumn("Role");
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();
				for(size_t i = 0; i < modifiers.size(); ++i) {
					const auto field = modifiers[i];
					ImGui::PushID(static_cast<int>(field));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const std::string order = std::to_string(i + 1);
					if(ImGui::Selectable(order.c_str(), selected_field_ == field,
										 ImGuiSelectableFlags_SpanAllColumns)) {
						selected_field_ = field;
						refresh_edit();
					}

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(fields[field].name.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(document_.value(fields[field]).c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(fields[field].property_id == 0 ? "basedon" : "Local value");
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			const auto selected	  = std::find(modifiers.begin(), modifiers.end(), selected_field_);
			const size_t position = size_t(selected - modifiers.begin());
			const bool reference  = selected != modifiers.end() && fields[selected_field_].property_id == 0;
			if(ImGui::Button("Insert base reference...")) {
				begin_template(TemplateAction::insert_reference);
				template_before_ = selected != modifiers.end() ? selected_field_ : no_selection;
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!reference);
			if(ImGui::Button("Remove reference...")) {
				begin_template(TemplateAction::remove_reference);
				template_reference_ = selected_field_;
			}

			ImGui::EndDisabled();
			ImGui::BeginDisabled(!reference || position == 0);
			const bool earlier = ImGui::Button("Move earlier");
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!reference || position + 1 >= modifiers.size());
			const bool later = ImGui::Button("Move later");
			ImGui::EndDisabled();
			ImGui::TextWrapped(
				"Local cycles and property limits checked; external archives unchecked. See Diagnostics.");
			if(earlier || later) {
				const size_t before = earlier							? modifiers[position - 1]
									  : position + 2 < modifiers.size() ? modifiers[position + 2]
																		: no_selection;
				std::string error;
				if(!document_.move_template_reference(selected_record_, selected_field_, before, error)) {
					report(error);
				} else {
					selected_field_ = modifiers[earlier ? position - 1 : position + 1];
					rebuild_tree();
					refresh_edit();
					page_	= 0;
					status_ = "Moved basedon; source-order precedence and effective values refreshed.";
				}
			}
		}

		void draw_inherited() {
			effective_rendered_ = true;
			ImGui::TextWrapped(
				"References expand in order; later values win. Resolution errors appear in Diagnostics.");
			const auto &effective = document_.records()[selected_record_].effective;
			if(format_ == Format::nsd) {
				ImGui::TextWrapped(
					"First-loaded templates win. Make local override copies a value. Reopen NSD to reload archives.");
			}

			uint32_t override_id = UINT32_MAX;
			const bool templates = format_ == Format::not_file;
			if(ImGui::BeginTable("Effective", 4,
								 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("Property");
				ImGui::TableSetupColumn("Value");
				ImGui::TableSetupColumn("Source");
				ImGui::TableSetupColumn("Local override");
				ImGui::TableHeadersRow();
				for(const auto &entry : effective) {
					const auto &field = entry.field;
					ImGui::PushID(static_cast<int>(field.property_id));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(field.name.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted((entry.external ? entry.external_value : document_.value(field)).c_str());
					ImGui::TableNextColumn();
					if(entry.external) {
						ImGui::TextUnformatted(entry.source.c_str());
					} else if(ImGui::SmallButton(entry.source.c_str())) {
						select_record(entry.record);
						selected_field_ = entry.field_index;
						refresh_edit();
					}

					{
						ImGui::TableNextColumn();
						if(templates ? entry.record == selected_record_ : !entry.external) {
							ImGui::TextUnformatted("Local");
						} else if(field.property_id == 0 || field.property_id == UINT32_MAX ||
								  (!templates && field.property_id <= 3)) {
							ImGui::TextUnformatted("Protected");
						} else if(ImGui::SmallButton("Make local override")) {
							override_id = field.property_id;
						}
					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			if(override_id != UINT32_MAX) {
				std::string error;
				if(!document_.override_property(selected_record_, override_id, error)) {
					report(error);
				} else {
					selected_field_ = no_selection;
					for(const auto &entry : document_.records()[selected_record_].effective) {
						if(entry.field.property_id == override_id && !entry.external &&
						   entry.record == selected_record_) {
							selected_field_ = entry.field_index;
						}
					}

					refresh_edit();
					page_	= 0;
					status_ = "Created a local copy of the inherited value. Edit it in Properties.";
				}
			}
		}

		void draw_raw(const Record &record) {
			constexpr size_t page_size = 512;
			if(ImGui::Checkbox("Inspect entire file (including header and unknown sections)", &whole_file_)) {
				page_ = 0;
			}

			const size_t base = whole_file_ ? 0 : record.offset;
			const size_t size = whole_file_ ? document_.bytes().size() : record.size;
			page_			  = std::min(page_, size ? (size - 1) / page_size : size_t(0));
			if(ImGui::Button("Previous") && page_ > 0) {
				--page_;
			}

			ImGui::SameLine();
			if(ImGui::Button("Next") && (page_ + 1) * page_size < size) {
				++page_;
			}

			ImGui::SameLine();
			ImGui::Text("Page %zu / %zu", page_ + 1, (size + page_size - 1) / page_size);
			const size_t start = base + page_ * page_size;
			const size_t end   = std::min(base + size, start + page_size);
			for(size_t offset = start; offset < end; offset += 16) {
				char line[160];
				int length = std::snprintf(line, sizeof(line), "%08zX  ", offset);
				for(size_t i = 0; i < 16 && offset + i < end; ++i) {
					length += std::snprintf(line + length, sizeof(line) - size_t(length), "%02X ",
											unsigned(document_.bytes()[offset + i]));
				}

				ImGui::TextUnformatted(line);
			}
		}

		void draw_changes() {
			size_t count	   = 0;
			const auto &before = document_.saved_bytes();
			const auto &after  = document_.bytes();
			ImGui::Text("Saved size: %zu bytes; current size: %zu bytes.", before.size(), after.size());
			for(size_t i = 0; i < std::max(before.size(), after.size()); ++i) {
				if(i < before.size() && i < after.size() && before[i] == after[i]) {
					continue;
				}

				if(count < 256) {
					if(i >= before.size()) {
						ImGui::Text("0x%08zX  -- -> %02X (added)", i, unsigned(after[i]));
					} else if(i >= after.size()) {
						ImGui::Text("0x%08zX  %02X -> -- (removed)", i, unsigned(before[i]));
					} else {
						ImGui::Text("0x%08zX  %02X -> %02X", i, unsigned(before[i]), unsigned(after[i]));
					}
				}

				++count;
			}

			ImGui::Text(
				"%zu differing byte positions since load/save (first 256 shown). Structural edits shift later data.",
				count);
		}

		void draw_archive_files() {
			if(!show_archive_files_) {
				return;
			}

			ImGui::OpenPopup("Scene template files");
			ImGui::SetNextWindowSize(ImVec2(800, 620), ImGuiCond_FirstUseEver);
			if(!ImGui::BeginPopupModal("Scene template files")) {
				return;
			}

			archive_files_rendered_ = true;
			const auto names		= document_.scene_archives();
			if(selected_archive_ >= names.size()) {
				selected_archive_ = no_selection;
			}

			ImGui::TextUnformatted("Global.not is implicit and always loaded first.");
			ImGui::Text("Declared archives: %zu / %u", names.size(), NSDParser::MAX_TEMPLATE_FILES);
			ImGui::TextWrapped("First-loaded templates win. Archive changes may break inheritance; check Diagnostics.");
			ImGui::BeginDisabled(archive_remove_pending_);
			if(ImGui::BeginTable("Scene archives", 2,
								 ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
								 ImVec2(0, 180))) {
				ImGui::TableSetupColumn("Order");
				ImGui::TableSetupColumn("Archive reference");
				ImGui::TableHeadersRow();
				for(size_t i = 0; i < names.size(); ++i) {
					ImGui::PushID(static_cast<int>(i));
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					const auto label = std::to_string(i + 1);
					if(ImGui::Selectable(label.c_str(), selected_archive_ == i, ImGuiSelectableFlags_SpanAllColumns)) {
						selected_archive_ = i;
					}

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(names[i].c_str());
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::TextWrapped(
				"Misc/Templates name: 1-31 printable ASCII bytes; .not optional. Workspace copies take priority.");
			ImGui::InputText("Archive name", archive_name_, sizeof(archive_name_));
			ImGui::BeginDisabled(!archive_name_[0] || names.size() >= NSDParser::MAX_TEMPLATE_FILES);
			const bool add = ImGui::Button("Add archive at end");
			ImGui::EndDisabled();
			const bool selected = selected_archive_ < names.size();
			ImGui::BeginDisabled(!selected);
			if(ImGui::Button("Remove selected archive...")) {
				archive_remove_pending_ = true;
				archive_remove_slot_	= selected_archive_;
			}

			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!selected || selected_archive_ == 0);
			const bool earlier = ImGui::Button("Move archive earlier");
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!selected || selected_archive_ + 1 >= names.size());
			const bool later = ImGui::Button("Move archive later");
			ImGui::EndDisabled();
			ImGui::EndDisabled();
			bool remove = false;
			if(archive_remove_pending_) {
				ImGui::Separator();
				ImGui::Text("Remove reference: %s", names[archive_remove_slot_].c_str());
				ImGui::TextWrapped(
					"Remove this scene reference? Inheritance may break. Disk files stay unchanged. Undo available.");
				remove = ImGui::Button("Confirm archive removal");
				ImGui::SameLine();
				if(ImGui::Button("Cancel archive removal")) {
					archive_remove_pending_ = false;
				}
			}

			ImGui::TextWrapped(
				"Reopen scene to reload archives. Close panel for Undo/Redo. Archive files are unchanged.");
			if(ImGui::CollapsingHeader("Archive/placement diagnostics")) {
				for(const auto &warning : document_.warnings()) {
					ImGui::BulletText("%s", warning.c_str());
				}
			}

			if(ImGui::Button("Close template files") || (smoke_ && !document_.records().empty())) {
				show_archive_files_		= false;
				archive_remove_pending_ = false;
				ImGui::CloseCurrentPopup();
			}

			if(add || earlier || later || remove) {
				std::string error;
				bool changed;
				if(add) {
					changed = document_.insert_scene_archive(no_selection, archive_name_, error);
				} else if(remove) {
					changed = document_.remove_scene_archive(archive_remove_slot_, error);
				} else {
					changed = document_.move_scene_archive(
						selected_archive_, earlier ? selected_archive_ - 1 : selected_archive_ + 2, error);
				}

				archive_remove_pending_ = false;
				if(!changed) {
					show_archive_files_ = false;
					ImGui::CloseCurrentPopup();
					report(error);
				} else {
					if(add) {
						selected_archive_ = names.size();
						archive_name_[0]  = '\0';
					} else if(remove) {
						selected_archive_ = no_selection;
					} else {
						selected_archive_ = earlier ? selected_archive_ - 1 : selected_archive_ + 1;
					}

					selected_field_ = no_selection;
					page_			= 0;
					rebuild_tree();
					status_ = "Updated scene template files. Effective values refreshed; review Diagnostics, then save "
							  "a workspace copy.";
				}
			}

			ImGui::EndPopup();
		}

		void draw_dialogs(GLFWwindow *window) {
			draw_archive_files();
			if(smoke_ && format_ == Format::nsd && archive_files_rendered_ && effective_rendered_ &&
			   !property_add_rendered_ && selected_record_ < document_.records().size()) {
				begin_property();
			}

			if(show_new_archive_) {
				ImGui::OpenPopup("New NOT archive");
			}

			if(ImGui::BeginPopupModal("New NOT archive", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextWrapped("Creates an empty root. Add local properties, including runtime type, before use.");
				ImGui::TextUnformatted("Root name: 1 through 31 printable ASCII bytes, not just spaces.");
				ImGui::InputText("Initial template name", template_name_, sizeof(template_name_));
				const bool create = ImGui::Button("Create archive");
				ImGui::SameLine();
				const bool cancel = ImGui::Button("Cancel archive creation");
				if(create || cancel) {
					show_new_archive_ = false;
					ImGui::CloseCurrentPopup();
					if(create) {
						std::string error;
						if(!document_.create_not_archive(template_name_, error)) {
							report(error);
						} else {
							loaded_				  = true;
							inheritance_rendered_ = false;
							selected_record_	  = 0;
							selected_field_		  = no_selection;
							page_				  = 0;
							edit_[0]			  = '\0';
							raw_enabled_ = whole_file_ = false;
							filter_.Clear();
							rebuild_tree();
							status_ = "Created unsaved NOT archive. Add local properties, then Save As into the "
									  "authoring workspace.";
						}
					}
				}

				ImGui::EndPopup();
			}

			if(show_add_property_) {
				ImGui::OpenPopup("Add local property");
			}

			if(ImGui::BeginPopupModal("Add local property", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				property_add_rendered_ = true;
				ImGui::Text("%s: %s", format_ == Format::nsd ? "Placement" : "Template",
							document_.records()[add_record_].name.c_str());
				ImGui::TextWrapped(
					"Append a local value; earlier values remain. Gameplay enums/flags are not validated.");
				if(format_ == Format::nsd) {
					ImGui::TextWrapped(
						"Counted placements only. References, primary transforms and sector are protected.");
				}

				const auto *selected = nodPropertyInfo(new_property_id_);
				if(ImGui::BeginCombo("Property", selected ? selected->name : "Select a property")) {
					property_filter_.Draw("Filter properties");
					for(uint32_t id = 1; const auto *info = nodPropertyInfo(id); ++id) {
						if(format_ == Format::nsd && id <= 3) {
							continue;
						}

						if(info->native_type != 0 && info->native_type != 2 && info->native_type != 3 &&
						   info->native_type != 4) {
							continue;
						}

						if(!property_filter_.PassFilter(info->name)) {
							continue;
						}

						const std::string label = std::string(info->name) + " (tag " + std::to_string(id) + ")";
						if(ImGui::Selectable(label.c_str(), id == new_property_id_)) {
							new_property_id_	   = id;
							new_property_value_[0] = '\0';
						}
					}

					ImGui::EndCombo();
				}

				selected = nodPropertyInfo(new_property_id_);
				if(selected) {
					ImGui::Text("Tag %u; native type %u; engine handler %u.", new_property_id_, selected->native_type,
								selected->handler);
					ImGui::TextUnformatted(
						selected->native_type == 0	 ? "Printable ASCII, at most 31 bytes (empty is allowed)."
						: selected->native_type == 2 ? "One finite float32 number."
						: selected->native_type == 3 ? "Unsigned decimal uint32 (raw bits, not a validated enum)."
													 : "Three finite float32 components separated by spaces.");
				}

				ImGui::SetNextItemWidth(500);
				ImGui::InputText("Initial value", new_property_value_, sizeof(new_property_value_));
				ImGui::BeginDisabled(!selected);
				const bool add = ImGui::Button("Append property");
				ImGui::EndDisabled();
				ImGui::SameLine();
				const bool cancel = ImGui::Button("Cancel property insertion");
				if(add || cancel || smoke_) {
					show_add_property_ = false;
					ImGui::CloseCurrentPopup();
					if(add) {
						std::string error;
						if(!document_.insert_property(add_record_, new_property_id_, new_property_value_, error)) {
							report(error);
						} else {
							selected_record_ = add_record_;
							selected_field_	 = modifier_fields(add_record_).back();
							page_			 = 0;
							refresh_edit();
							status_ = "Appended local property; effective values refreshed. Undo restores the complete "
									  "document.";
						}
					}
				}

				ImGui::EndPopup();
			}

			if(smoke_ && format_ == Format::nsd && property_add_rendered_ && document_.has_template_context() &&
			   !placement_template_rendered_ && selected_record_ < document_.records().size()) {
				begin_placement_template();
			}

			if(show_placement_template_) {
				ImGui::OpenPopup("Placement inheritance");
			}

			if(ImGui::BeginPopupModal("Placement inheritance", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				placement_template_rendered_ = true;
				ImGui::Text("Current template: %s", document_.records()[placement_record_].type.c_str());
				const bool removing = placement_action_ == PlacementAction::remove;
				if(placement_action_ == PlacementAction::primary) {
					ImGui::TextWrapped("Replace primary basedon only; other modifiers stay unchanged.");
				} else if(placement_action_ == PlacementAction::insert) {
					ImGui::Text("Insert a secondary basedon %s.",
								placement_slot_ == no_selection
									? "at the end"
									: ("before modifier #" + std::to_string(placement_slot_ + 1)).c_str());
				} else {
					ImGui::Text("%s secondary basedon #%zu only.", removing ? "Remove" : "Replace",
								placement_slot_ + 1);
				}

				ImGui::TextWrapped(
					"Requires the same resolved runtime class (tag 20). Inherited/local precedence may change.");
				if(!removing) {
					placement_filter_.Draw("Filter templates");
					if(ImGui::BeginListBox("Cached templates", ImVec2(650, 220))) {
						for(size_t i = 0; i < placement_choices_.size(); ++i) {
							const auto &choice		= placement_choices_[i];
							const std::string label = choice.name + " [" + choice.archive + "]";
							if(!placement_filter_.PassFilter(label.c_str())) {
								continue;
							}

							ImGui::PushID(static_cast<int>(i));
							const float label_x = ImGui::GetCursorPosX();
							if(ImGui::Selectable("##TemplateChoice", placement_choice_ == i)) {
								placement_choice_ = i;
							}

							ImGui::SameLine(label_x);
							ImGui::TextUnformatted(label.c_str());
							ImGui::PopID();
						}

						ImGui::EndListBox();
					}
				}

				ImGui::TextWrapped(
					"Global loads first. Apply checks class and property limits, not assets or gameplay.");
				ImGui::Checkbox("I understand precedence may change; retain serialized local modifiers",
								&placement_confirmed_);
				ImGui::BeginDisabled((!removing && placement_choice_ >= placement_choices_.size()) ||
									 !placement_confirmed_);
				const bool apply = ImGui::Button("Apply inheritance change");
				ImGui::EndDisabled();
				ImGui::SameLine();
				const bool cancel = ImGui::Button("Cancel template change");
				if(apply || cancel) {
					show_placement_template_ = false;
					ImGui::CloseCurrentPopup();
					if(apply) {
						std::string error;
						bool changed = false;
						switch(placement_action_) {
							case PlacementAction::primary:
								changed = document_.reassign_placement_template(
									placement_record_, placement_choices_[placement_choice_].name, error);
								break;
							case PlacementAction::insert:
								changed = document_.insert_placement_reference(
									placement_record_, placement_slot_, placement_choices_[placement_choice_].name,
									error);
								break;
							case PlacementAction::replace:
								changed = document_.replace_placement_reference(
									placement_record_, placement_slot_, placement_choices_[placement_choice_].name,
									error);
								break;
							case PlacementAction::remove:
								changed =
									document_.remove_placement_reference(placement_record_, placement_slot_, error);
								break;
						}

						if(!changed) {
							report(error);
						} else {
							selected_record_ = placement_record_;
							selected_field_	 = no_selection;
							selected_placement_modifier_ =
								placement_action_ == PlacementAction::replace ? placement_slot_ : no_selection;
							page_ = 0;
							rebuild_tree();
							refresh_edit();
							status_ = "Updated placement inheritance; runtime class, prefix and opaque body preserved.";
						}
					}
				}

				ImGui::EndPopup();
			}

			if(template_action_ != TemplateAction::none) {
				ImGui::OpenPopup("Template operation");
			}

			if(ImGui::BeginPopupModal("Template operation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				const bool removing			   = template_action_ == TemplateAction::remove;
				const bool renaming			   = template_action_ == TemplateAction::rename;
				const bool reparenting		   = template_action_ == TemplateAction::reparent;
				const bool inserting_reference = template_action_ == TemplateAction::insert_reference;
				const bool removing_reference  = template_action_ == TemplateAction::remove_reference;
				ImGui::Text("Selected template: %s", document_.records()[template_record_].name.c_str());
				if(reparenting || inserting_reference) {
					const auto &records = document_.records();
					const auto &fields	= records[template_record_].fields;
					if(inserting_reference) {
						ImGui::TextWrapped(
							"Insert basedon at the chosen position. Later values win; duplicates remain.");
						const auto modifiers = modifier_fields(template_record_);
						const std::string selected =
							template_before_ == no_selection
								? "End of modifier list"
								: "Before #" + std::to_string(template_before_) + ": " + fields[template_before_].name;
						if(ImGui::BeginCombo("Insert position", selected.c_str())) {
							if(ImGui::Selectable("End of modifier list", template_before_ == no_selection)) {
								template_before_ = no_selection;
							}

							for(const auto field : modifiers) {
								const std::string label =
									"Before #" + std::to_string(field) + ": " + fields[field].name;
								if(ImGui::Selectable(label.c_str(), template_before_ == field)) {
									template_before_ = field;
								}
							}

							ImGui::EndCombo();
						}
					} else if(template_reference_ == no_selection) {
						ImGui::TextWrapped("Insert first basedon before local modifiers; local values keep priority.");
					} else {
						ImGui::TextWrapped("Replace selected basedon only; modifier order is unchanged.");
						const std::string selected = "#" + std::to_string(template_reference_) + ": " +
													 document_.value(fields[template_reference_]);
						if(ImGui::BeginCombo("Reference to change", selected.c_str())) {
							for(size_t i = 0; i < fields.size(); ++i) {
								if(fields[i].property_id != 0) {
									continue;
								}

								const std::string label = "#" + std::to_string(i) + ": " + document_.value(fields[i]);
								if(ImGui::Selectable(label.c_str(), template_reference_ == i)) {
									template_reference_ = i;
									template_parent_	= no_selection;
								}
							}

							ImGui::EndCombo();
						}
					}

					if(ImGui::BeginCombo("New parent", template_parent_ < records.size()
														   ? records[template_parent_].name.c_str()
														   : "Select a template")) {
						for(size_t i = 0; i < records.size(); ++i) {
							if(i == template_record_) {
								continue;
							}

							if(ImGui::Selectable(records[i].name.c_str(), template_parent_ == i)) {
								template_parent_ = i;
							}
						}

						ImGui::EndCombo();
					}

					ImGui::TextWrapped(
						"Checks all local bases and descendants. External dependencies unchecked; see Diagnostics.");
				} else if(removing_reference) {
					const auto &field = document_.records()[template_record_].fields[template_reference_];
					ImGui::TextUnformatted(document_.value(field).c_str());
					ImGui::TextWrapped("Remove this basedon? This template and descendants may inherit different "
									   "values. Undo available.");
				} else if(removing) {
					ImGui::TextWrapped(
						"Delete this template? Referenced and final templates are protected. Undo available.");
				} else {
					ImGui::TextWrapped("%s",
									   renaming ? "Rename template and matching basedon references in this archive."
									   : template_action_ == TemplateAction::derive
										   ? "Create a child inheriting the selected template."
										   : "Copy template, including local values, metadata and basedon references.");
					ImGui::TextUnformatted("Unique name: 1-31 printable ASCII bytes.");
					ImGui::InputText("New template name", template_name_, sizeof(template_name_));
				}

				if(removing || renaming) {
					ImGui::TextWrapped("External references are not checked or updated.");
					ImGui::Checkbox("I checked external references", &external_references_acknowledged_);
				}

				ImGui::BeginDisabled(((removing || renaming) && !external_references_acknowledged_) ||
									 ((reparenting || inserting_reference) && template_parent_ == no_selection));
				const bool apply = ImGui::Button(removing			   ? "Delete template"
												 : renaming			   ? "Rename template"
												 : reparenting		   ? "Change inheritance"
												 : inserting_reference ? "Insert reference"
												 : removing_reference  ? "Remove reference"
																	   : "Create template");
				ImGui::EndDisabled();
				ImGui::SameLine();
				const bool cancel = ImGui::Button("Cancel template operation");
				if(apply || cancel) {
					const auto action = template_action_;
					template_action_  = TemplateAction::none;
					ImGui::CloseCurrentPopup();
					if(apply) {
						std::string error;
						bool changed = false;
						switch(action) {
							case TemplateAction::remove:
								changed = document_.remove_template(template_record_, error);
								break;
							case TemplateAction::derive:
								changed = document_.create_derived_template(template_record_, template_name_, error);
								break;
							case TemplateAction::duplicate:
								changed = document_.duplicate_template(template_record_, template_name_, error);
								break;
							case TemplateAction::rename:
								changed = document_.rename_template(template_record_, template_name_, error);
								break;
							case TemplateAction::reparent:
								changed = document_.reparent_template(template_record_, template_reference_,
																	  template_parent_, error);
								break;
							case TemplateAction::insert_reference:
								changed = document_.insert_template_reference(template_record_, template_before_,
																			  template_parent_, error);
								break;
							case TemplateAction::remove_reference:
								changed =
									document_.remove_template_reference(template_record_, template_reference_, error);
								break;
							case TemplateAction::none:
								error = "No template operation was selected.";
								break;
						}

						if(!changed) {
							report(error);
						} else {
							selected_field_ = no_selection;
							page_			= 0;
							filter_.Clear();
							if(action == TemplateAction::derive || action == TemplateAction::duplicate) {
								selected_record_ = document_.records().size() - 1;
							}

							rebuild_tree();
							if(action == TemplateAction::remove) {
								status_ = "Deleted template; local references checked. External references were not "
										  "rewritten.";
							} else if(action == TemplateAction::rename) {
								status_ =
									"Renamed template and local references. External references were not rewritten.";
							} else if(action == TemplateAction::reparent) {
								status_ = "Changed inheritance; local cycles checked. Review effective properties and "
										  "Diagnostics.";
							} else if(action == TemplateAction::insert_reference ||
									  action == TemplateAction::remove_reference) {
								status_ =
									"Updated basedon list. Review source order, effective properties and Diagnostics.";
							} else {
								status_ = "Created template. Native workspace save and Undo/Redo are available.";
							}
						}
					}
				}

				ImGui::EndPopup();
			}

			if(show_remove_property_) {
				ImGui::OpenPopup("Remove local property?");
			}

			if(ImGui::BeginPopupModal("Remove local property?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextWrapped(
					"Remove this local modifier? An earlier or inherited value may take effect. Undo available.");
				if(ImGui::Button("Remove property")) {
					show_remove_property_ = false;
					ImGui::CloseCurrentPopup();
					std::string error;
					if(!document_.remove_property(remove_record_, remove_field_, error)) {
						report(error);
					} else {
						selected_field_ = no_selection;
						page_			= 0;
						status_			= "Removed local property; effective inheritance refreshed.";
					}
				}

				ImGui::SameLine();
				if(ImGui::Button("Cancel removal")) {
					show_remove_property_ = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if(pending_ != Action::none && error_.empty()) {
				ImGui::OpenPopup("Unsaved changes");
			}

			if(ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextUnformatted("Save your changes before creating/opening another file or closing.");
				if(ImGui::Button("Discard changes")) {
					perform_action();
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();
				if(ImGui::Button("Cancel")) {
					pending_ = Action::none;
					ImGui::CloseCurrentPopup();
				}

				ImGui::TextUnformatted("Cancel, then use Save or Save As to retain the changes.");
				ImGui::EndPopup();
			}
#ifdef _WIN32
			if(show_open_) {
				show_open_ = false;
				std::vector<wchar_t> filename(32768, L'\0');
				const auto directory = document_.path().parent_path().wstring();
				OPENFILENAMEW dialog{};
				dialog.lStructSize	   = sizeof(dialog);
				dialog.hwndOwner	   = glfwGetWin32Window(window);
				dialog.lpstrFilter	   = format_ == Format::nsd
											 ? L"NSD scene files (*.nsd)\0*.nsd\0All files (*.*)\0*.*\0"
											 : L"NOT template files (*.not)\0*.not\0All files (*.*)\0*.*\0";
				dialog.lpstrFile	   = filename.data();
				dialog.nMaxFile		   = static_cast<DWORD>(filename.size());
				dialog.lpstrInitialDir = directory.empty() ? nullptr : directory.c_str();
				dialog.lpstrTitle	   = format_ == Format::nsd ? L"Open NSD scene" : L"Open NOT templates";
				dialog.Flags		   = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
				if(GetOpenFileNameW(&dialog)) {
					load(std::filesystem::path(filename.data()).u8string());
				} else {
					const DWORD error = CommDlgExtendedError();
					if(error) {
						report("System file dialog failed (code " + std::to_string(error) + ")");
					}
				}
			}
#else
			(void)window;
			if(show_open_) {
				ImGui::OpenPopup("Open file");
			}

			if(ImGui::BeginPopupModal("Open file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::SetNextItemWidth(700);
				ImGui::InputText("Path", open_path_, sizeof(open_path_));
				if(ImGui::Button("Open")) {
					show_open_ = false;
					ImGui::CloseCurrentPopup();
					load(open_path_);
				}

				ImGui::SameLine();
				if(ImGui::Button("Cancel")) {
					show_open_ = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
#endif
			if(show_save_) {
				ImGui::OpenPopup("Save As");
			}

			if(ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::SetNextItemWidth(700);
				ImGui::InputText("Destination", save_path_, sizeof(save_path_));
				if(ImGui::Button("Save file")) {
					std::error_code ec;
					const bool exists = std::filesystem::exists(std::filesystem::u8path(save_path_), ec);
					if(ec) {
						show_save_ = false;
						ImGui::CloseCurrentPopup();
						report("Cannot inspect destination: " + ec.message());
					} else if(exists) {
						show_save_		   = false;
						confirm_overwrite_ = true;
						ImGui::CloseCurrentPopup();
					} else {
						show_save_ = false;
						ImGui::CloseCurrentPopup();
						save_as(false);
					}
				}

				ImGui::SameLine();
				if(ImGui::Button("Cancel")) {
					show_save_ = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if(confirm_overwrite_) {
				ImGui::OpenPopup("Confirm overwrite");
			}

			if(ImGui::BeginPopupModal("Confirm overwrite", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextUnformatted("The destination already exists. Replace it?");
				ImGui::TextUnformatted(save_path_);
				if(ImGui::Button("Replace")) {
					confirm_overwrite_ = false;
					ImGui::CloseCurrentPopup();
					save_as(true);
				}

				ImGui::SameLine();
				if(ImGui::Button("Cancel")) {
					confirm_overwrite_ = false;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if(!error_.empty() && !ImGui::IsPopupOpen("Error")) {
				ImGui::OpenPopup("Error");
			}

			if(ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				if(error_.empty()) {
					ImGui::CloseCurrentPopup();
				} else {
					ImGui::TextWrapped("%s", error_.c_str());
					const bool dismiss = ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
										 ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
										 ImGui::IsKeyPressed(ImGuiKey_Escape);
					ImGui::SetItemDefaultFocus();
					if(dismiss) {
						error_.clear();
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}
		}

		Format format_;
		Document document_;
		bool smoke_							= false;
		bool inheritance_rendered_			= false;
		bool property_add_rendered_			= false;
		bool show_archive_files_			= false;
		bool archive_files_rendered_		= false;
		bool archive_remove_pending_		= false;
		bool effective_rendered_			= false;
		bool show_placement_template_		= false;
		bool placement_template_rendered_	= false;
		bool placement_confirmed_			= false;
		bool placement_order_rendered_		= false;
		PlacementAction placement_action_	= PlacementAction::primary;
		size_t placement_slot_				= no_selection;
		size_t selected_placement_modifier_ = no_selection;
		size_t placement_record_			= no_selection;
		size_t placement_choice_			= no_selection;
		std::vector<Document::PlacementTemplate> placement_choices_;
		ImGuiTextFilter placement_filter_;
		size_t selected_archive_	  = no_selection;
		size_t archive_remove_slot_	  = no_selection;
		char archive_name_[128]		  = {};
		bool loaded_				  = false;
		bool quit_					  = false;
		bool raw_enabled_			  = false;
		bool whole_file_			  = false;
		bool show_open_				  = false;
		bool show_save_				  = false;
		bool confirm_overwrite_		  = false;
		bool workspace_failed_		  = false;
		bool show_remove_property_	  = false;
		bool show_new_archive_		  = false;
		bool show_add_property_		  = false;
		size_t add_record_			  = no_selection;
		uint32_t new_property_id_	  = UINT32_MAX;
		char new_property_value_[512] = {};
		ImGuiTextFilter property_filter_;
		size_t remove_record_				   = no_selection;
		size_t remove_field_				   = no_selection;
		TemplateAction template_action_		   = TemplateAction::none;
		size_t template_record_				   = no_selection;
		size_t template_parent_				   = no_selection;
		size_t template_reference_			   = no_selection;
		size_t template_before_				   = no_selection;
		bool external_references_acknowledged_ = false;
		char template_name_[128]			   = {};
		Action pending_						   = Action::none;
		size_t selected_record_				   = no_selection;
		size_t selected_field_				   = no_selection;
		size_t page_						   = 0;
		ImGuiTextFilter filter_;
		std::vector<std::vector<size_t>> children_;
		char open_path_[4096] = {};
		char save_path_[4096] = {};
		char edit_[512]		  = {};
		std::string error_;
		std::string status_ = "Ready";
};

void glfw_error(int error, const char *description) {
	std::fprintf(stderr, "GLFW %d: %s\n", error, description);
}

} // namespace

int run_editor(int argc, char **argv, Format format) {
	std::string argument_error;
	NodTools::CommandLine command_line;
	if(!command_line.initialize(argc, argv, argument_error)) {
		std::fprintf(stderr, "%s\n", argument_error.c_str());
		return 1;
	}

	bool smoke = false;
	bool check = false;
	std::string path;
	std::filesystem::path workspace;
	std::filesystem::path assets;
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--help") {
			std::printf("Usage: %s [options] [file]\n"
						"  --check       Validate without GUI or saves.\n"
						"  --smoke-test  Render three frames; no saves.\n"
						"  --workspace DIR   Restrict saves to DIR.\n"
						"  --asset-root DIR  Read-only asset directory.\n",
						std::filesystem::u8path(argv[0]).filename().u8string().c_str());
			return 0;
		}

		if(arg == "--smoke-test") {
			smoke = true;
		} else if(arg == "--check") {
			check = true;
		} else if(arg == "--workspace" || arg == "--asset-root") {
			if(++i == argc) {
				std::fprintf(stderr, "%s requires a directory\n", arg.c_str());
				return 1;
			}

			(arg == "--workspace" ? workspace : assets) = std::filesystem::u8path(argv[i]);
		} else if(!arg.empty() && arg[0] == '-') {
			std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
			return 1;
		} else if(path.empty()) {
			path = arg;
		} else {
			std::fprintf(stderr, "Only one input file is supported\n");
			return 1;
		}
	}

	if(!assets.empty() && workspace.empty()) {
		std::fprintf(stderr, "--asset-root requires --workspace\n");
		return 1;
	}

	if(!workspace.empty()) {
		Document policy;
		std::string error;
		if(!policy.configure_workspace(workspace, assets, error)) {
			std::fprintf(stderr, "%s\n", error.c_str());
			return 1;
		}
	}

	if(check || smoke) {
		if(path.empty()) {
			std::fprintf(stderr, "An input file is required\n");
			return 1;
		}

		Document document;
		std::string error;
		if(!workspace.empty() && !document.configure_workspace(workspace, assets, error)) {
			std::fprintf(stderr, "%s\n", error.c_str());
			return 1;
		}

		if(!document.load(std::filesystem::u8path(path), format, error)) {
			std::fprintf(stderr, "%s\n", error.c_str());
			return 1;
		}

		std::printf("%zu records; %zu bytes; %zu diagnostics\n", document.records().size(), document.bytes().size(),
					document.warnings().size());
		for(const auto &warning : document.warnings()) {
			std::printf("  %s\n", warning.c_str());
		}

		if(check) {
			return 0;
		}
	}

	glfwSetErrorCallback(glfw_error);
	if(!glfwInit()) {
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	GLFWwindow *window = glfwCreateWindow(1400, 900, format == Format::nsd ? "NodSDK NSD Editor" : "NodSDK NOT Editor",
										  nullptr, nullptr);
	if(!window) {
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glewExperimental		= GL_TRUE;
	const GLenum glew_error = glewInit();
	if(glew_error != GLEW_OK) {
		std::fprintf(stderr, "GLEW initialization: %s\n", glewGetErrorString(glew_error));
#ifndef _WIN32
		if(glew_error != GLEW_ERROR_NO_GLX_DISPLAY) {
#endif
			glfwDestroyWindow(window);
			glfwTerminate();
			return 1;
#ifndef _WIN32
		}
#endif
	}

	glGetError();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	if(!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
		std::fprintf(stderr, "Cannot initialize ImGui GLFW backend\n");
		ImGui::DestroyContext();
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	if(!ImGui_ImplOpenGL3_Init("#version 330 core")) {
		std::fprintf(stderr, "Cannot initialize ImGui OpenGL backend\n");
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	int result = 0;
	{
		Editor editor(format, workspace, assets, smoke);
		if(!path.empty() && !editor.load(path) && smoke) {
			result = 1;
		}

		glfwSetWindowUserPointer(window, &editor);
		glfwSetDropCallback(window, [](GLFWwindow *target, int count, const char **paths) {
			if(count == 1) {
				static_cast<Editor *>(glfwGetWindowUserPointer(target))->drop(paths[0]);
			} else {
				std::fprintf(stderr, "Drop one NSD/NOT file at a time\n");
			}
		});
		int frames = 0;
		while(!editor.quit() && result == 0) {
			glfwPollEvents();
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			editor.draw(window);
			ImGui::Render();
			int width;
			int height;
			glfwGetFramebufferSize(window, &width, &height);
			glViewport(0, 0, width, height);
			glClearColor(0.12f, 0.12f, 0.14f, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
			if(smoke && ++frames == 3) {
				result = editor.failed() || !editor.smoke_view_ready() || glGetError() != GL_NO_ERROR ? 1 : 0;
				std::printf("Rendered %d editor frames; status %d\n", frames, result);
				break;
			}
		}

		glfwSetWindowUserPointer(window, nullptr);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return result;
}

} // namespace nod::edit
