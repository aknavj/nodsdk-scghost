/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: nodscript_cli.cpp
Version: 0.6.8

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "nod_script.h"
#include "NOTDatabase.h"
#include "../editors/workspace.h"
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <set>

namespace {
namespace fs = std::filesystem;

struct Token {
	std::string text;
	size_t line = 1, column = 1;
	bool quoted = false;
};

class Parser {
	const std::string &source;
	size_t offset = 0, line = 1, column = 1;
	Token token;
	std::string location(const Token &at) const {
		return std::to_string(at.line) + ":" + std::to_string(at.column) + ": ";
	}
	void advance() {
		if(source[offset++] == '\n') {
			++line;
			column = 1;
		} else {
			++column;
		}
	}
	bool next() {
		for(;;) {
			while(offset < source.size() && std::isspace(static_cast<unsigned char>(source[offset]))) {
				advance();
			}
			if(source.compare(offset, 2, "//") != 0) {
				break;
			}
			while(offset < source.size() && source[offset] != '\n') {
				advance();
			}
		}
		token = Token{"", line, column, false};
		if(offset == source.size()) {
			return true;
		}
		if(source[offset] == '"') {
			token.quoted = true;
			advance();
			while(offset < source.size() && source[offset] != '"') {
				const unsigned char c = source[offset];
				if(c < 32 || c > 126 || c == '\\') {
					return fail("Strings require printable ASCII without escapes");
				}
				token.text += source[offset];
				advance();
			}
			if(offset == source.size()) {
				return fail("Unterminated string");
			}
			advance();
			return true;
		}
		if(std::strchr("{}(),;", source[offset])) {
			token.text = source[offset];
			advance();
			return true;
		}
		while(offset < source.size() && !std::isspace(static_cast<unsigned char>(source[offset])) &&
			  !std::strchr("{}(),;\"", source[offset])) {
			const unsigned char c = source[offset];
			if(c < 33 || c > 126) {
				return fail("Source must be ASCII");
			}
			token.text += source[offset];
			advance();
		}
		return true;
	}
	bool fail(const std::string &message) {
		error = location(token) + message;
		return false;
	}
	bool expect(const char *value) {
		if(token.quoted || token.text != value) {
			return fail(std::string("Expected '") + value + "'");
		}
		return next();
	}
	bool scalar(std::string &value) {
		if(token.text.empty() || (!token.quoted && token.text.find_first_of("{}(),;") != std::string::npos)) {
			return fail("Expected a value");
		}
		value = token.text;
		return next();
	}
	bool vector(std::string &value) {
		if(!expect("(")) {
			return false;
		}
		for(size_t i = 0; i < 3; ++i) {
			float number = 0;
			if(token.quoted || !nod_scene::parse_float(token.text.c_str(), &number)) {
				return fail("Expected a finite coordinate");
			}
			if(i) {
				value += " ";
			}
			value += token.text;
			if(!next() || (i < 2 && !expect(","))) {
				return false;
			}
		}
		return expect(")");
	}
	static std::string number(double value) {
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::setprecision(9) << static_cast<float>(value);
		return stream.str();
	}
public:
	std::string error, entities;
	std::vector<size_t> lines;
	explicit Parser(const std::string &text) : source(text) {}
	bool parse() {
		if(!next() || !expect("nodscript") || !expect("1") || !expect(";")) {
			return false;
		}
		while(!token.text.empty()) {
			const size_t start = token.line;
			const std::string kind = token.text;
			const char *classname = kind == "console" ? "nod_console" : kind == "sound" ? "nod_sound_cue" :
				kind == "enemy" ? "nod_enemy_spawn" : kind == "sequence" ? "nod_trigger_relay" : nullptr;
			if(token.quoted || !classname) {
				return fail("Expected console, sound, enemy or sequence declaration; arbitrary code is unsupported");
			}
			if(lines.size() == 1024 || !next()) {
				return fail("At most 1024 declarations are supported");
			}
			std::string name;
			if(!scalar(name) || !expect("{")) {
				return false;
			}
			std::vector<std::pair<std::string, std::string>> fields{{"classname", classname}, {"targetname", name}};
			std::set<std::string> seen;
			size_t actions = 0;
			double elapsed = 0;
			while(token.text != "}" || token.quoted) {
				const std::string key = token.text;
				if(token.quoted || key.empty()) {
					return fail("Expected a setting or closing '}'");
				}
				if(!next()) {
					return false;
				}
				std::string value, output;
				if(kind == "sequence" && key == "wait") {
					float seconds = 0;
					if(token.quoted || !nod_scene::parse_float(token.text.c_str(), &seconds) || seconds < 0 ||
					   !std::isfinite(elapsed + seconds) || elapsed + seconds > FLT_MAX) {
						return fail("wait requires a finite nonnegative float32 duration");
					}
					elapsed += seconds;
					if(!next()) {
						return false;
					}
				} else if(kind == "sequence" && key == "trigger") {
					if(actions == 12 || !scalar(value)) {
						return fail("A sequence supports at most 12 trigger actions");
					}
					fields.emplace_back("nod_relay_target_" + std::to_string(actions), value);
					fields.emplace_back("nod_relay_delay_" + std::to_string(actions++), number(elapsed));
				} else {
					if(!seen.insert(key).second) {
						return fail("Duplicate setting: " + key);
					}
					if(key == "origin" || key == "angles" || (kind == "enemy" && key == "destination")) {
						output = key == "destination" ? "nod_destination" : key;
						if(!vector(value)) {
							return false;
						}
					} else {
						output = kind == "console" && key == "target" ? "target" :
							kind == "sound" && key == "cue" ? "nod_cue" :
							kind == "enemy" && key == "template" ? "nod_enemy" :
							kind == "enemy" && key == "limit" ? "nod_max_spawns" : "";
						if(output.empty()) {
							return fail("Unsupported setting: " + key);
						}
						if(!scalar(value)) {
							return false;
						}
					}
					fields.emplace_back(output, value);
				}
				if(!expect(";")) {
					return false;
				}
			}
			if(kind == "sequence") {
				fields.emplace_back("nod_relay_count", std::to_string(actions));
			}
			if((kind == "console" || kind == "enemy") && !seen.count("origin")) {
				return fail("Console/enemy declarations require an explicit origin");
			}
			entities += "\n{\n";
			for(const auto &field : fields) {
				if(field.second.size() >= nod_scene::map_value_capacity) {
					return fail("Setting exceeds the MAP value limit");
				}
				entities += "\"" + field.first + "\" \"" + field.second + "\"\n";
			}
			entities += "}\n";
			lines.push_back(start);
			if(!next()) {
				return false;
			}
		}
		if(lines.empty()) {
			return fail("Expected at least one declaration");
		}
		return true;
	}
};

bool read(const fs::path &path, size_t maximum, std::string &bytes, std::string &error) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	const auto length = file ? file.tellg() : std::streampos(-1);
	if(length < 0 || static_cast<uint64_t>(length) > maximum) {
		error = "Cannot read input or size limit exceeded: " + path.u8string();
		return false;
	}
	bytes.resize(static_cast<size_t>(length));
	file.seekg(0);
	if(!bytes.empty() && !file.read(&bytes[0], static_cast<std::streamsize>(bytes.size()))) {
		error = "Cannot read complete input: " + path.u8string();
		return false;
	}
	return true;
}

