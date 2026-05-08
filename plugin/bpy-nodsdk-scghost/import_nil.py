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
NIL importer — indoor level geometry.

Sector vertex layout (36 bytes per vertex, little-endian):
  offset  0  pos   (3 × float  = 12 B)
  offset 12  norm  (3 × float  = 12 B)
  offset 24  uv    (2 × float  =  8 B)
  offset 32  vcol  (4 × uint8  =  4 B)   — RGBA, present when SurfaceGroup.vert_type == 2

Index buffer per sector: flat uint16 list.
SurfaceGroup references a slice of that buffer via start_idx / num_strip_indices /
num_list_indices; strips use standard D3D winding (odd-flip).

Each sector is imported as one Blender mesh object.  All sector objects are
parented to an Empty named after the file.
"""

import os
import struct

import bpy
from bpy.props import BoolProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ImportHelper

from .binary_reader import BinaryReader
from .import_nod import _pos, _strip_to_tris, _uv

def _parse_sector(r: BinaryReader, file_size: int):
    if r.remaining() < 17:
        return None

    flags      = r.u32()
    center     = r.vec3()
    num_portals = r.u8()
    portal_data = r.bytes_n(num_portals * 16)
    has_geo    = r.u8()
    vert_type  = 0

    if has_geo:
        vert_type = r.u8()

    num_verts = r.u16()
    r.skip(2)

    vertex_data = b''
    tbn_data    = b''
    if num_verts > 0:
        vsz = num_verts * 36
        vertex_data = r.bytes_n(vsz)
        if vert_type == 1:
            tbn_data = r.bytes_n(vsz)

    lm_flag = lm_patch_count = 0
    if has_geo >= 2:
        lm_flag        = r.u8()
        lm_patch_count = r.u32()
        r.skip(lm_patch_count * 72)

    num_groups = r.u16()
    groups = []
    for _ in range(num_groups):
        shader_idx        = r.u32()
        start_idx         = r.u32()
        r.u32()
        num_strip         = r.u16()
        num_list          = r.u16()
        r.u32()
        g_vert_type       = r.u8()
        flag              = r.u8()
        r.u8()
        bbox_min          = r.vec3()
        bbox_max          = r.vec3()
        groups.append({'shader_idx': shader_idx, 'start_idx': start_idx,
                       'num_strip': num_strip, 'num_list': num_list,
                       'vert_type': g_vert_type, 'flag': flag,
                       'bbox_min': bbox_min, 'bbox_max': bbox_max})

    indices = []
    if has_geo:
        num_indices = r.u32()
        for _ in range(num_indices):
            indices.append(r.u16())

    num_things = r.u8()
    for _ in range(num_things):
        r.skip(3 * 4 + 4 + 4 + 12 * 4)

    num_surf_things = r.u16()
    r.skip(num_surf_things * 72)

    num_env_refs = r.u16()
    r.skip(num_env_refs * 33)

    r.skip(2 + 4)

    return {
        'flags': flags, 'center': center,
        'vert_type': vert_type, 'num_verts': num_verts,
        'vertex_data': vertex_data, 'indices': indices, 'groups': groups,
        'shaders': None,
    }


def parse_nil(data: bytes) -> dict:
    r = BinaryReader(data)
    if len(data) < 96 or data[:3] != b'NIL':
        raise ValueError('Not a NIL file')

    magic   = r.u32()
    version = r.u32()
    r.skip(72)
    r.skip(12)

    num_shaders = r.u32()
    shaders = [r.str32() for _ in range(num_shaders)]

    num_env_maps = r.u16()
    for _ in range(num_env_maps):
        r.str32()

    num_sectors = r.u32()
    sectors = []
    for si in range(num_sectors):
        sec = _parse_sector(r, len(data))
        if sec is None:
            break
        sec['shaders'] = shaders
        sectors.append(sec)

    return {'magic': magic, 'version': version,
            'shaders': shaders, 'sectors': sectors}


def _sector_to_mesh(name, sector):
    vertex_data = sector['vertex_data']
    indices     = sector['indices']
    groups      = sector['groups']
    shaders     = sector['shaders'] or []

    positions  = []
    face_loops = []
    face_uvs   = []
    face_vcols = []
    face_mats  = []

    mat_order = list(dict.fromkeys(
        shaders[g['shader_idx']] for g in groups
        if g['shader_idx'] < len(shaders)
    ))
    mat_idx = {n: i for i, n in enumerate(mat_order)}

    vert_cache = {}

    def get_vert(vi):
        if vi in vert_cache:
            return vert_cache[vi]
        off = vi * 36
        if off + 32 > len(vertex_data):
            return None
        px, py, pz = struct.unpack_from('<fff', vertex_data, off)
        idx = len(positions)
        positions.append(_pos(px, py, pz))
        vert_cache[vi] = idx
        return idx

    def get_uv(vi):
        off = vi * 36 + 24
        if off + 8 > len(vertex_data):
            return (0.0, 0.0)
        u, v = struct.unpack_from('<ff', vertex_data, off)
        return _uv(u, v)

    def get_vcol(vi):
        off = vi * 36 + 32
        if off + 4 > len(vertex_data):
            return (1.0, 1.0, 1.0, 1.0)
        r, g, b, a = vertex_data[off], vertex_data[off+1], vertex_data[off+2], vertex_data[off+3]
        return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)

    def add_tris(tris, midx, use_vcol):
        for tri in tris:
            vis = [get_vert(i) for i in tri]
            if None in vis or len(set(vis)) != 3:
                continue
            face_loops.append(tuple(vis))
            face_uvs.extend(get_uv(i) for i in tri)
            if use_vcol:
                face_vcols.extend(get_vcol(i) for i in tri)
            else:
                face_vcols.extend([(1.0, 1.0, 1.0, 1.0)] * 3)
            face_mats.append(midx)

    for g in groups:
        sidx = g['shader_idx']
        shader_name = shaders[sidx] if sidx < len(shaders) else ''
        midx      = mat_idx.get(shader_name, 0)
        base      = g['start_idx']
        use_vcol  = (g['vert_type'] == 2)

        if g['num_strip'] >= 3 and base + g['num_strip'] <= len(indices):
            raw  = indices[base: base + g['num_strip']]
            add_tris(_strip_to_tris(list(raw)), midx, use_vcol)

        lbase = base + g['num_strip']
        if g['num_list'] >= 3 and lbase + g['num_list'] <= len(indices):
            raw  = indices[lbase: lbase + g['num_list']]
            tris = [(raw[k], raw[k+1], raw[k+2]) for k in range(0, len(raw)-2, 3)]
            add_tris(tris, midx, use_vcol)

    if not face_loops:
        return None

    me = bpy.data.meshes.new(name)
    me.from_pydata(positions, [], face_loops)
    me.update()

    uv_lay = me.uv_layers.new(name='UVMap')
    for li, loop in enumerate(me.loops):
        uv_lay.data[li].uv = face_uvs[li] if li < len(face_uvs) else (0.0, 0.0)

    vcol_lay = me.color_attributes.new(name='Col', type='BYTE_COLOR', domain='CORNER')
    for li, loop in enumerate(me.loops):
        vcol_lay.data[li].color = face_vcols[li] if li < len(face_vcols) else (1.0, 1.0, 1.0, 1.0)

    for mn in mat_order:
        mat = bpy.data.materials.get(mn) or bpy.data.materials.new(mn)
        me.materials.append(mat)

    for pi, poly in enumerate(me.polygons):
        if pi < len(face_mats):
            poly.material_index = face_mats[pi]
        poly.use_smooth = True

    return bpy.data.objects.new(name, me)


def import_nil_file(filepath, context):
    with open(filepath, 'rb') as f:
        data = f.read()

    parsed    = parse_nil(data)
    name_base = os.path.splitext(os.path.basename(filepath))[0]
    coll      = context.collection

    root_empty = bpy.data.objects.new(name_base, None)
    coll.objects.link(root_empty)

    for si, sector in enumerate(parsed['sectors']):
        sec_name = f'{name_base}_S{si:03d}'
        ob = _sector_to_mesh(sec_name, sector)
        if ob is None:
            continue
        coll.objects.link(ob)
        ob.parent = root_empty

    context.view_layer.objects.active = root_empty
    root_empty.select_set(True)
    return root_empty


class ImportNIL(Operator, ImportHelper):
    bl_idname  = 'import_mesh.nil'
    bl_label   = 'Import NIL Level'
    bl_options = {'UNDO'}

    filename_ext = '.nil'
    filter_glob: StringProperty(default='*.nil;*.NIL', options={'HIDDEN'})

    def execute(self, context):
        try:
            import_nil_file(self.filepath, context)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ImportNIL.bl_idname, text='NIL Level (.nil)')


def register():
    bpy.utils.register_class(ImportNIL)
    bpy.types.TOPBAR_MT_file_import.append(menu_func)


def unregister():
    bpy.utils.unregister_class(ImportNIL)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func)
