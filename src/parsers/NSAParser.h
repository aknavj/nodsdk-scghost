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

#ifndef __NSAPARSER_H__
#define __NSAPARSER_H__

#include "AssetParser.h"

/*
 */
class NSAParser : public IAssetParser {

	public:
		static const uint32_t MAX_PROPS = 32;

		/*
		 */
		struct MaterialProperty {
			char key[128];
			char value[256];
		};

		/*
		 */
		struct Material {
			char name[128];
			MaterialProperty props[MAX_PROPS];
			uint32_t prop_count;
		};

		/*
		 */
		struct MaterialsData {
			Material *materials;
			uint32_t material_count;
		};

		NSAParser() { 
			memset(&md, 0, sizeof(md)); 
		}

		~NSAParser() override { 
			free(md.materials); 
		}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NSA"; }
		const MaterialsData &getMaterialsData() const { return md; }

	private:
		MaterialsData md;
};

#endif // __NSAPARSER_H__
