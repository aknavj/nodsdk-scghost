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

#ifndef __ASSETPARSERFACTORY_H__
#define __ASSETPARSERFACTORY_H__

#include "AssetParser.h"
#include "DDSParser.h"
#include "NADParser.h"
#include "NODParser.h"
#include "NMSParser.h"
#include "TerrainParser.h"
#include "NILParser.h"
#include "NLFXParser.h"
#include "NPDParser.h"
#include "NSAParser.h"
#include "NSDParser.h"
#include "XPRParser.h"
#include "NLTParser.h"
#include "TGAParser.h"
#include "NOCParser.h"
#include "NLSParser.h"
#include "NOTParser.h"
#include "NFXParser.h"
#include "NUIParser.h"
#include "NAKParser.h"
#include "NCEParser.h"
#include "NCSParser.h"
#include "NCOParser.h"
#include "NMBParser.h"
#include "NNBParser.h"
#include "XWBParser.h"
#include "XSBParser.h"


/*
 */
class AssetParserFactory {

	public:

		/*
		 */
		static IAssetParser *createParser(const char *filePath) {
			char ext[32];
			FileUtils::getFileExtension(filePath, ext, sizeof(ext));

			if(strcmp(ext, "dds") == 0) return new DDSParser();
			if(strcmp(ext, "nad") == 0) return new NADParser();
			if(strcmp(ext, "nod") == 0) return new NODParser();
			if(strcmp(ext, "nms") == 0) return new NMSParser();
			if(strcmp(ext, "nrt") == 0 || strcmp(ext, "nut") == 0) return new TerrainParser();
			if(strcmp(ext, "nil") == 0) return new NILParser();
			if(strcmp(ext, "nlfx") == 0) return new NLFXParser();
			if(strcmp(ext, "npd") == 0) return new NPDParser();
			if(strcmp(ext, "nsa") == 0) return new NSAParser();
			if(strcmp(ext, "nsd") == 0) return new NSDParser();
			if(strcmp(ext, "xpr") == 0) return new XPRParser();
			if(strcmp(ext, "nlt") == 0) return new NLTParser();
			if(strcmp(ext, "tga") == 0) return new TGAParser();
			if(strcmp(ext, "noc") == 0) return new NOCParser();
			if(strcmp(ext, "nls") == 0 || strcmp(ext, "nlu") == 0) return new NLSParser();
			if(strcmp(ext, "not") == 0) return new NOTParser();
			if(strcmp(ext, "nfx") == 0) return new NFXParser();
			if(strcmp(ext, "nui") == 0) return new NUIParser();
			if(strcmp(ext, "nak") == 0) return new NAKParser();
			if(strcmp(ext, "nce") == 0) return new NCEParser();
			if(strcmp(ext, "ncs") == 0) return new NCSParser();
			if(strcmp(ext, "nco") == 0) return new NCOParser();
			if(strcmp(ext, "nmb") == 0) return new NMBParser();
			if(strcmp(ext, "nnb") == 0) return new NNBParser();
			if(strcmp(ext, "xwb") == 0) return new XWBParser();
			if(strcmp(ext, "xsb") == 0) return new XSBParser();
			return nullptr;
		}

		/*
		 */
		static IAssetParser *parseFile(const char *filePath) {
			IAssetParser *p = createParser(filePath);
			if(p && p->parse(filePath)) return p;
			delete p;
			return nullptr;
		}
};

#endif // __ASSETPARSERFACTORY_H__
