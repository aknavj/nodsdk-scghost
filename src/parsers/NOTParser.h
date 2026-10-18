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

#ifndef __NOTPARSER_H__
#define __NOTPARSER_H__

#include "AssetParser.h"

/*
 */
class NOTParser : public IAssetParser {

	public:
		static const uint32_t MAGIC = 0x10544F4E; // "NOT\x10" LE

		static const uint32_t MAX_TEMPLATES	  = 2048;
		static const uint32_t MAX_PROPERTIES  = 128;
		static const uint32_t PROP_VALUE_SIZE = 32;
		static const uint32_t PROP_BOOL_ID	  = 1;

		/*
		 */
		struct Property {
			uint32_t prop_id;
			uint8_t value[PROP_VALUE_SIZE]; // raw value (32 bytes, or less for int)
			uint32_t value_size; // actual value bytes: 32 (standard), 1 (int)
			int is_comment; // true if this is a developer comment string
			size_t value_offset; // absolute source offset of the value payload
		};

		/*
		 */
		struct Template {
			char name[33]; // from TOC, null-terminated
			char parent[33]; // parent template name
			uint32_t num_properties;
			uint32_t flags; // 47 = root/engine-class, 0 = child
			uint32_t data_offset; // absolute file offset
			uint32_t data_size; // total bytes for this template's data

			Property *properties; // parsed properties, or nullptr
			uint32_t property_count; // actually parsed count
			bool properties_complete; // every declared property was decoded at full width

			// comment string (if any), heap-allocated
			char *comment;
			uint32_t comment_len;
		};

		/*
		 */
		struct TemplateDB {
			uint32_t version;
			uint32_t num_templates;
			Template *templates;

			uint8_t *raw_data;
			size_t raw_data_size;
			size_t file_size;
		};

		NOTParser() { memset(&m, 0, sizeof(m)); }
		~NOTParser() override;

		int parse(const char *filePath) override;
		int parseBuffer(const uint8_t *data, size_t size);
		const char *getLastError() const { return last_error; }
		int serialize(const char *filePath) const override;
		const char *getFileType() const override { return "NOT"; }

		const TemplateDB &getTemplateDB() const { return m; }
		const Template *findTemplate(const char *name) const;

		// count how many templates inherit from a given parent
		uint32_t countChildren(const char *parentName) const;

	private:
		int parse_toc(const uint8_t *data, size_t size);
		int parse_template_data(uint32_t index, const uint8_t *data, size_t size);
		int parse_properties(Template &tmpl, const uint8_t *propData, size_t propDataSize);

		TemplateDB m;
		const char *last_error = "";
		void clear();
		int reject(const char *message);
};

#endif // __NOTPARSER_H__
