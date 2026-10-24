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

#include "app/ViewerApp.h"
#include "../tools/common/tool_help.h"
#include <cstring>

/*
 */
int main(int argc, char **argv) {
	if(argc < 2 || (argc == 2 && std::strcmp(argv[1], "--help") == 0)) {
		nod_tools_print_help(argc < 2 ? stderr : stdout, "NODAssetViewer",
			"--help : Show help without opening a window.\n"
			"asset_directory : Root of the extracted game asset tree to browse.\n",
			"NODAssetViewer assets\n");
		return argc < 2 ? 1 : 0;
	}
	return viewer::app::ViewerApp{}.run(argc, argv);
}
