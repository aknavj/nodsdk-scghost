/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: tool_help.h
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

#ifndef NOD_TOOLS_HELP_H
#define NOD_TOOLS_HELP_H

#include <stdio.h>
#include "../../../res/version.h"

#define NOD_TOOLS_VERSION NODSDK_VERSION_STRING
#define NOD_TOOLS_REPOSITORY_URL "http://github.com/aknavj/nodsdk-scghost"
#define NOD_TOOLS_WINDOW_TITLE(tool) \
	"OpenNOD " tool " | " NOD_TOOLS_REPOSITORY_URL " | version: " NOD_TOOLS_VERSION

static inline void nod_tools_print_help(FILE *output, const char *tool, const char *arguments, const char *examples) {
	fprintf(output,
			"OpenNOD %s | " NOD_TOOLS_REPOSITORY_URL "\n"
			"Built date: %s | version: %s\n\n"
			"Help args:\n\n%s\n"
			"Example usage:\n%s",
			tool, __DATE__, NOD_TOOLS_VERSION, arguments, examples);
}

#endif
