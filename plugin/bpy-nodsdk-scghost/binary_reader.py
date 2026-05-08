# ===========================================================================
# Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
# Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
# Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors
#
# StaRev - Starcraft Ghost reversed
# This file is part of the StaRev source code.
#
# StaRev is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
# ===========================================================================

import struct

class BinaryReader:
    
    def __init__(self, data: bytes):
        self._d = data
        self._o = 0

    def tell(self) -> int:
        return self._o

    def seek(self, pos: int):
        self._o = pos

    def skip(self, n: int):
        self._o += n

    def remaining(self) -> int:
        return max(0, len(self._d) - self._o)

    def bytes_n(self, n: int) -> bytes:
        v = self._d[self._o:self._o + n]
        self._o += n
        return v

    def u8(self) -> int:
        v = self._d[self._o]
        self._o += 1
        return v

    def u16(self) -> int:
        (v,) = struct.unpack_from('<H', self._d, self._o)
        self._o += 2
        return v

    def u32(self) -> int:
        (v,) = struct.unpack_from('<I', self._d, self._o)
        self._o += 4
        return v

    def f32(self) -> float:
        (v,) = struct.unpack_from('<f', self._d, self._o)
        self._o += 4
        return v

    def vec3(self):
        x = self.f32()
        y = self.f32()
        z = self.f32()
        return (x, y, z)

    def str32(self) -> str:
        b = self.bytes_n(32)
        return b.split(b'\x00')[0].decode('latin-1', errors='replace').rstrip()
