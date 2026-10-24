/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: template_catalog.cpp
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

#include "document.h"
#include "metadata_xml.h"
#include "template_edit.h"
#include "tool_help.h"
#include <map>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <map>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

/*
 */
namespace {
	
/*
 */
using nod::metadata::xml;
using nod::metadata::map_name;

/*
 */
void usage(FILE *output) {
	nod_tools_print_help(output, "NODTemplates",
		"--workspace DIR : Authoring workspace; shared writes are restricted to it.\n"
		"--asset-root DIR : Optional read-only original asset directory.\n"
		"--archive NAME : Add an ordered NOT archive; repeatable, Global is implicit.\n"
		"--details 1 : Include resolved property details in the XML catalog.\n"
		"--template NAME : Select a template for inspection or a shared write.\n"
		"--modifiers HEX : Resolve a placement's native modifier stream.\n"
		"--scene FILE.nsd : Read the instance catalog from an NSD scene.\n"
		"--sources \"CRC CRC CRC\" : Supply source signatures for instance catalog mode.\n"
		"--schema N : Set the instance catalog schema (1-15).\n"
		"--target-archive NAME : Select the workspace archive for a shared operation.\n"
		"--write-property ID : Change an existing shared property.\n"
		"--override-property ID : Add a local override of an inherited property.\n"
		"--preview-remove ID : Preview override removal without writing.\n"
		"--remove-override ID : Remove a local override with provenance validation.\n"
		"--restore-override ID : Restore an override with complete native proof.\n"
		"--expect OLD : Require the expected current value before a shared operation.\n"
		"--value NEW : Set the replacement or restored property value.\n"
		"--expect-owner NAME : Require the expected owning template.\n"
		"--expect-archive NAME : Require the expected owning archive.\n"
		"--slot N : Specify the serialized modifier slot for removal/restoration proof.\n"
		"--modifier HEX : Supply the complete native modifier payload as proof.\n"
		"--order HEX : Supply the expected native modifier order as proof.\n"
		"--help : Show help.\n",
		"NODTemplates --workspace workspace --asset-root assets --archive level.not --details 1\n"
		"NODTemplates --workspace workspace --asset-root assets --template Crate --details 1\n");
}

/*
 */
bool property_xml(const NOTDatabase &database, std::map<std::string, nod::edit::Document> &documents,
				  const std::vector<NOTParser::ResolvedProperty> &properties,
				  std::ostream &output, std::string &error) {
	for(const auto &resolved : properties) {
		const auto *archive = database.source(resolved.owner);
		if(!archive) { error = "Missing property archive provenance"; return false; }
		auto inserted = documents.try_emplace(archive->name);
		auto &document = inserted.first->second;
		if(inserted.second && !document.load_bytes(archive->bytes, nod::edit::Format::not_file, error)) return false;
		bool found = false;
		for(size_t r = 0; r < document.records().size() && !found; ++r) {
			const auto &record = document.records()[r];
			for(size_t f = 0; f < record.fields.size(); ++f) {
				const auto &field = record.fields[f];
				if(!field.native_modifier || field.offset != resolved.property->value_offset) continue;
				const auto *info = nodPropertyInfo(resolved.property->prop_id);
				const char *storage = nod::metadata::storage(field.kind);
				const auto value = document.value(field);
				std::string issue;
				const bool editable = nod::metadata::editable(document, field, value, issue);
				output << "<property id=\"" << resolved.property->prop_id << "\" name=\"" << xml(info ? info->name : "Unknown")
					<< "\" storage=\"" << storage << "\" value=\"" << xml(value) << "\" archive=\"" << xml(archive->name)
					<< "\" template=\"" << xml(resolved.owner->name) << "\" editable=\"" << (editable ? "1" : "0")
					<< "\" problem=\"" << xml(issue) << "\"/>\n";
				found = true;
				break;
			}
		}
		if(!found) { error = "Cannot bind a resolved property to its owning archive field"; return false; }
	}
	return true;
}
}

/*
 */
