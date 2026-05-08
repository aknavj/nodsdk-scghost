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



"""NMB importer — multi-body compound mesh bundle."""

import os
import struct

import bpy
from bpy.props import BoolProperty, EnumProperty, IntProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ImportHelper

from .binary_reader import BinaryReader
from .import_nod import (
    _base_stride, _base_type, _build_mesh, _pos, _stride, _strip_to_tris, _uv,
)


def _parse_nmb_entry(data: bytes, off: int):
    """Parse one NMB ModelEntry starting at byte offset off.
    Returns (entry_dict, next_offset) or (None, off) on failure.
    """
    if off + 66 > len(data):
        return None, off

    shader_count    = data[off + 0]
    bone_count      = data[off + 1]
    mg_count        = data[off + 2]
    marker_count    = struct.unpack_from('<H', data, off + 3)[0]
    name            = data[off + 5:off + 37].split(b'\x00')[0].decode('latin-1', errors='replace').rstrip()
    vset_count      = data[off + 37]
    flags           = struct.unpack_from('<I', data, off + 38)[0]
    bmin            = struct.unpack_from('<3f', data, off + 42)
    bmax            = struct.unpack_from('<3f', data, off + 54)

    if vset_count == 0 or vset_count > 4:
        return None, off

    pos = off + 66

    vs_types  = []
    vs_counts = []
    vs_offsets = []
    vs_sizes   = []
    total_vb   = 0

    for i in range(vset_count):
        if pos + 5 > len(data):
            return None, off
        vtype  = data[pos]
        vcount = struct.unpack_from('<I', data, pos + 1)[0]
        pos   += 5
        st     = _stride(vtype)
        vs_types.append(vtype)
        vs_counts.append(vcount)
        vs_offsets.append(pos)
        vsz = vcount * st
        vs_sizes.append(vsz)
        pos += vsz
        total_vb += vsz

    vdata_off = vs_offsets[0] if vs_offsets else pos
    vertex_data = bytearray(total_vb)
    dst = 0
    vertex_sets = []
    for i in range(vset_count):
        vt  = vs_types[i]
        vc  = vs_counts[i]
        st  = _stride(vt)
        bst = _base_stride(vt)
        src = data[vs_offsets[i]: vs_offsets[i] + vs_sizes[i]]
        vertex_data[dst: dst + len(src)] = src
        vertex_sets.append({'type': vt, 'stride': st, 'base_stride': bst,
                             'count': vc, 'data_offset': dst})
        dst += vs_sizes[i]

    if pos + shader_count * 32 > len(data):
        return None, off
    shaders = []
    for _ in range(shader_count):
        shaders.append(data[pos:pos + 32].split(b'\x00')[0].decode('latin-1', errors='replace').rstrip())
        pos += 32

    bones = []
    if pos + bone_count * 26 > len(data):
        return None, off
    for _ in range(bone_count):
        bpos   = struct.unpack_from('<3f', data, pos)
        boff   = struct.unpack_from('<3f', data, pos + 12)
        parent   = data[pos + 24]
        joint_id = data[pos + 25]
        bones.append({'pos': bpos, 'orient': None, 'offset': boff,
                      'parent': parent, 'joint_id': joint_id})
        pos += 26

    if pos + 4 > len(data):
        return None, off
    index_count = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    if pos + index_count * 2 > len(data):
        return None, off
    indices = list(struct.unpack_from(f'<{index_count}H', data, pos))
    pos += index_count * 2

    mesh_groups = []
    cur_idx    = 0
    base_verts = [0] * 4
    index_offsets = [0] * 4

    for g in range(mg_count):
        if pos + 4 > len(data):
            break
        shader_idx = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        if pos + 1 > len(data):
            break
        vsi = data[pos]
        if vsi >= vset_count:
            vsi = 0
        pos += 1

        lods = []
        for lod in range(4):
            if pos + 6 > len(data):
                break
            sc, lc, nv = struct.unpack_from('<HHH', data, pos)
            pos += 6
            lods.append({'strip_start': index_offsets[vsi],
                         'strip_count': sc,
                         'list_start': index_offsets[vsi] + sc,
                         'list_count': lc,
                         'num_verts': nv})
            index_offsets[vsi] += sc + lc

        if pos + 2 > len(data):
            break
        base_adv = struct.unpack_from('<H', data, pos)[0]
        pos += 2

        a_bones_count = mg_count
        if pos + bone_count > len(data):
            break
        a_bones = list(data[pos:pos + bone_count])
        pos += bone_count

        mesh_groups.append({
            'shader_idx': shader_idx,
            'lods': lods if len(lods) == 4 else (lods + [lods[0]] * (4 - len(lods))),
            'base_vert_advance': base_adv,
            'a_bones': a_bones,
            'num_bones': bone_count,
            'vertex_set_index': vsi,
            'base_vertex': base_verts[vsi],
        })
        base_verts[vsi] += base_adv

    pos += marker_count * 52

    entry = {
        'name': name, 'flags': flags, 'bmin': bmin, 'bmax': bmax,
        'shaders': shaders, 'bones': bones,
        'vertex_sets': vertex_sets, 'vertex_data': bytes(vertex_data),
        'indices': indices, 'mesh_groups': mesh_groups,
    }
    return entry, pos


