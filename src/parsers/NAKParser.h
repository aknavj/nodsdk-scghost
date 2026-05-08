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

#ifndef __NAKPARSER_H__
#define __NAKPARSER_H__

#include "AssetParser.h"

/*
 */
class NAKParser : public IAssetParser {

	public:
		static const uint32_t MAX_KEYFRAMES = 128;

		struct CameraKeyframe {
			float time; // frame time
			float fov; // field of view
			float near_clip; // near clip plane
			float far_clip; // far clip plane
		};

		struct CameraTrackData {
			CameraKeyframe keyframes[MAX_KEYFRAMES];
			uint32_t keyframe_count;
			size_t file_size;
		};

		NAKParser() { memset(&m, 0, sizeof(m)); }
		~NAKParser() override = default;

		int parse(const char *filePath) override;
		int serialize(const char *filePath) const override;
		const char *getFileType() const override { return "NAK"; }
		const CameraTrackData &getCameraTrackData() const { return m; }

	private:
		CameraTrackData m;
};

#endif // __NAKPARSER_H__