void remove_source_reference(std::string &map) {
	nod_scene::Lexer lexer{reinterpret_cast<const unsigned char *>(map.data()), map.size(), 0, {}, false};
	unsigned depth = 0;
	bool key = true, source_key = false;
	size_t key_start = 0;
	while(lexer.offset < lexer.size) {
		const size_t start = lexer.offset;
		if(!nod_scene::read_token(&lexer)) {
			break;
		}
		if(!lexer.is_quoted && std::strcmp(lexer.token, "{") == 0) {
			++depth;
		} else if(!lexer.is_quoted && std::strcmp(lexer.token, "}") == 0) {
			if(--depth == 0) {
				break;
			}
		} else if(depth == 1 && lexer.is_quoted) {
			if(key) {
				key_start = start;
				source_key = std::strcmp(lexer.token, "nod_script_file") == 0;
			} else if(source_key) {
				map.erase(key_start, lexer.offset - key_start);
				break;
			}
			key = !key;
		}
	}
}
} // namespace

int main(int argc, char **argv) {
	bool check = false;
	std::string label, error;
	fs::path workspace, assets;
	std::vector<std::string> archives;
	std::vector<fs::path> paths;
	for(int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if(arg == "--help") {
			std::cout << "NODScript: native-compatible script source compiler (language version 1)\n"
				"NODScript --check source.nods input.map\n"
				"NODScript --workspace DIR [--source-name NAME] source.nods input.map expanded.map\n"
				"Optional --asset-root DIR [--archive NAME ...] validates templates against NOT archives.\n"
				"Preserves MAP content; appends verified native control declarations.\n"
				"Supports console, sound, enemy, sequence, trigger and wait; no arbitrary runtime code.\n";
			return 0;
		} else if(arg == "--check") {
			check = true;
		} else if(arg == "--workspace" || arg == "--source-name" || arg == "--asset-root" || arg == "--archive") {
			if(++i == argc) {
				std::cerr << "NODScript: missing value for " << arg << "\n";
				return 1;
			}
			if(arg == "--workspace") {
				workspace = fs::u8path(argv[i]);
			} else if(arg == "--asset-root") {
				assets = fs::u8path(argv[i]);
			} else if(arg == "--archive") {
				archives.emplace_back(argv[i]);
			} else {
				label = argv[i];
			}
		} else if(arg.empty() || arg[0] == '-') {
			std::cerr << "NODScript: unknown option: " << arg << "\n";
			return 1;
		} else {
			paths.push_back(fs::u8path(arg));
		}
	}
	const auto failure = [&](const std::string &message) {
		std::cerr << "NODScript: " << message << "\n";
		return 1;
	};
	if(paths.size() != (check ? 2u : 3u)) {
		return failure("Expected source.nods, input.map and (unless --check) expanded.map; use --help");
	}
	if(paths[1].extension() != ".map" || (!check && paths[2].extension() != ".map")) {
		return failure("MAP input/output paths must have the .map extension");
	}
	if(label.empty()) {
		label = paths[0].u8string();
	}
	std::string source, map;
	if(!read(paths[0], 1024 * 1024, source, error) || !read(paths[1], 64 * 1024 * 1024, map, error)) {
		return failure(error);
	}
	Parser parser(source);
	if(!parser.parse()) {
		return failure(label + ":" + parser.error);
	}
	nod_scene::Map original{}, expanded{};
	Blob blob{reinterpret_cast<unsigned char *>(map.data()), map.size()};
	if(!nod_scene::parse_authoring_map(&blob, &original)) {
		return failure("Invalid input MAP");
	}
	const size_t first = original.count;
	if(!first || !nod_scene::get_property(&original.entities[0], "classname") ||
	   std::strcmp(nod_scene::get_property(&original.entities[0], "classname"), "worldspawn")) {
		nod_scene::free_map(&original);
		return failure("Input MAP requires a leading worldspawn");
	}
	std::map<std::string, uint32_t> imported;
	for(size_t i = 0; i < original.count; ++i) {
		const auto *id = nod_scene::entity_id(&original.entities[i]);
		if(id && std::strncmp(id, "nsd:", 4) == 0) {
			imported.emplace(id, static_cast<uint32_t>(imported.size()));
		}
	}
	nod_scene::free_map(&original);
	remove_source_reference(map);
	map += parser.entities;
	blob = Blob{reinterpret_cast<unsigned char *>(map.data()), map.size()};
	if(!nod_scene::parse_authoring_map(&blob, &expanded)) {
		return failure(label + ": generated MAP is invalid");
	}
	std::vector<std::vector<uint8_t>> records;
	NOTDatabase templates;
	if(!assets.empty()) {
		if(archives.empty()) {
			for(size_t i = 0; i < expanded.count; ++i) {
				if(!nod::script::template_class(expanded.entities[i])) {
					continue;
				}

				const auto *context = nod_scene::get_property(&expanded.entities[i], "nod_template_archives");
				std::istringstream input(context ? context : "");
				std::string archive;
				while(std::getline(input, archive, ';')) {
					archives.push_back(archive);
				}

				break;
			}
		}

		if(!templates.load(workspace, archives, error, assets)) {
			nod_scene::free_map(&expanded);
			return failure(error);
		}
	} else {
		if(!archives.empty()) {
			nod_scene::free_map(&expanded);
			return failure("--archive requires --asset-root and --workspace");
		}

		std::cerr << "NODScript: no NOT catalog supplied; template compatibility must be validated by MAPtoNSD/NODscene.\n";
	}

	const auto imported_count = static_cast<uint32_t>(imported.size());
	uint32_t first_preset = imported_count;
	for(size_t i = 0; i < expanded.count; ++i) {
		if(nod::script::template_class(expanded.entities[i])) {
			++first_preset;
		}
	}

	const bool targets_valid = nod::script::targets(expanded, imported_count, imported, error);
	for(auto target = imported.begin(); target != imported.end();) {
		if(target->second >= first_preset) {
			target = imported.erase(target);
		} else {
			++target;
		}
	}

	const bool valid = targets_valid && nod::script::compile(expanded, first_preset, imported, records, error,
														   assets.empty() ? nullptr : &templates);
	nod_scene::free_map(&expanded);
	if(!valid) {
		size_t index = first;
		if(error.compare(0, 7, "Entity ") == 0) {
			std::istringstream stream(error.substr(7));
			stream >> index;
		}
		const size_t line = index >= first && index - first < parser.lines.size() ? parser.lines[index - first] : 1;
		return failure(label + ":" + std::to_string(line) + ":1: " + error);
	}
	if(!check) {
		std::error_code ec;
		if(workspace.empty() || !fs::is_directory(workspace, ec) || ec ||
		   !nod::edit::workspace_contains(workspace, paths[2], error)) {
			return failure("Output must be inside an existing authoring workspace: " + error);
		}
		for(size_t i = 0; i < 2; ++i) {
			ec.clear();
			if(fs::weakly_canonical(paths[i], ec) == fs::weakly_canonical(paths[2], ec) && !ec) {
				return failure("Output must differ from both source inputs");
			}
		}
		const std::vector<uint8_t> bytes(map.begin(), map.end());
		if(!nod::edit::atomic_write(paths[2], bytes, true, error)) {
			return failure(error);
		}
	}
	std::cout << "NODScript: validated " << parser.lines.size() << " declarations, "
			  << records.size() << " native preset records" << (check ? "\n" : "; wrote expanded MAP\n");
	return 0;
}
