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

#pragma once

#include <string>
#include <cstddef>

/*
 */
namespace viewer::core {

    [[nodiscard]] std::string toLower(std::string_view s);

    // case-insensitive strcmp; matches behaviour of decompiled
    // materialManager::strncmpAlphaB.
    [[nodiscard]] int striCmp(const char *a, const char *b) noexcept;

    // strip a trailing space-suffix and a trailing ".dds"; mirrors engine
    // MaterialManager::cleanTextureName.
    [[nodiscard]] std::string cleanTexName(const char *raw);

    // lower-cased file extension WITHOUT the leading dot ("nod", "dds", ...).
    [[nodiscard]] std::string getExtension(const std::string &path);

    // file name portion (after last '/').
    [[nodiscard]] std::string getFilename(const std::string &path);

    // "X.X MB" / "X.X KB" / "Y B".
    [[nodiscard]] std::string formatSize(std::size_t bytes);

} // namespace viewer::core
