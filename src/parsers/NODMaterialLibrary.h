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

#ifndef __NODMATERIALLIBRARY_H__
#define __NODMATERIALLIBRARY_H__

#include "NSAParser.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

/*
 * The materials and textures of an extracted game build, and how a model's
 * material name becomes a material:
 *
 *   1. the name is defined in an archive (.nsa): the first definition, with
 *      the archives in the order of Materials/shaderlist.txt, then the
 *      archives that list does not name, in file name order;
 *   2. a file <name>.nas exists under Materials: its one material;
 *   3. a texture file of the same name exists: a default material, drawn
 *      solid and lit with that texture;
 *   4. none of these.
 *
 * Textures are found by name without case: 3D/Materials first, then the
 * folders of MaterialDirs.txt in their order, each with its subfolders.
 * Every name is matched without case.
 */
class NODMaterialLibrary {

	public:
		enum Step : uint32_t { STEP_ARCHIVE = 1, STEP_NAS = 2, STEP_TEXTURE = 3, STEP_NONE = 4 };

		struct Archive {
			std::string file; // name on disk, under Materials
			bool listed; // named by shaderlist.txt
			std::unique_ptr<NSAParser> parser;
		};

		struct Definition {
			uint32_t archive; // index into archives()
			uint32_t material; // index into that archive's materials
		};

		struct Found {
			Step step = STEP_NONE;
			const NSAParser::Material *material = nullptr; // steps 1 and 2
			std::string source; // the archive or .nas file (step 1, 2) or the texture file (step 3)
			uint32_t line = 0; // line of the definition (steps 1 and 2)
		};

		// Read shaderlist.txt, MaterialDirs.txt, every .nsa and .nas, and index
		// the textures. False with `error` set when the build has no Materials.
		bool load(const std::string &buildRoot, std::string &error);

		Found resolve(const std::string &materialName) const;
		// The texture file for a texture name (folder and extension dropped,
		// text after the first blank dropped); empty when there is none.
		std::string findTexture(const std::string &textureName) const;

		const std::vector<Archive> &archives() const { return archives_; }
		// archives named by shaderlist.txt with no file
		const std::vector<std::string> &missingArchives() const { return missingArchives_; }
		// folders named by MaterialDirs.txt that do not exist
		const std::vector<std::string> &missingFolders() const { return missingFolders_; }
		// every definition of a name (lower case), in lookup order
		const std::map<std::string, std::vector<Definition>> &definitions() const { return definitions_; }
		// texture names (lower case) found in more than one place, with every place
		const std::map<std::string, std::vector<std::string>> &duplicateTextures() const { return duplicateTextures_; }
		size_t textureCount() const { return textures_.size(); }

		static std::string lower(std::string s);

	private:
		std::string root_;
		std::vector<Archive> archives_;
		std::vector<std::unique_ptr<NSAParser>> nas_;
		std::map<std::string, std::pair<const NSAParser::Material *, std::string>> nasByName_;
		std::vector<std::string> missingArchives_, missingFolders_;
		std::map<std::string, std::vector<Definition>> definitions_;
		std::map<std::string, std::string> textures_;
		std::map<std::string, std::vector<std::string>> duplicateTextures_;
		void indexTextures(const std::string &folder);
};

#endif // __NODMATERIALLIBRARY_H__
