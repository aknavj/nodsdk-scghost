/*
===========================================================================
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

// NODExport: models, clips and materials as data a renderer can load.
//
//   NODExport nod <in.nod> <out.json> <out.bin>
//   NODExport nad <in.nad> <out.json>
//   NODExport materials <build> <names.txt> <out.json>
//
// Exit 0 when exported; 1 when the input is refused (out.json then holds
// {"format", "error"}); 2 on a usage or output error.

#include "export.h"

namespace {

	int usage() {
		std::fprintf(stderr, "usage: NODExport nod <in.nod> <out.json> <out.bin>\n"
							 "       NODExport nad <in.nad> <out.json>\n"
							 "       NODExport materials <build> <names.txt> <out.json>\n");
		return 2;
	}

} // namespace

int main(int argc, char **argv) {
	if(argc < 2) return usage();
	const std::string mode = argv[1];
	std::string json;
	const char *out = nullptr;
	bool ok			= false;
	if(mode == "nod" && argc == 5) {
		out = argv[3];
		ok	= nodexport::exportNod(argv[2], argv[4], json);
	} else if(mode == "nad" && argc == 4) {
		out = argv[3];
		ok	= nodexport::exportNad(argv[2], json);
	} else if(mode == "materials" && argc == 5) {
		out = argv[4];
		ok	= nodexport::exportMaterials(argv[2], argv[3], json);
	} else {
		return usage();
	}
	if(!survey::writeFile(out, json)) {
		std::fprintf(stderr, "NODExport: cannot write %s\n", out);
		return 2;
	}
	return ok ? 0 : 1;
}
