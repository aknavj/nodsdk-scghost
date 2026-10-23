/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: document.cpp
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
#include "tool_cli.h"
#include "workspace.h"
#include "NSDParser.h"
#include "NOTParser.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/*
 */
namespace nod::edit {
namespace {

/*
 */
uint32_t read_u32(const uint8_t *p) {
	return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

/*
 */
void write_u32(std::vector<uint8_t> &out, uint32_t value, size_t offset = 0) {
	for(size_t i = 0; i < 4; ++i) {
		out[offset + i] = uint8_t(value >> (i * 8));
	}
}

/*
 */
void write_text(std::vector<uint8_t> &out, size_t offset, const std::string &value) {
	std::copy(value.begin(), value.end(), out.begin() + offset);
	out[offset + value.size()] = 0;
}

/*
 */
size_t placement_prefix(const std::vector<NODModifier> &modifiers) {
	return nodPlacementPrefix(modifiers);
}

/*
 */
bool ascii_text(const std::vector<uint8_t> &bytes, size_t offset, size_t size) {
	for(size_t i = 0; i < size; ++i) {
		const auto c = bytes[offset + i];
		if(c == 0) {
			return true;
		}

		if(c < 32 || c > 126) {
			return false;
		}
	}

	return false;
}

/*
 */
Field property_field(const NODModifier &property, const std::vector<uint8_t> &bytes) {
	const auto *info = nodPropertyInfo(property.prop_id);
	Field field{info ? std::string(info->name) + " (tag " + std::to_string(property.prop_id) + ")"
					 : "Property " + std::to_string(property.prop_id),
				ValueKind::raw,
				property.value_offset,
				32,
				property.prop_id != 0,
				"Unknown native property; explicit raw payload editing only.",
				property.prop_id};
	field.native_modifier = true;
	if(!info) {
		return field;
	}

	field.description = "Engine handler " + std::to_string(info->handler) + ", native type " +
						std::to_string(info->native_type) + ". Unused payload bytes are preserved.";
	switch(info->native_type) {
		case 0:
			if(ascii_text(bytes, field.offset, 32)) {
				field.kind = ValueKind::text;
			} else {
				field.editable = false;
			}

			break;
		case 2:
			field.kind = ValueKind::float32;
			field.size = 4;
			break;
		case 3:
			field.kind = ValueKind::uint32;
			field.size = 4;
			break;
		case 4:
			field.kind = ValueKind::vector3;
			field.size = 12;
			break;
	}

	if(property.prop_id == 0) {
		field.description = "Template reference; use Reparent or Inheritance order.";
	}

	if(property.prop_id == 4) {
		field.name = "Model (tag 4)";
	}

	return field;
}

std::string lower_name(std::string name) {
	for(auto &c : name) {
		if(c >= 'A' && c <= 'Z') {
			c += 'a' - 'A';
		}
	}

	return name;
}

/*
 */
bool template_name(const std::string &name, std::string &error) {
	if(name.empty() || name.size() > 31 || std::all_of(name.begin(), name.end(), [](char c) { return c == ' '; }) ||
	   std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c > 126; })) {
		error = "Template names require 1 through 31 printable ASCII bytes, not just spaces";
		return false;
	}

	return true;
}

/*
 */
bool encode_value(const Field &entry, const std::vector<uint8_t> &bytes, const std::string &value,
				  std::vector<uint8_t> &replacement, std::string &error) {
	replacement.assign(entry.size, 0);
	std::istringstream input(value);
	input.imbue(std::locale::classic());
	switch(entry.kind) {
		case ValueKind::float32: {
			float number;
			if(!(input >> number) || !std::isfinite(number) || !(input >> std::ws).eof()) {
				error = "Expected one finite 32-bit floating-point number";
				return false;
			}

			uint32_t bits;
			std::memcpy(&bits, &number, 4);
			write_u32(replacement, bits);
			break;
		}

		case ValueKind::uint32: {
			uint64_t number;
			if(value.find('-') != std::string::npos || !(input >> number) || number > UINT32_MAX ||
			   !(input >> std::ws).eof()) {
				error = "Expected an unsigned 32-bit integer";
				return false;
			}

			write_u32(replacement, uint32_t(number));
			break;
		}

		case ValueKind::vector3:
			for(size_t i = 0; i < 3; ++i) {
				float number;
				if(!(input >> number) || !std::isfinite(number)) {
					error = "Expected three finite float32 components";
					return false;
				}

				uint32_t bits;
				std::memcpy(&bits, &number, 4);
				for(size_t j = 0; j < 4; ++j) {
					replacement[i * 4 + j] = uint8_t(bits >> (j * 8));
				}
			}

			if(!(input >> std::ws).eof()) {
				error = "Expected exactly three vector components";
				return false;
			}

			break;
		case ValueKind::text:
			if(value.size() >= entry.size ||
			   std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c > 126; })) {
				error = "Expected printable ASCII, at most " + std::to_string(entry.size - 1) + " bytes";
				return false;
			}

			// Leave unexposed bytes after the new terminator intact.
			std::copy_n(bytes.begin() + entry.offset, entry.size, replacement.begin());
			std::copy(value.begin(), value.end(), replacement.begin());
			replacement[value.size()] = 0;
			break;
		case ValueKind::raw:
			for(size_t i = 0; i < entry.size; ++i) {
				std::string token;
				if(!(input >> token) || token.size() != 2 ||
				   token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
					error = "Expected exactly " + std::to_string(entry.size) + " two-digit hex bytes";
					return false;
				}

				replacement[i] = uint8_t(std::stoul(token, nullptr, 16));
			}

			if(!(input >> std::ws).eof()) {
				error = "Too many hex bytes";
				return false;
			}

			break;
	}

	return true;
}

bool same_path(const std::filesystem::path &a, const std::filesystem::path &b, std::string &error) {
	std::error_code ec;
	const bool equivalent = std::filesystem::equivalent(a, b, ec);
	if(!ec) {
		return equivalent;
	}

	if(ec == std::errc::no_such_file_or_directory) {
		return false;
	}

	error = "Cannot compare file paths: " + ec.message();
	return false;
}

} // namespace

/*
 */
