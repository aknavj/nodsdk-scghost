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

"""
OpenNod SDK-scghost Blender import plugin.

Importers:
  File > Import > NOD Model (.nod)        — single rigid/skinned mesh
  File > Import > NMB Bundle (.nmb)       — multi-body compound mesh
  File > Import > NIL Level (.nil)        — indoor level geometry (sectors)
  File > Import > NAD Animation (.nad)    — animation clip (requires active Armature)

Install: zip the blender/ folder, then Blender > Preferences > Add-ons > Install.
"""

bl_info = {
    'name':        'OpenNod SDK-scghost Importers',
    'author':      'aknavj',
    'version':     (1, 0, 0),
    'blender':     (4, 0, 0),
    'location':    'File > Import',
    'description': 'Import NOD / NMB / NIL / NAD formats from OpenNod SDK-scghost',
    'category':    'Import-Export',
}

from . import import_nad, import_nil, import_nmb, import_nod

def register():
    import_nod.register()
    import_nmb.register()
    import_nil.register()
    import_nad.register()

def unregister():
    import_nod.unregister()
    import_nmb.unregister()
    import_nil.unregister()
    import_nad.unregister()