def parse_nmb(data: bytes) -> list:
    r = BinaryReader(data)
    version     = r.u32()
    model_count = r.u32()
    r.u32()

    entries = []
    off = r.tell()
    for _ in range(model_count):
        entry, off = _parse_nmb_entry(data, off)
        if entry is None:
            break
        entries.append(entry)
    return entries


def import_nmb_file(filepath, context, model_index=-1, import_armature=True):
    with open(filepath, 'rb') as f:
        data = f.read()

    entries = parse_nmb(data)
    if not entries:
        return []

    name_base = os.path.splitext(os.path.basename(filepath))[0]
    created   = []
    coll      = context.collection

    indices = range(len(entries)) if model_index < 0 else [model_index]
    for i in indices:
        if i >= len(entries):
            continue
        entry  = entries[i]
        lbl    = entry['name'] or f'{name_base}.{i:03d}'
        ob     = _build_mesh(lbl, entry, import_armature)
        coll.objects.link(ob)
        created.append(ob)

    if created:
        context.view_layer.objects.active = created[-1]
        for ob in created:
            ob.select_set(True)
    return created


def _model_items(self, context):
    fp = getattr(self, 'filepath', '')
    if not fp or not os.path.isfile(fp):
        return [('ALL', 'All models', '')]
    try:
        with open(fp, 'rb') as f:
            data = f.read()
        entries = parse_nmb(data)
        items = [('ALL', 'All models', '')]
        for i, e in enumerate(entries):
            lbl = e['name'] or f'Model {i}'
            items.append((str(i), lbl, ''))
        return items
    except Exception:
        return [('ALL', 'All models', '')]


class ImportNMB(Operator, ImportHelper):
    bl_idname  = 'import_mesh.nmb'
    bl_label   = 'Import NMB'
    bl_options = {'UNDO'}

    filename_ext = '.nmb'
    filter_glob: StringProperty(default='*.nmb;*.NMB', options={'HIDDEN'})
    import_armature: BoolProperty(name='Import Armature', default=True)
    model_index: IntProperty(name='Model index (-1 = all)', default=-1, min=-1)

    def execute(self, context):
        import_nmb_file(self.filepath, context, self.model_index,
                        self.import_armature)
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ImportNMB.bl_idname, text='NMB Bundle (.nmb)')


def register():
    bpy.utils.register_class(ImportNMB)
    bpy.types.TOPBAR_MT_file_import.append(menu_func)


def unregister():
    bpy.utils.unregister_class(ImportNMB)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func)
