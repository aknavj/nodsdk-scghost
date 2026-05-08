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

#ifndef __NUIPARSER_H__
#define __NUIPARSER_H__

#include "AssetParser.h"

/*
 */
class NUIParser : public IAssetParser {

	public:
		static const uint32_t MAX_ENTRIES  = 64;
		static const uint32_t MAX_QUADS	   = 32;
		static const uint32_t MAX_ELEMENTS = 32;

		// raw line entry (kept for backward compatibility)
		struct UIEntry {
			char line[256]; // full line text
			char type[32]; // "header", "quad", "element", "property"
		};

		/*
		 */
		struct UIQuadData {
			char material[256];
			int pos_x, pos_y;
			int width, height;
		};

		/*
		 */
		enum ElementType {
			ELEM_UNKNOWN = 0,
			ELEM_BUTTON,
			ELEM_TEXTBOX,
			ELEM_EDIT,
			ELEM_STATIC,
			ELEM_SCROLLBAR,
			ELEM_CHECKBOX,
			ELEM_COMBOBOX,
			ELEM_LISTBOX,
			ELEM_SLIDER
		};

		/*
		 */
		struct UIElementData {
			ElementType type;
			char name[128];
			int pos_x, pos_y;
			int width, height;
			uint8_t color_r, color_g, color_b, color_a;
			char font[64];
			uint32_t flags;
		};

		/*
		 */
		struct UILayoutData {
			char name[64]; // dialog name
			int pos_x, pos_y; // dialog position
			int is_dialog; // dialog=1 flag
			char music[128]; // optional music reference

			UIEntry entries[MAX_ENTRIES];
			uint32_t entry_count;

			UIQuadData quads[MAX_QUADS];
			uint32_t quad_count;

			UIElementData elements[MAX_ELEMENTS];
			uint32_t element_count;

			char *raw_text;
			size_t raw_text_len;
			size_t file_size;
		};

		NUIParser() { memset(&m, 0, sizeof(m)); }
		~NUIParser() override;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NUI"; }
		const UILayoutData &getUILayoutData() const { return m; }

		static ElementType parse_element_type(const char *typeStr);
		static const char *elementTypeToString(ElementType type);

	private:
		UILayoutData m;

		int parse_quad_line(const char *line, UIQuadData &quad);
		int parse_element_line(const char *line, UIElementData &elem);
};

#endif // __NUIPARSER_H__