int main(int argc, char **argv) {
#ifdef _WIN32
	int count;
	auto wide = CommandLineToArgvW(GetCommandLineW(), &count);
	if(!wide) { std::cerr << "Cannot read Unicode arguments\n"; return 1; }
	std::vector<std::string> strings;
	for(int i = 0; i < count; ++i) strings.push_back(std::filesystem::path(wide[i]).u8string());
	LocalFree(wide);
	std::vector<char *> pointers;
	for(auto &value : strings) pointers.push_back(value.data());
	pointers.push_back(nullptr);
	argc = count; argv = pointers.data();
#endif
	if(argc < 2) {
		usage(stderr);
		return 1;
	}
	for(int i = 1; i < argc; i += 2)
		if(std::string(argv[i]) == "--scene" || std::string(argv[i]) == "--sources" || std::string(argv[i]) == "--schema")
			return nod::metadata::instance_catalog(argc, argv);
	std::filesystem::path workspace, assets;
	std::vector<std::string> archives;
	std::string error, selected, placement;
	std::map<std::string, std::string> write;
	std::map<std::string, bool> seen;
	bool details = false;
	for(int i = 1; i < argc; ++i) {
		const std::string option = argv[i];
		if(option == "--help") {
			usage(stdout);
			return 0;
		}
		if(option != "--archive" && !seen.emplace(option, true).second) { std::cerr << "Duplicate option: " << option << '\n'; return 1; }
		if(option == "--target-archive" || option == "--write-property" || option == "--expect" || option == "--value" ||
		   option == "--override-property" || option == "--remove-override" || option == "--expect-owner" || option == "--expect-archive" ||
		   option == "--preview-remove" || option == "--restore-override" || option == "--slot" || option == "--modifier" || option == "--order") {
			if(++i == argc || !write.emplace(option, argv[i]).second) { std::cerr << "Incomplete or duplicate shared write option\n"; return 1; }
			continue;
		}
		if((option != "--workspace" && option != "--asset-root" && option != "--archive" && option != "--template" &&
			option != "--details" && option != "--modifiers") || ++i == argc) {
			std::cerr << "Invalid or incomplete option: " << option << '\n'; return 1;
		}
		if(option == "--details") {
			if(details || std::string(argv[i]) != "1") { std::cerr << "--details requires one value of 1\n"; return 1; }
			details = true;
		} else if(option == "--modifiers") {
			if(!placement.empty() || !*argv[i]) { std::cerr << "Empty or duplicate placement modifier stream\n"; return 1; }
			placement = argv[i];
		} else if(option == "--template") {
			if(!selected.empty() || !map_name(argv[i])) { std::cerr << "Specify one MAP-safe template name\n"; return 1; }
			selected = argv[i];
		} else if(option == "--archive") {
			std::string name;
			if(!NOTDatabase::archiveName(argv[i], name, error)) { std::cerr << error << '\n'; return 1; }
			if(!map_name(name) || name.find(';') != std::string::npos || name == "global.not" ||
			   std::find(archives.begin(), archives.end(), name) != archives.end() || archives.size() == 9) {
				std::cerr << "Archive references must be MAP-safe names of at most 31 bytes; Global is implicit, duplicates and more than nine slots are rejected\n"; return 1;
			}
			archives.push_back(name);
		} else (option == "--workspace" ? workspace : assets) = std::filesystem::u8path(argv[i]);
	}
	nod::edit::Document policy;
	if(workspace.empty() || !policy.configure_workspace(workspace, assets, error)) {
		std::cerr << (error.empty() ? "--workspace is required" : error) << '\n'; return 1;
	}
	NOTDatabase database;
	if(!database.load(workspace, archives, error, assets)) { std::cerr << error << '\n'; return 1; }
	const auto *selection = selected.empty() ? nullptr : database.findTemplate(selected.c_str());
	if(!selected.empty() && !selection) { std::cerr << "Template not found in the active archive context: " << selected << '\n'; return 1; }
	std::unique_ptr<nod::edit::Document> pending;
	nod::metadata::SharedRemoval removal;
	bool preview = false;
	if(!write.empty()) {
		const bool adding = write.count("--override-property") != 0, removing = write.count("--remove-override") != 0;
		const bool restoring = write.count("--restore-override") != 0;
		preview = write.count("--preview-remove") != 0;
		const bool structural = adding || removing || restoring;
		const size_t proof = write.count("--slot") + write.count("--modifier") + write.count("--order");
		if(write.count("--write-property") + adding + removing + restoring + preview != 1 ||
		   (proof && (proof != 3 || (!removing && !restoring))) || (restoring && proof != 3) ||
		   write.size() != (preview ? 3 : structural ? 6 + proof : 4) ||
		   !write.count("--target-archive") || !write.count("--expect") || (!preview && !write.count("--value")) ||
		   (structural && (!write.count("--expect-owner") || !write.count("--expect-archive"))) ||
		   !selection || seen.count("--modifiers") || seen.count("--details")) {
			std::cerr << "Specify one shared operation with its complete values/provenance; restoration requires complete native proof; preview is read-only; no placement modifiers\n"; return 1;
		}
		const auto operation = adding ? nod::metadata::SharedOperation::add_override :
			removing ? nod::metadata::SharedOperation::remove_override :
			restoring ? nod::metadata::SharedOperation::restore_override :
			preview ? nod::metadata::SharedOperation::preview_remove : nod::metadata::SharedOperation::set;
		if(proof) { removal.slot = write["--slot"]; removal.payload = write["--modifier"]; removal.order = write["--order"]; }
		if(!nod::metadata::prepare_shared_edit(database, workspace, assets, selected, write["--target-archive"],
			write[adding ? "--override-property" : removing ? "--remove-override" :
				restoring ? "--restore-override" : preview ? "--preview-remove" : "--write-property"],
			write["--expect"], preview ? "" : write["--value"], operation,
			structural ? write["--expect-owner"] : "", structural ? write["--expect-archive"] : "", removal, pending, error)) {
			std::cerr << error << '\n'; return 1;
		}
		selection = database.findTemplate(selected.c_str());
	}
	std::vector<NODModifier> modifiers;
	nod::edit::Document placement_document;
	if(!placement.empty()) {
		if(!selection || placement.size() % 72 || placement.size() > 128 * 72) {
			std::cerr << "Placement queries require a template and 1 through 128 complete hex modifiers\n"; return 1;
		}
		std::vector<uint8_t> bytes(52, 0);
		memcpy(bytes.data(), "NOT\x10", 4);
		bytes[4] = 1; bytes[8] = 1;
		memcpy(bytes.data() + 12, selection->name, strlen(selection->name));
		bytes[44] = 48; bytes[48] = static_cast<uint8_t>(placement.size() / 72);
		for(size_t i = 0; i < placement.size(); i += 2) {
			const auto digit = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' :
				c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
			const int high = digit(placement[i]), low = digit(placement[i + 1]);
			if(high < 0 || low < 0) { std::cerr << "Invalid lowercase placement modifier hex\n"; return 1; }
			bytes.push_back(static_cast<uint8_t>(high * 16 + low));
		}
		bytes.resize(bytes.size() + 4, 0);
		NOTParser parser;
		if(!parser.parseBuffer(bytes.data(), bytes.size()) ||
		   !placement_document.load_bytes(bytes, nod::edit::Format::not_file, error)) {
			std::cerr << "Invalid placement modifiers: " << error << '\n'; return 1;
		}
		const auto &record = parser.getTemplateDB().templates[0];
		modifiers.assign(record.properties, record.properties + record.property_count);
		std::string primary;
		if(modifiers[0].prop_id != 0 || !nodModifierString(modifiers[0], primary) ||
		   database.findTemplate(primary.c_str()) != selection) {
			std::cerr << "Placement modifier stream does not begin with the selected template identity\n"; return 1;
		}
	}
	std::map<std::string, nod::edit::Document> documents;
	std::ostringstream output;
	output << "<nod-template-catalog version=\"1\">\n";
	for(const auto &snapshot : database.snapshots()) output << "<archive name=\"" << xml(snapshot.name) << "\"/>\n";
	for(const auto &warning : database.warnings()) output << "<warning text=\"" << xml(warning) << "\"/>\n";
	for(const auto &entry : database.templates()) {
		if(selection && entry.second != selection) continue;
		const auto *source = database.source(entry.second);
		if(!source) { std::cerr << "Missing archive provenance\n"; return 1; }
		const std::string name = entry.second->name;
		std::vector<NOTParser::ResolvedProperty> resolved;
		std::string problem, model;
		uint32_t type = 0;
		bool has_type = false;
		if(!map_name(name)) problem = "Name is not a MAP-safe printable ASCII name of 1 through 31 bytes";
		else if((modifiers.empty() ? database.resolveModifiers(name.c_str(), resolved, problem) :
			database.resolveModifiers(modifiers.data(), modifiers.size(), resolved, problem)) == NOTParser::ResolveStatus::ok) {
			for(const auto &value : resolved) {
				if(value.property->prop_id == 20) {
					has_type = true;
					type = FileUtils::readUInt32(value.property->value, 0);
				}
				if(value.property->prop_id == 4 && !nodModifierString(*value.property, model)) model = "[invalid model reference]";
			}
			if(!has_type) problem = "No resolved native type (property 20); not a placeable template";
		}
		output << "<template name=\"" << xml(map_name(name) ? name : "[unsupported template name]")
			<< "\" archive=\"" << xml(source->name) << "\" type=\"" << type
			<< "\" properties=\"" << resolved.size() << "\" placeable=\"" << (problem.empty() ? "1" : "0")
			<< "\" model=\"" << xml(model) << "\" problem=\"" << xml(problem) << "\"";
		if(details) output << " parent=\"" << xml(entry.second->parent) << "\"";
		if(preview) output << " removal-slot=\"" << removal.slot << "\" removal-payload=\"" << removal.payload
			<< "\" removal-order=\"" << removal.order << "\"";
		if(!selection && !details) output << "/>\n";
		else {
			output << " details=\"1\">\n";
			for(const auto &value : resolved) {
				if(value.owner) {
					if(!property_xml(database, documents, {value}, output, error)) { std::cerr << error << '\n'; return 1; }
				} else {
					const auto field = placement_document.modifier_field(*value.property);
					const auto text = placement_document.value(field);
					const auto *info = nodPropertyInfo(value.property->prop_id);
					std::string issue;
					const bool enabled = nod::metadata::editable(placement_document, field, text, issue);
					output << "<property id=\"" << value.property->prop_id << "\" name=\"" << xml(info ? info->name : "Unknown")
						<< "\" storage=\"" << nod::metadata::storage(field.kind) << "\" value=\"" << xml(text)
						<< "\" archive=\"\" template=\"" << xml(name) << "\" source=\"placement\" editable=\""
						<< (enabled ? "1" : "0") << "\" problem=\"" << xml(issue) << "\"/>\n";
				}
			}
			output << "</template>\n";
		}
	}
	output << "</nod-template-catalog>\n";
	if(output.str().size() > 16 * 1024 * 1024) { std::cerr << "Template metadata exceeds 16 MiB\n"; return 1; }
	if(pending) {
		const auto *written_source = database.source(selection);
		if(!written_source) { std::cerr << "Missing edited template archive provenance; nothing was saved\n"; return 1; }
		for(const auto &snapshot : database.snapshots()) {
			NOTDatabase::Snapshot latest;
			if(!NOTDatabase::readArchive(workspace, snapshot.name, latest, error, assets)) {
				std::cerr << error << '\n'; return 1;
			}
			const auto &expected_bytes = snapshot.name == written_source->name ? pending->saved_bytes() : snapshot.bytes;
			if(latest.bytes != expected_bytes) {
				std::cerr << "Shared archive context changed while validating the edit; reload metadata\n"; return 1;
			}
		}
		if(!pending->save(pending->suggested_save_path(), false, error)) { std::cerr << error << '\n'; return 1; }
	}
	std::cout << output.str() << std::flush;
	if(!std::cout.good()) {
		std::cerr << (pending ? "Shared archive was saved, but cannot write result catalog; reload metadata\n" : "Cannot write template catalog\n");
		return 1;
	}
	return 0;
}
