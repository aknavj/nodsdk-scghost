/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: instance_catalog.cpp
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

#include "metadata_xml.h"
#include "../common/tool_common.h"
#include "NSDParser.h"
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace nod::metadata {
namespace {
bool source_signature(const std::string &value) {
	if(value.size() != 26 || value[8] != ' ' || value[17] != ' ') return false;
	for(size_t i = 0; i < value.size(); ++i)
		if(i != 8 && i != 17 && !(value[i] >= '0' && value[i] <= '9') && !(value[i] >= 'a' && value[i] <= 'f'))
			return false;
	return true;
}

void property_xml(const nod::edit::Document &document, const NODModifier &property,
				  bool local, const std::string &archive, const std::string &owner,
				  bool permitted, const std::string &restriction, bool protected_prefix, std::ostream &out) {
	auto field = document.modifier_field(property);
	if(protected_prefix && property.prop_id == 15) {
		field.kind = nod::edit::ValueKind::text; field.size = 32; field.editable = false;
	}
	const auto value = document.value(field);
	const auto *info = nodPropertyInfo(property.prop_id);
	std::string issue;
	bool enabled = editable(document, field, value, issue);
	if(!permitted) { enabled = false; issue = restriction; }
	out << "<property id=\"" << property.prop_id << "\" name=\"" << xml(protected_prefix && property.prop_id == 15 ?
		"Region class" : info ? info->name : "Unknown")
		<< "\" storage=\"" << storage(field.kind) << "\" value=\"" << xml(value)
		<< "\" archive=\"" << xml(archive) << "\" template=\"" << xml(owner)
		<< "\" source=\"" << (local ? "nsd" : "not") << "\" offset=\"" << field.offset
		<< "\" editable=\"" << (enabled ? "1" : "0") << "\" problem=\"" << xml(issue) << "\"/>\n";
}
}

int instance_catalog(int argc, char **argv) {
	std::map<std::string, std::string> options;
	for(int i = 1; i < argc; ++i) {
		const std::string name = argv[i];
		if((name != "--workspace" && name != "--asset-root" && name != "--scene" &&
			name != "--sources" && name != "--schema") || ++i == argc || !options.emplace(name, argv[i]).second) {
			std::cerr << "Invalid, duplicate or incomplete instance-query option: " << name << '\n'; return 1;
		}
	}
	const auto &signature = options["--sources"];
	const auto &schema_text = options["--schema"];
	unsigned schema = 0;
	if(schema_text.empty() || schema_text.size() > 2 || schema_text[0] == '0') {
		std::cerr << "An imported scene schema from 1 through 15 is required\n"; return 1;
	}
	for(char c : schema_text) {
		if(c < '0' || c > '9') { std::cerr << "Invalid imported scene schema\n"; return 1; }
		schema = schema * 10 + unsigned(c - '0');
	}
	if(schema < 1 || schema > 15 || !source_signature(signature) || options["--workspace"].empty() || options["--scene"].empty()) {
		std::cerr << "Specify workspace, NSD source, exact three-checksum scene signature and supported schema\n"; return 1;
	}
	nod::edit::Document scene;
	std::string error;
	if(!scene.configure_workspace(std::filesystem::u8path(options["--workspace"]),
								 std::filesystem::u8path(options["--asset-root"]), error) ||
	   !scene.load(std::filesystem::u8path(options["--scene"]), nod::edit::Format::nsd, error)) {
		std::cerr << error << '\n'; return 1;
	}
	Blob source{const_cast<unsigned char *>(scene.bytes().data()), scene.bytes().size()};
	std::ostringstream checksum;
	checksum << std::hex << std::setw(8) << std::setfill('0') << crc32(&source);
	if(signature.substr(9, 8) != checksum.str()) {
		std::cerr << "NSD checksum differs from the imported MAP's immutable source binding\n"; return 1;
	}
	NSDParser parser;
	if(!parser.parseBuffer(scene.bytes().data(), scene.bytes().size())) {
		std::cerr << parser.getLastError() << '\n'; return 1;
	}
	NOTDatabase database;
	if(!database.load(std::filesystem::u8path(options["--workspace"]), parser.getTemplateFiles(), error,
					  std::filesystem::u8path(options["--asset-root"]))) {
		std::cerr << error << '\n'; return 1;
	}
	if(parser.getSceneData().entity_array_count > 20480) {
		std::cerr << "Too many NSD instances for the metadata protocol\n"; return 1;
	}
	std::map<std::string, nod::edit::Document> documents;
	for(const auto &archive : database.snapshots())
		if(!documents[archive.name].load_bytes(archive.bytes, nod::edit::Format::not_file, error)) {
			std::cerr << error << '\n'; return 1;
		}
	std::ostringstream output;
	output << "<nod-instance-catalog version=\"1\" sources=\"" << signature << "\" schema=\"" << schema << "\">\n";
	for(const auto &archive : database.snapshots()) output << "<archive name=\"" << xml(archive.name) << "\"/>\n";
	for(const auto &warning : database.warnings()) output << "<warning text=\"" << xml(warning) << "\"/>\n";
	for(uint32_t i = 0; i < parser.getSceneData().entity_array_count; ++i) {
		const auto &entity = parser.getSceneData().entities[i];
		std::vector<NODModifier> modifiers;
		std::vector<NOTParser::ResolvedProperty> effective;
		std::map<uint32_t, const NODModifier *> local;
		std::set<size_t> protected_offsets;
		std::string problem;
		uint32_t type = 0;
		if(!parser.getModifiers(entity, modifiers)) problem = "NSD modifier count/layout is not verified";
		else {
			for(const auto &property : modifiers) local[property.prop_id] = &property;
			if(local.size() > 128) { std::cerr << "NSD local properties exceed the supported 128-field protocol limit\n"; return 1; }
			std::vector<nod::edit::Document::PlacementModifier> slots;
			if(scene.placement_modifiers(i, slots, problem))
				for(const auto &slot : slots) if(slot.protected_prefix) protected_offsets.insert(slot.field.offset);
			if(problem.empty() && database.resolveModifiers(modifiers.data(), modifiers.size(), effective, problem) == NOTParser::ResolveStatus::ok) {
				bool typed = false;
				for(const auto &property : effective)
					if(property.property->prop_id == 20) { type = u32(property.property->value); typed = true; }
				if(!typed) problem = "No effective native runtime type (property 20)";
			}
		}
		if(!problem.empty()) effective.clear();
		output << "<instance identity=\"nsd:" << entity.block_offset << "\" name=\"" << xml(entity.type_name)
			<< "\" type=\"" << type << "\" local-count=\"" << local.size() << "\" effective-count=\"" << effective.size()
			<< "\" problem=\"" << xml(problem) << "\">\n<local>\n";
		for(const auto &property : local) {
			const bool prefix = protected_offsets.count(property.second->value_offset) != 0;
			const bool permitted = problem.empty() && !prefix && schema >= 14 && nodPropertyMapEditable(property.first, schema == 15);
			const std::string restriction = !problem.empty() ? problem : schema < 14 ?
				"This imported MAP schema does not expose native local property editing" :
				"Native structure or preview-dependent property; edit the source map and re-enrich";
			property_xml(scene, *property.second, true, "", entity.type_name, permitted, restriction, prefix, output);
		}
		output << "</local>\n<effective>\n";
		for(const auto &resolved : effective) {
			if(!resolved.owner) {
				const bool prefix = protected_offsets.count(resolved.property->value_offset) != 0;
				const bool permitted = !prefix && schema >= 14 && nodPropertyMapEditable(resolved.property->prop_id, schema == 15);
				property_xml(scene, *resolved.property, true, "", entity.type_name, permitted,
					"Native structure or preview-dependent property; read-only", prefix, output);
			} else {
				const auto *archive = database.source(resolved.owner);
				if(!archive) { std::cerr << "Missing NSD effective-property archive provenance\n"; return 1; }
				property_xml(documents.at(archive->name), *resolved.property, false, archive->name, resolved.owner->name, false,
					"Inherited value; native modifier insertion is not supported in imported MAPs", false, output);
			}
		}
		output << "</effective>\n</instance>\n";
	}
	output << "</nod-instance-catalog>\n";
	const auto xml_text = output.str();
	if(xml_text.size() > 16 * 1024 * 1024) { std::cerr << "NSD metadata exceeds 16 MiB\n"; return 1; }
	std::cout << xml_text << std::flush;
	if(!std::cout.good()) { std::cerr << "Cannot write NSD metadata\n"; return 1; }
	return 0;
}
}
