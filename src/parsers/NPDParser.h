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

#ifndef __NPDPARSER_H__
#define __NPDPARSER_H__

#include "AssetParser.h"
#include <glm/glm.hpp>

/*
 */
class NPDParser : public IAssetParser {

	public:
		/*
		 */
		struct PlacementRecord {
			uint16_t entity_id;
			glm::vec3 position;
			uint16_t flags;
			uint32_t color;
			uint8_t extra[6];
		};

		/*
		 */
		struct PlacementData {
			uint8_t header[8];
			PlacementRecord *records;
			uint32_t record_count;
			uint8_t *trailer;
			size_t trailer_size;
			size_t file_size;
		};

		NPDParser() { 
			memset(&pd, 0, sizeof(pd)); 
		}

		~NPDParser() override {
			free(pd.records);
			free(pd.trailer);
		}

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NPD"; }
		const PlacementData &getPlacementData() const { return pd; }

	private:
		PlacementData pd;
};

#endif // __NPDPARSER_H__
