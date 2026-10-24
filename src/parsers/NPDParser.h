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
			uint32_t color; // Legacy raw packed bytes 16..19, not a color.
			uint8_t extra[6]; // Legacy raw packed bytes 20..25.
			uint8_t component() const { return uint8_t(color); }
			uint16_t associationA() const { return uint16_t(color >> 8); }
			uint16_t associationB() const { return uint16_t((color >> 24) | (uint16_t(extra[0]) << 8)); }
			uint16_t sector() const { return uint16_t(extra[1] | (uint16_t(extra[2]) << 8)); }
			uint16_t firstOutgoing() const { return uint16_t(extra[3] | (uint16_t(extra[4]) << 8)); }
			uint8_t outgoingCount() const { return extra[5]; }
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
			struct LinkRecord {
				uint16_t from, to, cost; // cost is the legacy name for native link flags.
				uint16_t flags() const { return cost; }
			};
			LinkRecord *links; // decoded view; serialize preserves the original trailer
			size_t link_count;
		};

		NPDParser() { 
			memset(&pd, 0, sizeof(pd)); 
		}

		~NPDParser() override {
			free(pd.records);
			free(pd.trailer);
			free(pd.links);
		}

		int parse(const char *filePath) override;
		// Owns decoded records/trailer; parsing again invalidates previous views.
		int parseBuffer(const uint8_t *data, size_t size);
		const char *getLastError() const { return last_error; }
		int serialize(const char *filePath) const override;

		const char *getFileType() const override { return "NPD"; }
		const PlacementData &getPlacementData() const { return pd; }

	private:
		PlacementData pd;
		const char *last_error = "";
		void clear();
		int reject(const char *message);
};

#endif // __NPDPARSER_H__