bool atomic_write(const std::filesystem::path &path, const std::vector<uint8_t> &bytes, bool replace,
				  std::string &error) {
	std::filesystem::path temporary;
	int fd = -1;
	for(unsigned i = 0; i < 100; ++i) {
		temporary = path;
		temporary += ".nodedit-" + std::to_string(i) + ".tmp";
#ifdef _WIN32
		fd = _wopen(temporary.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
		fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
#endif
		if(fd >= 0) {
			break;
		}

		if(errno != EEXIST) {
			error = "Cannot create save file: " + std::string(std::strerror(errno));
			return false;
		}
	}

	if(fd < 0) {
		error = "Cannot allocate a unique temporary save file";
		return false;
	}
#ifdef _WIN32
	FILE *file = _fdopen(fd, "wb");
#else
	FILE *file = fdopen(fd, "wb");
#endif
	bool ok = file != nullptr;
	if(file) {
		ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
		if(std::fflush(file) != 0) {
			ok = false;
		}
#ifdef _WIN32
		if(_commit(fd) != 0) {
			ok = false;
		}
#else
		if(fsync(fd) != 0) {
			ok = false;
		}
#endif
		if(std::fclose(file) != 0) {
			ok = false;
		}
	} else {
#ifdef _WIN32
		_close(fd);
#else
		close(fd);
#endif
	}

	if(!ok) {
		error = "Cannot finish writing save file; destination was not replaced";
	}

	if(ok) {
#ifdef _WIN32
		ok = MoveFileExW(temporary.c_str(), path.c_str(),
						 MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != 0;
		if(!ok) {
			error = "Cannot publish save file: " + std::system_category().message(GetLastError());
		}
#else
		if(replace) {
			ok = std::rename(temporary.c_str(), path.c_str()) == 0;
		} else {
			ok = link(temporary.c_str(), path.c_str()) == 0;
		}

		if(!ok) {
			error = "Cannot publish save file: " + std::string(std::strerror(errno));
		}
#endif
	}

	std::error_code cleanup;
	std::filesystem::remove(temporary, cleanup);
	if(cleanup) {
		error += (error.empty() ? "" : "; ");
		error += "Cannot remove temporary save file: " + cleanup.message();
		return false;
	}

	return ok;
}

/*
 */
bool Document::configure_workspace(const std::filesystem::path &workspace, const std::filesystem::path &assets,
								   std::string &error) {
	error.clear();
	std::error_code ec;
	if(workspace.empty() || !std::filesystem::is_directory(workspace, ec) || ec) {
		error = "The authoring workspace must be an existing directory";
		return false;
	}

	const auto root = std::filesystem::canonical(workspace, ec);
	if(ec) {
		error = "Cannot resolve authoring workspace: " + ec.message();
		return false;
	}

	std::filesystem::path source;
	if(!assets.empty()) {
		if(!std::filesystem::is_directory(assets, ec) || ec) {
			error = "The asset root must be an existing directory";
			return false;
		}

		source = std::filesystem::canonical(assets, ec);
		if(ec) {
			error = "Cannot resolve asset root: " + ec.message();
			return false;
		}

		if(workspace_contains(source, root, error)) {
			error = "The workspace must be outside the original asset root";
			return false;
		}

		if(!error.empty()) {
			return false;
		}

		if(workspace_contains(root, source, error)) {
			error = "The original asset root cannot be inside the workspace";
			return false;
		}

		if(!error.empty()) {
			return false;
		}
	}

	workspace_ = root;
	assets_	   = source;
	return true;
}

/*
 */
std::filesystem::path Document::suggested_save_path() const {
	if(path_.empty() && format_ == Format::not_file && !bytes_.empty() && saved_.empty()) {
		return workspace_.empty() ? std::filesystem::path("Untitled.not")
								  : workspace_ / "Misc" / "Templates" / "Untitled.not";
	}

	if(workspace_.empty() || path_.empty()) {
		return path_;
	}

	std::string error;
	if(workspace_contains(workspace_, path_, error)) {
		return path_;
	}

	if(!assets_.empty() && workspace_contains(assets_, path_, error)) {
		std::error_code ec;
		const auto relative = std::filesystem::relative(path_, assets_, ec);
		if(!ec) {
			return workspace_ / relative;
		}
	}

	return workspace_ / path_.filename();
}

/*
 */
bool Document::load(const std::filesystem::path &path, Format format, std::string &error) {
	error.clear();
	std::vector<uint8_t> bytes;
	if(!NodTools::read_file(path, bytes, error)) {
		return false;
	}

	Document next;
	if(!next.load_bytes(bytes, format, error)) {
		return false;
	}

	next.workspace_ = workspace_;
	next.assets_	= assets_;
	std::error_code ec;
	next.path_ = std::filesystem::absolute(path, ec);
	if(ec) {
		error = "Cannot resolve file path: " + ec.message();
		return false;
	}

	if(format == Format::nsd) {
		const auto root = workspace_.empty() ? next.path_.parent_path().parent_path() : workspace_;
		bool exists		= std::filesystem::exists(root / "Misc" / "Templates" / "Global.not", ec);
		if(ec) {
			error = "Cannot inspect template path: " + ec.message();
			return false;
		}

		if(!exists && !assets_.empty()) {
			exists = std::filesystem::exists(assets_ / "Misc" / "Templates" / "Global.not", ec);
			if(ec) {
				error = "Cannot inspect source template path: " + ec.message();
				return false;
			}
		}

		if(exists || !next.referenced_templates_.empty()) {
			NOTDatabase templates;
			if(!templates.load(root, next.referenced_templates_, error, assets_) ||
			   !next.set_template_archives(templates.snapshots(), error)) {
				return false;
			}
		}
	}

	*this = std::move(next);
	return true;
}

/*
 */
bool Document::load_bytes(const std::vector<uint8_t> &bytes, Format format, std::string &error) {
	error.clear();
	Document next;
	next.workspace_ = workspace_;
	next.assets_	= assets_;
	next.format_	= format;
	next.bytes_		= bytes;
	if(!(format == Format::nsd ? next.index_nsd(error) : next.index_not(error))) {
		return false;
	}

	next.saved_ = next.bytes_;
	*this		= std::move(next);
	return true;
}

/*
 */
bool Document::create_not_archive(const std::string &root_name, std::string &error) {
	error.clear();
	if(!template_name(root_name, error)) {
		return false;
	}

	std::vector<uint8_t> bytes(56, 0);
	std::copy_n("NOT\x10", 4, bytes.begin());
	write_u32(bytes, 1, 4);
	write_u32(bytes, 1, 8);
	write_text(bytes, 12, root_name);
	write_u32(bytes, 48, 44);
	Document next;
	next.workspace_ = workspace_;
	next.assets_	= assets_;
	if(!next.load_bytes(bytes, Format::not_file, error)) {
		return false;
	}

	next.saved_.clear();
	*this = std::move(next);
	return true;
}

/*
 */
bool Document::set_template_bytes(const std::vector<uint8_t> &bytes, std::string &error) {
	return set_template_archives(bytes.empty() ? std::vector<NOTDatabase::Snapshot>{}
											   : std::vector<NOTDatabase::Snapshot>{{"Global.not", bytes}},
								 error);
}

/*
 */
bool Document::set_template_archives(const std::vector<NOTDatabase::Snapshot> &archives, std::string &error) {
	error.clear();
	if(format_ != Format::nsd) {
		error = "Template context is only available for NSD documents";
		return false;
	}

	Document next			= *this;
	next.template_archives_ = archives;
	next.records_.clear();
	next.warnings_.clear();
	if(!next.index_nsd(error)) {
		return false;
	}

	*this = std::move(next);
	return true;
}

/*
 */
bool Document::insert_scene_archive(size_t before, const std::string &name, std::string &error) {
	error.clear();
	if(format_ != Format::nsd || bytes_.empty()) {
		error = "Open an NSD scene to add a template archive";
		return false;
	}

	const auto count = referenced_templates_.size();
	if(before == size_t(-1)) {
		before = count;
	}

	if(before > count || count >= NSDParser::MAX_TEMPLATE_FILES || bytes_.size() > SIZE_MAX - 32) {
		error = "Invalid archive position or the native nine-archive limit would be exceeded";
		return false;
	}

	if(!template_name(name, error)) {
		error = "Archive references require 1 through 31 printable ASCII bytes, not just spaces";
		return false;
	}

	std::string canonical;
	if(!NOTDatabase::archiveName(name, canonical, error)) {
		return false;
	}

	if(canonical == "global.not") {
		error = "Global.not is loaded implicitly before the scene archive list";
		return false;
	}

	for(const auto &existing : referenced_templates_) {
		std::string normalized;
		if(!NOTDatabase::archiveName(existing, normalized, error)) {
			return false;
		}

		if(normalized == canonical) {
			error = "That archive is already in the scene list";
			return false;
		}
	}

	std::vector<uint8_t> entry(32, 0);
	write_text(entry, 0, name);
	auto candidate = bytes_;
	candidate.insert(candidate.begin() + 80 + before * 32, entry.begin(), entry.end());
	write_u32(candidate, uint32_t(count + 1), 76);
	return commit_scene_archives(std::move(candidate), error);
}

/*
 */
bool Document::remove_scene_archive(size_t slot, std::string &error) {
	error.clear();
	if(format_ != Format::nsd || slot >= referenced_templates_.size()) {
		error = "Select a declared NSD template archive to remove";
		return false;
	}

	auto candidate = bytes_;
	candidate.erase(candidate.begin() + 80 + slot * 32, candidate.begin() + 80 + (slot + 1) * 32);
	write_u32(candidate, uint32_t(referenced_templates_.size() - 1), 76);
	return commit_scene_archives(std::move(candidate), error);
}

/*
 */
bool Document::move_scene_archive(size_t slot, size_t before, std::string &error) {
	error.clear();
	const auto count = referenced_templates_.size();
	if(before == size_t(-1)) {
		before = count;
	}

	if(format_ != Format::nsd || slot >= count || before > count) {
		error = "Select an NSD archive and a valid destination";
		return false;
	}

	if(before == slot || before == slot + 1) {
		return true;
	}

	auto candidate		= bytes_;
	const size_t source = 80 + slot * 32;
	const std::vector<uint8_t> entry(candidate.begin() + source, candidate.begin() + source + 32);
	candidate.erase(candidate.begin() + source, candidate.begin() + source + 32);
	if(before > slot) {
		--before;
	}

	candidate.insert(candidate.begin() + 80 + before * 32, entry.begin(), entry.end());
	return commit_scene_archives(std::move(candidate), error);
}

/*
 */
bool Document::commit_scene_archives(std::vector<uint8_t> candidate, std::string &error) {
	if(candidate == bytes_) {
		return true;
	}

	Document validation;
	if(!validation.load_bytes(candidate, Format::nsd, error)) {
		return false;
	}

	auto root = workspace_;
	if(root.empty() && !path_.empty()) {
		root = path_.parent_path().parent_path();
	}

	std::vector<std::string> requests{"Global.not"};
	requests.insert(requests.end(), validation.referenced_templates_.begin(), validation.referenced_templates_.end());
	std::vector<NOTDatabase::Snapshot> archives;
	std::set<std::string> loaded;
	for(const auto &request : requests) {
		std::string normalized;
		if(!NOTDatabase::archiveName(request, normalized, error)) {
			return false;
		}

		if(!loaded.insert(normalized).second) {
			continue;
		}

		const NOTDatabase::Snapshot *cached = nullptr;
		for(const auto &snapshot : template_archives_) {
			std::string name;
			if(!NOTDatabase::archiveName(snapshot.name, name, error)) {
				return false;
			}

			if(name == normalized) {
				cached = &snapshot;
				break;
			}
		}

		if(cached) {
			archives.push_back(*cached);
		} else {
			if(root.empty()) {
				error = "Open the scene from disk, configure a workspace or supply every required archive snapshot";
				return false;
			}

			NOTDatabase::Snapshot snapshot;
			if(!NOTDatabase::readArchive(root, normalized, snapshot, error, assets_)) {
				return false;
			}

			archives.push_back(std::move(snapshot));
		}
	}

	if(!validation.set_template_archives(archives, error)) {
		return false;
	}

	return commit_nsd_structure(std::move(validation));
}

/*
 */
bool Document::commit_nsd_structure(Document &&validation) {
	if(validation.bytes_ == bytes_) {
		return true;
	}

	Change change{0, bytes_, validation.bytes_, true};
	change.archives_before = template_archives_;
	change.archives_after  = validation.template_archives_;
	history_.resize(cursor_);
	history_.push_back(std::move(change));
	bytes_				  = std::move(validation.bytes_);
	records_			  = std::move(validation.records_);
	warnings_			  = std::move(validation.warnings_);
	referenced_templates_ = std::move(validation.referenced_templates_);
	template_archives_	  = std::move(validation.template_archives_);
	cursor_				  = history_.size();
	return true;
}

/*
 */
bool Document::index_nsd(std::string &error) {
	NSDParser parser;
	if(!parser.parseBuffer(bytes_.data(), bytes_.size())) {
		error = parser.getLastError();
		return false;
	}

	const auto &scene	  = parser.getSceneData();
	referenced_templates_ = parser.getTemplateFiles();
	NOTDatabase templates;
	if(!templates.loadBuffers(template_archives_, error,
							  template_archives_.empty() ? std::vector<std::string>{} : referenced_templates_)) {
		return false;
	}

	warnings_.insert(warnings_.end(), templates.warnings().begin(), templates.warnings().end());
	std::vector<Document> template_values(templates.snapshots().size());
	for(size_t i = 0; i < template_values.size(); ++i) {
		template_values[i].bytes_ = templates.snapshots()[i].bytes;
	}

	if(template_archives_.empty()) {
		warnings_.push_back("No NOT context. Open a Levels NSD with Misc/Templates available to resolve properties.");
	}

	warnings_.push_back(
		"Partial NSD decoding; unknown data preserved. Placement add/delete and class conversion unsupported.");
	if(scene.entity_array_count != scene.entity_count) {
		warnings_.push_back("Entity count mismatch; unrecognized records preserved.");
	}

	for(uint32_t i = 0; i < scene.entity_array_count; ++i) {
		const auto &entity = scene.entities[i];
		Record record;
		record.name	  = std::string(entity.type_name) + " [" + std::to_string(i) + "]";
		record.type	  = entity.type_name;
		record.offset = entity.block_offset;
		record.size	  = entity.block_size;
		record.fields.push_back(
			{"Native type", ValueKind::text, record.offset, 32, false,
			 "Primary template; Change placement template allows same-runtime-class reassignment only."});
		static const char *axes[] = {"X", "Y", "Z"};
		for(size_t axis = 0; axis < 3; ++axis) {
			record.fields.push_back({std::string("Position ") + axes[axis], ValueKind::float32,
									 record.offset + 72 + axis * 4, 4, true, "Native world units, Z-up."});
			record.fields.push_back({std::string("Rotation ") + axes[axis], ValueKind::float32,
									 record.offset + 36 + axis * 4, 4, true,
									 "Native Euler degrees (pitch X, yaw Y, roll Z), not Radiant angles."});
		}

		if(entity.has_sector) {
			record.fields.push_back({"Sector", ValueKind::uint32, record.offset + 108, 4, true,
									 "Native sector index; cross-file membership is not rebuilt or validated.", 3});
		}

		if(entity.has_model_override && entity.model_override_valid) {
			for(size_t slot = 140; slot < 284 && slot + 36 <= record.size; slot += 36) {
				if(read_u32(bytes_.data() + record.offset + slot) != 4) {
					continue;
				}

				record.fields.push_back(
					{"Model override", ValueKind::text, record.offset + slot + 4, 32, true,
					 "Existing tag-4 reference; maximum 31 ASCII bytes. Empty means an explicit empty override.", 4});
				break;
			}
		}

		if(entity.script_name[0]) {
			record.fields.push_back({"Script", ValueKind::text, record.offset + 144, 60, false,
									 "Inspected only; complete script payload layout is not verified."});
		}

		std::vector<NODModifier> modifiers;
		if(parser.getModifiers(entity, modifiers)) {
			if(entity.script_name[0]) {
				record.fields.pop_back();
			}

			for(const auto &property : modifiers) {
				for(auto &field : record.fields) {
					if(field.property_id == property.prop_id && field.offset == property.value_offset) {
						field.native_modifier = true;
					}
				}

				if(property.prop_id > 4 || property.prop_id == 0 ||
				   (property.prop_id == 1 && property.value_offset != record.offset + 36) ||
				   (property.prop_id == 2 && property.value_offset != record.offset + 72) ||
				   (property.prop_id == 3 && property.value_offset != record.offset + 108) ||
				   (property.prop_id == 4 &&
					std::none_of(record.fields.begin(), record.fields.end(),
								 [&](const Field &field) { return field.offset == property.value_offset; }))) {
					record.fields.push_back(property_field(property, bytes_));
				}
			}

			if(!template_archives_.empty()) {
				std::vector<NOTParser::ResolvedProperty> resolved;
				std::string issue;
				if(templates.resolveModifiers(modifiers.data(), modifiers.size(), resolved, issue) !=
				   NOTParser::ResolveStatus::ok) {
					warnings_.push_back(record.name + ": " + issue);
				} else {
					for(const auto &property : resolved) {
						const bool external = property.owner != nullptr;
						const auto *archive = external ? templates.source(property.owner) : nullptr;
						if(external && !archive) {
							error = "Missing effective-property archive provenance";
							return false;
						}

						const auto field = property_field(*property.property, external ? archive->bytes : bytes_);
						const size_t archive_index = external ? size_t(archive - templates.snapshots().data()) : 0;
						size_t source_field		   = 0;
						if(!external) {
							const auto match =
								std::find_if(record.fields.begin(), record.fields.end(),
											 [&](const Field &entry) { return entry.offset == field.offset; });
							if(match != record.fields.end()) {
								source_field = size_t(match - record.fields.begin());
							}
						}

						record.effective.push_back(
							{field, external ? "NOT " + archive->name + ": " + property.owner->name : "NSD placement",
							 external ? template_values[archive_index].value(field) : "", i, source_field, external,
							 external ? archive->name : ""});
					}
				}
			}
		} else {
			warnings_.push_back(
				record.name +
				": modifier count/layout not verified; only legacy validated placement fields are exposed.");
		}

		records_.push_back(std::move(record));
	}

	return true;
}

/*
 */
bool Document::index_not(std::string &error) {
	NOTParser parser;
	if(!parser.parseBuffer(bytes_.data(), bytes_.size())) {
		error = parser.getLastError();
		return false;
	}

	warnings_.push_back(
		"Integers are raw uint32 bits. Gameplay enums and cross-file references are not fully checked.");
	const auto &db = parser.getTemplateDB();
	for(uint32_t i = 0; i < db.num_templates; ++i) {
		const auto &tmpl = db.templates[i];
		Record record;
		record.name	  = tmpl.name;
		record.parent = tmpl.parent;
		record.offset = tmpl.data_offset;
		record.size	  = tmpl.data_size;
		record.type	  = "template";
		record.fields.push_back({"Name", ValueKind::text, 12 + size_t(i) * 36, 32, false,
								 "Rename updates local basedon references only; external references stay unchanged."});
		std::set<uint32_t> property_ids;
		size_t references = 0;
		for(uint32_t p = 0; p < tmpl.property_count; ++p) {
			const auto &prop = tmpl.properties[p];
			if(prop.prop_id == 0) {
				++references;
			}

			if(!property_ids.insert(prop.prop_id).second) {
				warnings_.push_back(record.name + ": repeated property ID " + std::to_string(prop.prop_id) +
									"; expansion follows source order, later values replace earlier ones.");
			}

			record.fields.push_back(property_field(prop, bytes_));
		}

		if(references > 1) {
			warnings_.push_back(
				record.name + ": multiple basedon references; tree shows the first, effective properties expand all.");
		}

		if(tmpl.extra_size) {
			record.fields.push_back(
				{"Editor data", ValueKind::raw, tmpl.extra_offset, tmpl.extra_size, false,
				 "Separate length-prefixed editor metadata. Not a runtime property; preserved byte-identically."});
		}

		if(tmpl.trailing_size) {
			warnings_.push_back(record.name + ": " + std::to_string(tmpl.trailing_size) +
								" trailing bytes ignored by the engine are preserved.");
		}

		records_.push_back(std::move(record));
	}

	link_parents();
	for(size_t i = 0; i < records_.size(); ++i) {
		std::vector<NOTParser::ResolvedProperty> resolved;
		std::string issue;
		if(parser.resolveModifiers(records_[i].name.c_str(), resolved, issue) != NOTParser::ResolveStatus::ok) {
			warnings_.push_back(records_[i].name + ": " + issue);
			continue;
		}

		for(const auto &property : resolved) {
			const size_t owner = size_t(property.owner - db.templates);
			for(size_t f = 0; f < records_[owner].fields.size(); ++f) {
				if(records_[owner].fields[f].offset == property.property->value_offset) {
					records_[i].effective.push_back(
						{records_[owner].fields[f], records_[owner].name, "", owner, f, false});
				}
			}
		}
	}

	return true;
}

/*
 */
void Document::link_parents() {
	std::map<std::string, int> names;
	std::set<std::string> duplicates;
	for(size_t i = 0; i < records_.size(); ++i) {
		if(!names.emplace(lower_name(records_[i].name), static_cast<int>(i)).second) {
			duplicates.insert(lower_name(records_[i].name));
			warnings_.push_back("Duplicate template name: " + records_[i].name);
		}
	}

	for(auto &record : records_) {
		if(record.parent.empty()) {
			continue;
		}

		const auto found = names.find(lower_name(record.parent));
		if(found == names.end() || duplicates.count(lower_name(record.parent))) {
			warnings_.push_back(record.name + ": unresolved or ambiguous parent " + record.parent);
		} else {
			record.parent_index = found->second;
		}
	}

	for(size_t i = 0; i < records_.size(); ++i) {
		std::set<int> seen;
		int current = static_cast<int>(i);
		while(current >= 0) {
			if(!seen.insert(current).second) {
				warnings_.push_back(records_[i].name + ": inheritance cycle; detached in the tree only.");
				records_[i].parent_index = -1;
				break;
			}

			current = records_[size_t(current)].parent_index;
		}
	}
}

/*
 */
std::string Document::value(const Field &field) const {
	std::ostringstream out;
	out.imbue(std::locale::classic());
	const auto *data = bytes_.data() + field.offset;
	switch(field.kind) {
		case ValueKind::float32: {
			uint32_t bits = read_u32(data);
			float number;
			std::memcpy(&number, &bits, 4);
			out << std::setprecision(std::numeric_limits<float>::max_digits10) << number;
			break;
		}

		case ValueKind::uint32:
			out << read_u32(data);
			break;
		case ValueKind::vector3:
			for(size_t i = 0; i < 3; ++i) {
				const auto bits = read_u32(data + i * 4);
				float number;
				std::memcpy(&number, &bits, 4);
				if(i) {
					out << ' ';
				}

				out << std::setprecision(std::numeric_limits<float>::max_digits10) << number;
			}

			break;
		case ValueKind::text:
			for(size_t i = 0; i < field.size && data[i]; ++i) {
				out << char(data[i]);
			}

			break;
		case ValueKind::raw:
			for(size_t i = 0; i < field.size; ++i) {
				if(i) {
					out << ' ';
				}

				out << std::hex << std::setw(2) << std::setfill('0') << unsigned(data[i]);
			}

			break;
	}

	return out.str();
}

/*
 */
Field Document::modifier_field(const NODModifier &modifier) const {
	return property_field(modifier, bytes_);
}

/*
 */
bool encode_modifier(uint32_t id, const std::string &value, NODModifier &modifier, std::string &error) {
	error.clear();
	NODModifier next{};
	next.prop_id	  = id;
	next.value_size	  = 32;
	next.value_offset = 4;
	std::vector<uint8_t> property(36, 0);
	const auto field = property_field(next, property);
	if(!field.editable || field.kind == ValueKind::raw) {
		error = "New properties require a verified string, float32, uint32 or vector3 storage type";
		return false;
	}

	std::vector<uint8_t> replacement;
	if(!encode_value(field, property, value, replacement, error)) {
		return false;
	}

	std::copy(replacement.begin(), replacement.end(), next.value);
	modifier = next;
	return true;
}

/*
 */
bool Document::validate_value(const Field &field, const std::string &value, std::string &error) const {
	error.clear();
	if(!field.editable || field.offset > bytes_.size() || field.size > bytes_.size() - field.offset) {
		error = "This field is read-only or outside its document";
		return false;
	}

	std::vector<uint8_t> replacement;
	return encode_value(field, bytes_, value, replacement, error);
}

/*
 */
bool Document::edit(size_t record, size_t field, const std::string &value, std::string &error) {
	error.clear();
	if(record >= records_.size() || field >= records_[record].fields.size()) {
		error = "No such field";
		return false;
	}

	const auto &entry = records_[record].fields[field];
	if(!entry.editable) {
		error = "This field is read-only";
		return false;
	}

	std::vector<uint8_t> replacement;
	if(!encode_value(entry, bytes_, value, replacement, error)) {
		return false;
	}

	Change change{entry.offset,
				  std::vector<uint8_t>(bytes_.begin() + entry.offset, bytes_.begin() + entry.offset + entry.size),
				  std::move(replacement)};
	if(change.before == change.after) {
		return true;
	}

	auto candidate = bytes_;
	std::copy(change.after.begin(), change.after.end(), candidate.begin() + change.offset);
	Document validation;
	if(!validation.load_bytes(candidate, format_, error)) {
		return false;
	}

	if(!same_layout(validation)) {
		error = "This edit changes the decoded record layout and cannot be safely patched";
		return false;
	}

	history_.resize(cursor_);
	history_.push_back(std::move(change));
	const auto &last = history_.back();
	std::copy(last.after.begin(), last.after.end(), bytes_.begin() + last.offset);
	cursor_ = history_.size();
	return true;
}

/*
 */
bool Document::insert_property(size_t record, uint32_t property_id, const std::string &value, std::string &error,
							   bool check_inheritance) {
	error.clear();
	if(record >= records_.size()) {
		error = "Select a template or verified scene placement to add a local property";
		return false;
	}

	if(property_id == 0 || !nodPropertyInfo(property_id)) {
		error = "Choose a known native property; basedon references use the inheritance controls";
		return false;
	}

	if(format_ == Format::nsd && property_id <= 3) {
		error = "Placement references, transforms and sector use their protected existing fields, not new modifiers";
		return false;
	}

	std::vector<uint8_t> property(36, 0);
	write_u32(property, property_id);
	NODModifier modifier{};
	if(!encode_modifier(property_id, value, modifier, error)) {
		return false;
	}

	std::copy(std::begin(modifier.value), std::end(modifier.value), property.begin() + 4);
	if(format_ == Format::nsd) {
		return splice_nsd_property(record, size_t(-1), property, error);
	}

	const auto count = read_u32(bytes_.data() + records_[record].offset);
	return splice_not_property(record, records_[record].offset + 4 + size_t(count) * 36, property, error,
							   check_inheritance);
}

/*
 */
bool Document::insert_property(size_t record, size_t before, const std::vector<uint8_t> &property, std::string &error,
							   bool check_inheritance) {
	error.clear();
	if(format_ != Format::not_file || record >= records_.size() || property.size() != 36) {
		error = "Select a NOT template and a complete native modifier to restore";
		return false;
	}

	const auto id = read_u32(property.data());
	if(id == 0 || !nodPropertyInfo(id)) {
		error = "Restore requires a known non-reference native property";
		return false;
	}

	NODModifier modifier{};
	modifier.prop_id	  = id;
	modifier.value_offset = 4;
	modifier.value_size	  = 32;
	const auto field	  = property_field(modifier, property);
	Document payload;
	payload.bytes_ = property;
	if(!field.editable || field.kind == ValueKind::raw || !payload.validate_value(field, payload.value(field), error)) {
		if(error.empty()) {
			error = "Restore requires a verified typed payload";
		}

		return false;
	}

	const auto count = read_u32(bytes_.data() + records_[record].offset);
	if(before == size_t(-1)) {
		before = count;
	}

	if(before > count) {
		error = "The restore ordinal is outside the native modifier array";
		return false;
	}

	return splice_not_property(record, records_[record].offset + 4 + before * 36, property, error, check_inheritance);
}

/*
 */
bool Document::override_property(size_t record, uint32_t property_id, std::string &error) {
	error.clear();
	if(record >= records_.size()) {
		error = "Select a template or verified scene placement to create a local override";
		return false;
	}

	if(property_id == 0 || property_id == UINT32_MAX) {
		error = "Template references and reserved property IDs cannot be inserted as overrides";
		return false;
	}

	if(format_ == Format::nsd && property_id <= 3) {
		error =
			"Placement references, transforms and sector use their protected existing fields, not inherited insertion";
		return false;
	}

	const auto &target = records_[record];
	const auto inherited =
		std::find_if(target.effective.begin(), target.effective.end(),
					 [&](const Record::EffectiveProperty &entry) { return entry.field.property_id == property_id; });
	if(inherited == target.effective.end()) {
		error = "No resolved inherited property with that ID is available";
		return false;
	}

	if(format_ == Format::nsd ? !inherited->external : inherited->record == record) {
		error = "The effective value is already local; edit that property instead";
		return false;
	}

	const auto *source_bytes = &bytes_;
	if(format_ == Format::nsd) {
		source_bytes = nullptr;
		for(const auto &snapshot : template_archives_) {
			std::string name;
			if(!NOTDatabase::archiveName(snapshot.name, name, error)) {
				return false;
			}

			if(name == inherited->archive) {
				source_bytes = &snapshot.bytes;
				break;
			}
		}

		if(!source_bytes) {
			error = "Missing inherited property's archive snapshot";
			return false;
		}
	}

	const size_t value = inherited->field.offset;
	if(value < 4 || source_bytes->size() < 32 || value > source_bytes->size() - 32 ||
	   read_u32(source_bytes->data() + value - 4) != property_id) {
		error = "Inherited property does not identify a complete native payload";
		return false;
	}

	const size_t source = value - 4;
	const std::vector<uint8_t> property(source_bytes->begin() + source, source_bytes->begin() + source + 36);
	if(format_ == Format::nsd) {
		return splice_nsd_property(record, size_t(-1), property, error);
	}

	const auto count = read_u32(bytes_.data() + target.offset);
	return splice_not_property(record, target.offset + 4 + size_t(count) * 36, property, error);
}

/*
 */
bool Document::removable_property(size_t record, size_t field) const {
	if(record >= records_.size() || field >= records_[record].fields.size()) {
		return false;
	}

	const auto &entry = records_[record].fields[field];
	if(!entry.native_modifier || entry.property_id == 0 || entry.property_id == UINT32_MAX) {
		return false;
	}

	if(format_ == Format::not_file) {
		return true;
	}

	const auto prefix = records_[record].offset;
	return entry.offset >= prefix + 108 &&
		   (entry.offset != prefix + 108 || (entry.property_id != 3 && entry.property_id != 15));
}

/*
 */
bool Document::remove_property(size_t record, size_t field, std::string &error) {
	error.clear();
	if(!removable_property(record, field)) {
		error = "Select a complete local modifier; references, names, editor data and the required placement prefix "
				"are protected";
		return false;
	}

	const auto &entry = records_[record].fields[field];
	if(format_ == Format::nsd) {
		return splice_nsd_property(record, entry.offset - 4, {}, error);
	}

	return splice_not_property(record, entry.offset - 4, {}, error);
}

/*
 */
bool Document::placement_templates(std::vector<PlacementTemplate> &choices, std::string &error) const {
	error.clear();
	choices.clear();
	if(format_ != Format::nsd || template_archives_.empty()) {
		error = "Open an NSD with a cached NOT template context to choose a placement template";
		return false;
	}

	NOTDatabase templates;
	if(!templates.loadBuffers(template_archives_, error, referenced_templates_)) {
		return false;
	}

	for(const auto &entry : templates.templates()) {
		const auto *source = templates.source(entry.second);
		if(!source) {
			error = "Missing placement template archive provenance";
			choices.clear();
			return false;
		}

		choices.push_back({entry.second->name, source->name});
	}

	return true;
}

/*
 */
bool Document::reassign_placement_template(size_t record, const std::string &name, std::string &error) {
	error.clear();
	std::vector<NODModifier> modifiers;
	if(!nsd_modifiers(record, modifiers, error)) {
		return false;
	}

	const auto start = records_[record].offset;
	auto changed	 = modifiers;
	if(!placement_reference(name, changed[0], error) || !validate_placement_inheritance(modifiers, changed, error)) {
		return false;
	}

	std::string target_name;
	if(!nodModifierString(changed[0], target_name)) {
		error = "Invalid primary reference payload";
		return false;
	}

	auto candidate = bytes_;
	std::copy_n(changed[0].value, 32, candidate.begin() + start);
	if(candidate == bytes_) {
		return true;
	}

	Document validation;
	if(!validation.load_bytes(candidate, Format::nsd, error) ||
	   !validation.set_template_archives(template_archives_, error)) {
		return false;
	}

	if(!same_layout(validation) || validation.records_[record].type != target_name) {
		error = "Template reassignment changes placement recognition or layout and cannot be safely applied";
		return false;
	}

	return commit_nsd_structure(std::move(validation));
}

/*
 */
bool Document::nsd_modifiers(size_t record, std::vector<NODModifier> &modifiers, std::string &error) const {
	error.clear();
	modifiers.clear();
	if(format_ != Format::nsd || record >= records_.size()) {
		error = "Select a verified NSD placement";
		return false;
	}

	NSDParser::SceneEntity entity{};
	entity.block_offset = records_[record].offset;
	entity.block_size	= records_[record].size;
	if(!NSDParser::readModifiers(bytes_.data(), bytes_.size(), entity, modifiers)) {
		error = "The placement's counted native modifier array is not verified";
		modifiers.clear();
		return false;
	}

	if(record + 1 < records_.size() &&
	   records_[record].offset - 4 + modifiers.size() * 36 > records_[record + 1].offset - 8) {
		error = "The modifier array overlaps the next placement's header";
		modifiers.clear();
		return false;
	}

	return true;
}

/*
 */
bool Document::placement_modifiers(size_t record, std::vector<PlacementModifier> &out, std::string &error) const {
	out.clear();
	std::vector<NODModifier> modifiers;
	if(!nsd_modifiers(record, modifiers, error)) {
		return false;
	}

	const auto prefix = placement_prefix(modifiers);
	for(size_t i = 0; i < modifiers.size(); ++i) {
		out.push_back({property_field(modifiers[i], bytes_), i < prefix});
	}

	return true;
}

/*
 */
bool Document::placement_reference(const std::string &name, NODModifier &modifier, std::string &error) const {
	if(!template_name(name, error)) {
		return false;
	}

	if(template_archives_.empty()) {
		error = "Placement inheritance editing requires cached NOT context";
		return false;
	}

	NOTDatabase templates;
	if(!templates.loadBuffers(template_archives_, error, referenced_templates_)) {
		return false;
	}

	const auto *target = templates.findTemplate(name.c_str());
	if(!target) {
		error = "Missing template: " + name;
		return false;
	}

	const std::string target_name = target->name;
	if(!template_name(target_name, error)) {
		return false;
	}

	modifier			= {};
	modifier.value_size = 32;
	std::copy(target_name.begin(), target_name.end(), modifier.value);
	return true;
}

/*
 */
bool Document::validate_placement_inheritance(const std::vector<NODModifier> &before,
											  const std::vector<NODModifier> &after, std::string &error) const {
	if(template_archives_.empty()) {
		error = "Placement inheritance editing requires cached NOT context";
		return false;
	}

	NOTDatabase templates;
	if(!templates.loadBuffers(template_archives_, error, referenced_templates_)) {
		return false;
	}

	auto runtime_class = [&](const std::vector<NODModifier> &modifiers, uint32_t &type) {
		std::vector<NOTParser::ResolvedProperty> resolved;
		if(templates.resolveModifiers(modifiers.data(), modifiers.size(), resolved, error) !=
		   NOTParser::ResolveStatus::ok) {
			return false;
		}

		for(const auto &entry : resolved) {
			if(entry.property->prop_id == 20) {
				type = read_u32(entry.property->value);
				return true;
			}
		}

		error =
			"The placement has no resolved native type (property 20); inheritance editing cannot verify compatibility";
		return false;
	};
	uint32_t before_type = 0;
	uint32_t after_type	 = 0;
	if(!runtime_class(before, before_type) || !runtime_class(after, after_type)) {
		return false;
	}

	if(before_type != after_type) {
		error = "Placement inheritance would change the runtime class; cross-class body conversion is not supported";
		return false;
	}

	return true;
}

/*
 */
bool Document::commit_placement_references(size_t record, const std::vector<NODModifier> &before,
										   const std::vector<NODModifier> &after, std::string &error) {
	if(after.size() > NOTParser::MAX_PROPERTIES) {
		error = "The native 128-modifier limit would be exceeded";
		return false;
	}

	if(after.size() > before.size() && bytes_.size() > SIZE_MAX - 36) {
		error = "The native scene size limit would be exceeded";
		return false;
	}

	if(!validate_placement_inheritance(before, after, error)) {
		return false;
	}

	std::vector<uint8_t> serialized(after.size() * 36, 0);
	for(size_t i = 0; i < after.size(); ++i) {
		write_u32(serialized, after[i].prop_id, i * 36);
		std::copy_n(after[i].value, 32, serialized.begin() + i * 36 + 4);
	}

	const auto start = records_[record].offset - 4;
	auto candidate	 = bytes_;
	candidate.erase(candidate.begin() + start, candidate.begin() + start + before.size() * 36);
	candidate.insert(candidate.begin() + start, serialized.begin(), serialized.end());
	write_u32(candidate, uint32_t(after.size()), start - 4);
	if(candidate == bytes_) {
		return true;
	}

	return commit_nsd_placement_change(record, std::move(candidate), error);
}

/*
 */
bool Document::insert_placement_reference(size_t record, size_t before, const std::string &name, std::string &error) {
	std::vector<NODModifier> modifiers;
	if(!nsd_modifiers(record, modifiers, error)) {
		return false;
	}

	if(before == size_t(-1)) {
		before = modifiers.size();
	}

	if(before < placement_prefix(modifiers) || before > modifiers.size()) {
		error = "References must be inserted after the protected placement prefix";
		return false;
	}

	NODModifier reference{};
	if(!placement_reference(name, reference, error)) {
		return false;
	}

	auto changed = modifiers;
	changed.insert(changed.begin() + before, reference);
	return commit_placement_references(record, modifiers, changed, error);
}

/*
 */
bool Document::replace_placement_reference(size_t record, size_t slot, const std::string &name, std::string &error) {
	std::vector<NODModifier> modifiers;
	if(!nsd_modifiers(record, modifiers, error)) {
		return false;
	}

	if(slot < placement_prefix(modifiers) || slot >= modifiers.size() || modifiers[slot].prop_id != 0) {
		error = "Select a secondary basedon reference; the placement prefix is protected";
		return false;
	}

	auto changed = modifiers;
	NODModifier reference{};
	if(!placement_reference(name, reference, error)) {
		return false;
	}

	const auto terminator = std::find(reference.value, reference.value + 32, uint8_t(0));
	std::copy(reference.value, terminator + 1, changed[slot].value);
	return commit_placement_references(record, modifiers, changed, error);
}

/*
 */
bool Document::remove_placement_reference(size_t record, size_t slot, std::string &error) {
	std::vector<NODModifier> modifiers;
	if(!nsd_modifiers(record, modifiers, error)) {
		return false;
	}

	if(slot < placement_prefix(modifiers) || slot >= modifiers.size() || modifiers[slot].prop_id != 0) {
		error = "Select a secondary basedon reference; the placement prefix is protected";
		return false;
	}

	auto changed = modifiers;
	changed.erase(changed.begin() + slot);
	return commit_placement_references(record, modifiers, changed, error);
}

/*
 */
bool Document::move_placement_reference(size_t record, size_t slot, size_t before, std::string &error) {
	std::vector<NODModifier> modifiers;
	if(!nsd_modifiers(record, modifiers, error)) {
		return false;
	}

	if(before == size_t(-1)) {
		before = modifiers.size();
	}

	const auto prefix = placement_prefix(modifiers);
	if(slot < prefix || slot >= modifiers.size() || modifiers[slot].prop_id != 0 || before < prefix ||
	   before > modifiers.size()) {
		error = "Only secondary basedon references may move, and not into the protected prefix";
		return false;
	}

	auto changed		 = modifiers;
	const auto reference = changed[slot];
	changed.erase(changed.begin() + slot);
	if(before > slot) {
		--before;
	}

	changed.insert(changed.begin() + before, reference);
	return commit_placement_references(record, modifiers, changed, error);
}

/*
 */
bool Document::splice_nsd_property(size_t record, size_t offset, const std::vector<uint8_t> &property,
								   std::string &error) {
	NSDParser parser;
	if(!parser.parseBuffer(bytes_.data(), bytes_.size())) {
		error = parser.getLastError();
		return false;
	}

	const auto &scene = parser.getSceneData();
	std::vector<NODModifier> modifiers;
	if(scene.entity_array_count != records_.size() || !parser.getModifiers(scene.entities[record], modifiers)) {
		error = "The placement's counted native modifier array is not verified; structural editing is disabled";
		return false;
	}

	const auto &target = records_[record];
	const size_t count = modifiers.size();
	const size_t first = target.offset - 4;
	const size_t end   = first + count * 36;
	if(record + 1 < records_.size() && end > records_[record + 1].offset - 8) {
		error = "The modifier array overlaps the next placement's header";
		return false;
	}

	const bool inserting = !property.empty();
	if(inserting && (property.size() != 36 || count >= NOTParser::MAX_PROPERTIES || bytes_.size() > SIZE_MAX - 36)) {
		error = "The native 128-modifier count or scene size limit would be exceeded";
		return false;
	}

	if(offset == size_t(-1)) {
		offset = end;
	}

	if(offset < first || offset > end || (offset - first) % 36 || (!inserting && (offset == end || count <= 3))) {
		error = "The selected field is not a complete removable placement modifier";
		return false;
	}

	auto candidate = bytes_;
	if(inserting) {
		candidate.insert(candidate.begin() + offset, property.begin(), property.end());
	} else {
		candidate.erase(candidate.begin() + offset, candidate.begin() + offset + 36);
	}

	write_u32(candidate, uint32_t(inserting ? count + 1 : count - 1), target.offset - 8);
	return commit_nsd_placement_change(record, std::move(candidate), error);
}

/*
 */
bool Document::commit_nsd_placement_change(size_t record, std::vector<uint8_t> candidate, std::string &error) {
	NSDParser parser;
	if(!parser.parseBuffer(bytes_.data(), bytes_.size())) {
		error = parser.getLastError();
		return false;
	}

	const auto &scene	 = parser.getSceneData();
	const bool inserting = candidate.size() > bytes_.size();
	const auto shift	 = inserting ? candidate.size() - bytes_.size() : bytes_.size() - candidate.size();
	Document validation;
	if(!validation.load_bytes(candidate, Format::nsd, error) ||
	   !validation.set_template_archives(template_archives_, error)) {
		return false;
	}

	NSDParser reparsed;
	if(!reparsed.parseBuffer(candidate.data(), candidate.size())) {
		error = reparsed.getLastError();
		return false;
	}

	if(validation.records_.size() != records_.size()) {
		error = "This modifier change alters placement recognition and cannot be safely applied";
		return false;
	}

	std::vector<NODModifier> modifiers;
	if(!reparsed.getModifiers(reparsed.getSceneData().entities[record], modifiers)) {
		error = "The modified placement no longer has a verified counted modifier array";
		return false;
	}

	std::vector<NODModifier> original_modifiers;
	if(!parser.getModifiers(scene.entities[record], original_modifiers) ||
	   placement_prefix(original_modifiers) != placement_prefix(modifiers)) {
		error = "This modifier change reinterprets the protected placement prefix";
		return false;
	}

	if(inserting && !template_archives_.empty()) {
		NOTDatabase templates;
		if(!templates.loadBuffers(template_archives_, error, referenced_templates_)) {
			return false;
		}

		std::vector<NOTParser::ResolvedProperty> resolved;
		std::string issue;
		if(templates.resolveModifiers(modifiers.data(), modifiers.size(), resolved, issue) ==
		   NOTParser::ResolveStatus::too_many) {
			error = issue;
			return false;
		}
	}

	for(size_t i = 0; i < records_.size(); ++i) {
		const auto &before			 = records_[i];
		const auto &after			 = validation.records_[i];
		const auto &old_entity		 = scene.entities[i];
		const auto &new_entity		 = reparsed.getSceneData().entities[i];
		const size_t expected_offset = i <= record ? before.offset
									   : inserting ? before.offset + shift
												   : before.offset - shift;
		const size_t expected_size = i != record ? before.size : inserting ? before.size + shift : before.size - shift;
		if(after.type != before.type || after.offset != expected_offset || after.size != expected_size ||
		   old_entity.has_sector != new_entity.has_sector || old_entity.sector != new_entity.sector ||
		   std::strcmp(old_entity.region_class, new_entity.region_class) != 0 ||
		   old_entity.has_bounds != new_entity.has_bounds) {
			error =
				"This modifier change reinterprets a placement prefix or record boundary; it cannot be safely applied";
			return false;
		}
	}

	return commit_nsd_structure(std::move(validation));
}

/*
 */
bool Document::splice_not_property(size_t record, size_t offset, const std::vector<uint8_t> &property,
								   std::string &error, bool check_inheritance) {
	const auto &target	 = records_[record];
	const uint32_t count = read_u32(bytes_.data() + target.offset);
	const bool inserting = !property.empty();
	if(inserting && (property.size() != 36 || count >= NOTParser::MAX_PROPERTIES || bytes_.size() > UINT32_MAX - 36)) {
		error = "The template modifier count or archive size limit would be exceeded";
		return false;
	}

	const size_t first = target.offset + 4;
	const size_t end   = first + size_t(count) * 36;
	if(offset < first || offset > end || (offset - first) % 36 || (!inserting && offset == end)) {
		error = "The selected property is not a verified complete native modifier";
		return false;
	}

	auto candidate = bytes_;
	if(inserting) {
		candidate.insert(candidate.begin() + offset, property.begin(), property.end());
	} else {
		candidate.erase(candidate.begin() + offset, candidate.begin() + offset + 36);
	}

	write_u32(candidate, inserting ? count + 1 : count - 1, target.offset);
	for(size_t i = record + 1; i < records_.size(); ++i) {
		const size_t next = inserting ? records_[i].offset + 36 : records_[i].offset - 36;
		write_u32(candidate, static_cast<uint32_t>(next), 12 + i * 36 + 32);
	}

	return commit_not_structure(std::move(candidate), error, check_inheritance ? record : size_t(-1));
}

/*
 */
bool Document::create_derived_template(size_t parent, const std::string &name, std::string &error) {
	error.clear();
	if(format_ != Format::not_file || parent >= records_.size()) {
		error = "Select an existing NOT template as the new template's parent";
		return false;
	}

	const auto &reference = records_[parent].name;
	if(!template_name(reference, error)) {
		error = "The parent name cannot be represented as a validated native basedon reference";
		return false;
	}

	std::vector<uint8_t> payload(44, 0);
	write_u32(payload, 1);
	std::copy(reference.begin(), reference.end(), payload.begin() + 8);
	return append_template(name, payload, error);
}

/*
 */
bool Document::duplicate_template(size_t source, const std::string &name, std::string &error) {
	error.clear();
	if(format_ != Format::not_file || source >= records_.size()) {
		error = "Select a NOT template to duplicate";
		return false;
	}

	const auto &record = records_[source];
	const std::vector<uint8_t> payload(bytes_.begin() + record.offset, bytes_.begin() + record.offset + record.size);
	return append_template(name, payload, error);
}

/*
 */
bool Document::append_template(const std::string &name, const std::vector<uint8_t> &payload, std::string &error) {
	if(!template_name(name, error)) {
		return false;
	}

	if(std::any_of(records_.begin(), records_.end(),
				   [&](const Record &record) { return lower_name(record.name) == lower_name(name); })) {
		error = "A template with that name already exists in this archive (case-insensitive)";
		return false;
	}

	if(records_.size() >= NOTParser::MAX_TEMPLATES || bytes_.size() > UINT32_MAX - 36 ||
	   payload.size() > UINT32_MAX - 36 - bytes_.size()) {
		error = "The native template count or archive size limit would be exceeded";
		return false;
	}

	const size_t table_end = 12 + records_.size() * 36;
	auto candidate		   = bytes_;
	candidate.insert(candidate.begin() + table_end, 36, uint8_t(0));
	std::copy(name.begin(), name.end(), candidate.begin() + table_end);
	write_u32(candidate, static_cast<uint32_t>(candidate.size()), table_end + 32);
	for(size_t i = 0; i < records_.size(); ++i) {
		write_u32(candidate, static_cast<uint32_t>(records_[i].offset + 36), 12 + i * 36 + 32);
	}

	candidate.insert(candidate.end(), payload.begin(), payload.end());
	write_u32(candidate, static_cast<uint32_t>(records_.size() + 1), 8);
	return commit_not_structure(std::move(candidate), error);
}

/*
 */
bool Document::remove_template(size_t record, std::string &error) {
	error.clear();
	if(format_ != Format::not_file || record >= records_.size()) {
		error = "Select a NOT template to delete";
		return false;
	}

	if(records_.size() == 1) {
		error = "The final template cannot be deleted: empty native archives are not supported";
		return false;
	}

	const auto &target = records_[record];
	for(size_t i = 0; i < records_.size(); ++i) {
		if(i == record) {
			continue;
		}

		for(const auto &field : records_[i].fields) {
			if(field.property_id != 0) {
				continue;
			}

			if(!ascii_text(bytes_, field.offset, 32)) {
				error = "Cannot verify references: " + records_[i].name + " has an invalid basedon payload";
				return false;
			}

			if(lower_name(value(field)) == lower_name(target.name)) {
				error = "Cannot delete " + target.name + ": template " + records_[i].name + " references it";
				return false;
			}
		}
	}

	auto candidate = bytes_;
	candidate.erase(candidate.begin() + target.offset, candidate.begin() + target.offset + target.size);
	candidate.erase(candidate.begin() + 12 + record * 36, candidate.begin() + 12 + (record + 1) * 36);
	write_u32(candidate, static_cast<uint32_t>(records_.size() - 1), 8);
	for(size_t i = 0; i < records_.size(); ++i) {
		if(i == record) {
			continue;
		}

		const size_t next_index = i < record ? i : i - 1;
		const size_t offset		= records_[i].offset - 36 - (i > record ? target.size : 0);
		write_u32(candidate, static_cast<uint32_t>(offset), 12 + next_index * 36 + 32);
	}

	return commit_not_structure(std::move(candidate), error);
}

/*
 */
bool Document::rename_template(size_t record, const std::string &name, std::string &error) {
	error.clear();
	if(format_ != Format::not_file || record >= records_.size()) {
		error = "Select a NOT template to rename";
		return false;
	}

	if(!template_name(name, error)) {
		return false;
	}

	const auto &previous = records_[record].name;
	if(previous == name) {
		return true;
	}

	const auto old_key = lower_name(previous);
	const auto new_key = lower_name(name);
	for(size_t i = 0; i < records_.size(); ++i) {
		if(i != record && lower_name(records_[i].name) == new_key) {
			error = "A template with that name already exists in this archive (case-insensitive)";
			return false;
		}
	}

	auto candidate = bytes_;
	write_text(candidate, 12 + record * 36, name);
	for(const auto &entry : records_) {
		for(const auto &field : entry.fields) {
			if(field.property_id != 0) {
				continue;
			}

			if(!ascii_text(bytes_, field.offset, 32)) {
				error = "Cannot verify references: " + entry.name + " has an invalid basedon payload";
				return false;
			}

			const auto reference = lower_name(value(field));
			if(reference == old_key) {
				write_text(candidate, field.offset, name);
			} else if(reference == new_key) {
				error = "Cannot rename: " + entry.name +
						" already references the proposed new name; renaming would retarget it";
				return false;
			}
		}
	}

	return commit_not_structure(std::move(candidate), error);
}

/*
 */
bool Document::reparent_template(size_t record, size_t reference_field, size_t parent, std::string &error) {
	error.clear();
	if(format_ != Format::not_file || record >= records_.size() || parent >= records_.size()) {
		error = "Select a NOT template and a parent from the open archive";
		return false;
	}

	const auto &name = records_[parent].name;
	if(!template_name(name, error)) {
		error = "The parent name cannot be represented as a validated native basedon reference";
		return false;
	}

	const auto &target = records_[record];
	if(reference_field == size_t(-1)) {
		if(std::any_of(target.fields.begin(), target.fields.end(),
					   [](const Field &field) { return field.property_id == 0; })) {
			error = "Select the particular basedon reference to change; other references must retain their order";
			return false;
		}

		std::vector<uint8_t> property(36, 0);
		write_text(property, 4, name);
		return splice_not_property(record, target.offset + 4, property, error, true);
	}

	if(reference_field >= target.fields.size() || target.fields[reference_field].property_id != 0) {
		error = "Select an existing basedon field, not a local property, name or editor metadata";
		return false;
	}

	const auto &field		   = target.fields[reference_field];
	const bool valid		   = ascii_text(bytes_, field.offset, 32);
	const std::string previous = valid ? value(field) : "";
	if(valid && previous == name) {
		return true;
	}

	auto candidate = bytes_;
	write_text(candidate, field.offset, name);
	const bool topology_changed = !valid || lower_name(previous) != lower_name(name);
	return commit_not_structure(std::move(candidate), error, topology_changed ? record : size_t(-1));
}

/*
 */
bool Document::not_modifier_offset(size_t record, size_t field, size_t &offset, std::string &error) const {
	if(format_ != Format::not_file || record >= records_.size() || field >= records_[record].fields.size() ||
	   !records_[record].fields[field].native_modifier) {
		error = "Select a complete native modifier in a NOT template, not its name or editor metadata";
		return false;
	}

	offset = records_[record].fields[field].offset - 4;
	return true;
}

/*
 */
bool Document::insert_template_reference(size_t record, size_t before_field, size_t parent, std::string &error) {
	error.clear();
	if(format_ != Format::not_file || record >= records_.size() || parent >= records_.size()) {
		error = "Select a NOT template and a parent from the open archive";
		return false;
	}

	const auto &name = records_[parent].name;
	if(!template_name(name, error)) {
		error = "The parent name cannot be represented as a validated native basedon reference";
		return false;
	}

	const auto &target = records_[record];
	size_t offset	   = target.offset + 4 + size_t(read_u32(bytes_.data() + target.offset)) * 36;
	if(before_field != size_t(-1) && !not_modifier_offset(record, before_field, offset, error)) {
		return false;
	}

	std::vector<uint8_t> property(36, 0);
	write_text(property, 4, name);
	return splice_not_property(record, offset, property, error, true);
}

/*
 */
bool Document::remove_template_reference(size_t record, size_t field, std::string &error) {
	error.clear();
	size_t offset;
	if(!not_modifier_offset(record, field, offset, error)) {
		return false;
	}

	if(records_[record].fields[field].property_id != 0) {
		error = "Select a basedon reference, not a local property";
		return false;
	}

	return splice_not_property(record, offset, {}, error);
}

/*
 */
bool Document::move_template_reference(size_t record, size_t field, size_t before_field, std::string &error) {
	error.clear();
	size_t source;
	if(!not_modifier_offset(record, field, source, error)) {
		return false;
	}

	if(records_[record].fields[field].property_id != 0) {
		error = "Select a basedon reference, not a local property";
		return false;
	}

	const auto &target = records_[record];
	size_t destination = target.offset + 4 + size_t(read_u32(bytes_.data() + target.offset)) * 36;
	if(before_field != size_t(-1) && !not_modifier_offset(record, before_field, destination, error)) {
		return false;
	}

	if(destination == source || destination == source + 36) {
		return true;
	}

	const std::vector<uint8_t> property(bytes_.begin() + source, bytes_.begin() + source + 36);
	auto candidate = bytes_;
	candidate.erase(candidate.begin() + source, candidate.begin() + source + 36);
	if(destination > source) {
		destination -= 36;
	}

	candidate.insert(candidate.begin() + destination, property.begin(), property.end());
	return commit_not_structure(std::move(candidate), error);
}

/*
 */
bool Document::validate_local_inheritance(size_t root, std::string &error) const {
	std::map<std::string, size_t> names;
	for(size_t i = 0; i < records_.size(); ++i) {
		names.emplace(lower_name(records_[i].name), i);
	}

	std::vector<std::vector<size_t>> dependents(records_.size());
	for(size_t i = 0; i < records_.size(); ++i) {
		for(const auto &field : records_[i].fields) {
			if(field.property_id != 0 || !ascii_text(bytes_, field.offset, 32)) {
				continue;
			}

			const auto dependency = names.find(lower_name(value(field)));
			if(dependency != names.end()) {
				dependents[dependency->second].push_back(i);
			}
		}
	}

	std::vector<unsigned char> state(records_.size(), 0);
	std::vector<std::set<uint32_t>> properties(records_.size());
	std::function<bool(size_t)> visit = [&](size_t record) {
		if(state[record] == 1) {
			error = "The inheritance change would reach a local cycle at template " + records_[record].name;
			return false;
		}

		if(state[record] == 2) {
			return true;
		}

		state[record]		 = 1;
		const size_t start	 = records_[record].offset;
		const uint32_t count = read_u32(bytes_.data() + start);
		for(size_t i = 0; i < count; ++i) {
			const uint32_t id = read_u32(bytes_.data() + start + 4 + i * 36);
			if(id != 0) {
				properties[record].insert(id);
			}
		}

		for(const auto &field : records_[record].fields) {
			if(field.property_id != 0) {
				continue;
			}

			if(!ascii_text(bytes_, field.offset, 32)) {
				error = "Cannot verify inheritance: " + records_[record].name + " has an invalid basedon payload";
				return false;
			}

			const auto dependency = names.find(lower_name(value(field)));
			if(dependency != names.end()) {
				if(!visit(dependency->second)) {
					return false;
				}

				const auto &inherited = properties[dependency->second];
				properties[record].insert(inherited.begin(), inherited.end());
				if(properties[record].size() > NOTParser::MAX_PROPERTIES) {
					error = "The inheritance change would exceed 128 expanded property IDs at template " +
							records_[record].name;
					return false;
				}
			}
		}

		state[record] = 2;
		return true;
	};
	std::vector<size_t> affected{root};
	std::vector<bool> seen(records_.size(), false);
	seen[root] = true;
	for(size_t i = 0; i < affected.size(); ++i) {
		const size_t record = affected[i];
		if(!visit(record)) {
			return false;
		}

		for(const auto dependent : dependents[record]) {
			if(seen[dependent]) {
				continue;
			}

			seen[dependent] = true;
			affected.push_back(dependent);
		}
	}

	return true;
}

/*
 */
bool Document::commit_not_structure(std::vector<uint8_t> candidate, std::string &error, size_t inheritance_root) {
	if(candidate == bytes_) {
		return true;
	}

	Document validation;
	if(!validation.load_bytes(candidate, Format::not_file, error)) {
		return false;
	}

	if(inheritance_root != size_t(-1) && !validation.validate_local_inheritance(inheritance_root, error)) {
		return false;
	}

	Change change{0, bytes_, std::move(candidate), true};
	history_.resize(cursor_);
	history_.push_back(std::move(change));
	bytes_	  = std::move(validation.bytes_);
	records_  = std::move(validation.records_);
	warnings_ = std::move(validation.warnings_);
	cursor_	  = history_.size();
	return true;
}

/*
 */
bool Document::apply_history(const Change &change, bool forward, std::string *error) {
	const auto &replacement = forward ? change.after : change.before;
	if(change.structural) {
		Document validation;
		std::string issue;
		if(!validation.load_bytes(replacement, format_, issue) ||
		   (format_ == Format::nsd &&
			!validation.set_template_archives(forward ? change.archives_after : change.archives_before, issue))) {
			if(error) {
				*error = issue;
			} else {
				std::fprintf(stderr, "Cannot restore native editor history: %s\n", issue.c_str());
			}

			return false;
		}

		bytes_				  = std::move(validation.bytes_);
		records_			  = std::move(validation.records_);
		warnings_			  = std::move(validation.warnings_);
		referenced_templates_ = std::move(validation.referenced_templates_);
		template_archives_	  = std::move(validation.template_archives_);
	} else {
		std::copy(replacement.begin(), replacement.end(), bytes_.begin() + change.offset);
	}

	return true;
}

/*
 */
bool Document::undo(std::string *error) {
	if(error) {
		error->clear();
	}

	if(!can_undo()) {
		return false;
	}

	if(!apply_history(history_[cursor_ - 1], false, error)) {
		return false;
	}

	--cursor_;
	return true;
}

/*
 */
bool Document::redo(std::string *error) {
	if(error) {
		error->clear();
	}

	if(!can_redo()) {
		return false;
	}

	if(!apply_history(history_[cursor_], true, error)) {
		return false;
	}

	++cursor_;
	return true;
}

/*
 */
bool Document::save(const std::filesystem::path &path, bool overwrite, std::string &error) {
	error.clear();
	Document validation;
	if(!validation.load_bytes(bytes_, format_, error)) {
		return false;
	}

	if(!same_layout(validation)) {
		error = "Decoded layout changed; refusing to save";
		return false;
	}

	std::error_code ec;
	const auto absolute = std::filesystem::absolute(path, ec);
	if(ec) {
		error = "Cannot resolve save path: " + ec.message();
		return false;
	}

	if(!workspace_.empty()) {
		if(!workspace_contains(workspace_, absolute, error)) {
			if(error.empty()) {
				error = "Native edits may only be saved inside the authoring workspace";
			}

			return false;
		}

		std::filesystem::create_directories(absolute.parent_path(), ec);
		if(ec) {
			error = "Cannot create workspace destination: " + ec.message();
			return false;
		}

		if(!workspace_contains(workspace_, absolute, error)) {
			if(error.empty()) {
				error = "The destination resolves outside the authoring workspace";
			}

			return false;
		}
	}

	const bool exists = std::filesystem::exists(absolute, ec);
	if(ec) {
		error = "Cannot inspect save path: " + ec.message();
		return false;
	}

	const bool original = !path_.empty() && (path_ == absolute || (exists && same_path(path_, absolute, error)));
	if(!error.empty()) {
		return false;
	}

	if(original) {
		std::vector<uint8_t> current;
		if(!NodTools::read_file(path_, current, error)) {
			return false;
		}

		if(current != saved_) {
			error = "File changed on disk. Reload it or save to another path.";
			return false;
		}
	}

	if(exists && !original && !overwrite) {
		error = "Destination exists; explicit overwrite confirmation is required";
		return false;
	}

	if(!atomic_write(absolute, bytes_, exists, error)) {
		return false;
	}

	path_  = absolute;
	saved_ = bytes_;
	return true;
}

/*
 */
bool Document::same_layout(const Document &other) const {
	if(records_.size() != other.records_.size() || bytes_.size() != other.bytes_.size()) {
		return false;
	}

	for(size_t i = 0; i < records_.size(); ++i) {
		const auto &a = records_[i];
		const auto &b = other.records_[i];
		if(a.offset != b.offset || a.size != b.size || a.fields.size() != b.fields.size()) {
			return false;
		}

		for(size_t j = 0; j < a.fields.size(); ++j) {
			const auto &x = a.fields[j];
			const auto &y = b.fields[j];
			if(x.offset != y.offset || x.size != y.size || x.kind != y.kind || x.editable != y.editable ||
			   x.property_id != y.property_id || x.native_modifier != y.native_modifier) {
				return false;
			}
		}
	}

	return true;
}

} // namespace nod::edit
