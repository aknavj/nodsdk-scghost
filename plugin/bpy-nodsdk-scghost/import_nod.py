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

"""NOD importer — single skinned/rigid mesh."""

import os
import struct

import bpy
import bmesh
import mathutils
from bpy.props import BoolProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ImportHelper

from .binary_reader import BinaryReader

def _stride(t):
    return {0: 32, 1: 36, 2: 48, 3: 68, 4: 72, 5: 84}.get(t, 32)

def _base_stride(t):
    return {3: 32, 4: 36, 5: 48}.get(t, _stride(t))

def _base_type(t):
    return t - 3 if 3 <= t <= 5 else t

def _pos(x, y, z):
    return (x, z, -y)

def _uv(u, v):
    return (u, 1.0 - v)

def _strip_to_tris(idx):
    tris = []
    for i in range(len(idx) - 2):
        a, b, c = idx[i], idx[i + 1], idx[i + 2]
        if a == b or b == c or a == c:
            continue
        tris.append((b, a, c) if i & 1 else (a, b, c))
    return tris

def parse_nod(data: bytes) -> dict:
    r = BinaryReader(data)

    flags         = r.u32()
    shader_count  = r.u8()
    bone_count    = r.u8()
    vset_count    = r.u8()
    mg_count      = r.u8()
    r.u32()
    bmin          = r.vec3()
    bmax          = r.vec3()

    vset_desc = [(r.u32(), r.u32()) for _ in range(4)]
    index_count = r.u32()
    r.skip(20)

    shaders = [r.str32() for _ in range(shader_count)]

    bones = []
    for _ in range(bone_count):
        pos    = r.vec3()
        orient = struct.unpack_from('<9f', data, r.tell())
        r.skip(36)
        offset = r.vec3()
        r.u8(); r.u8()
        parent   = r.u8()
        joint_id = r.u8()
        bones.append({'pos': pos, 'orient': orient, 'offset': offset,
                      'parent': parent, 'joint_id': joint_id})

    vertex_sets = []
    total_vb = 0
    for i in range(min(vset_count, 4)):
        vt, vc = vset_desc[i]
        st  = _stride(vt)
        bst = _base_stride(vt)
        vertex_sets.append({'type': vt, 'stride': st, 'base_stride': bst,
                             'count': vc, 'data_offset': total_vb})
        total_vb += vc * st

    vertex_data = r.bytes_n(total_vb)
    indices = [r.u16() for _ in range(index_count)]

    mesh_groups = []
    cur_idx = 0
    base_verts = [0] * 4
    mg_blob_start = r.tell()
    for i in range(mg_count):
        buf = data[mg_blob_start + i * 56: mg_blob_start + i * 56 + 56]

        shader_idx = struct.unpack_from('<I', buf, 0)[0]

        lods = []
        for lod in range(4):
            sc = struct.unpack_from('<H', buf, 4 + lod * 6)[0]
            lc = struct.unpack_from('<H', buf, 6 + lod * 6)[0]
            nv = struct.unpack_from('<H', buf, 8 + lod * 6)[0]
            if sc > 0 or lc > 0 or lod == 0:
                lods.append({'strip_start': cur_idx, 'strip_count': sc,
                             'list_start': cur_idx + sc, 'list_count': lc,
                             'num_verts': nv})
                cur_idx += sc + lc
            else:
                lods.append(lods[0].copy())

        base_adv = struct.unpack_from('<H', buf, 0x1C)[0]
        a_bones  = list(buf[0x21:0x21 + 20])
        num_bones = buf[0x35]
        vsi       = buf[0x36]
        if vsi >= vset_count:
            vsi = 0

        mg = {'shader_idx': shader_idx, 'lods': lods,
              'base_vert_advance': base_adv, 'a_bones': a_bones,
              'num_bones': num_bones, 'vertex_set_index': vsi,
              'base_vertex': base_verts[vsi]}
        base_verts[vsi] += base_adv
        mesh_groups.append(mg)

    return {
        'flags': flags, 'bmin': bmin, 'bmax': bmax,
        'shaders': shaders, 'bones': bones,
        'vertex_sets': vertex_sets, 'vertex_data': vertex_data,
        'indices': indices, 'mesh_groups': mesh_groups,
    }

def _build_mesh(name, parsed, import_armature=True):
    vertex_data = parsed['vertex_data']
    vertex_sets = parsed['vertex_sets']
    indices     = parsed['indices']
    mesh_groups = parsed['mesh_groups']
    shaders     = parsed['shaders']
    bones       = parsed['bones']

    skin_table = {}
    if bones:
        for mg in mesh_groups:
            vsi  = mg['vertex_set_index']
            if vsi >= len(vertex_sets):
                continue
            vs    = vertex_sets[vsi]
            stride = vs['base_stride']
            bt    = _base_type(vs['type'])
            nV    = mg['lods'][0]['num_verts']
            base  = mg['base_vertex']
            palette  = mg['a_bones']
            num_bones_pal = mg['num_bones']
            data_off = vs['data_offset']
            for v in range(nV):
                raw_vi = base + v
                vbase  = data_off + raw_vi * stride
                if bt == 1 and stride >= 36 and vbase + 33 <= len(vertex_data):
                    li = vertex_data[vbase + 32]
                    bidx = palette[li] if li < num_bones_pal else 0
                    skin_table[(vsi, raw_vi)] = ((bidx, bidx, bidx, bidx),
                                                  (1.0, 0.0, 0.0, 0.0))
                elif bt == 2 and stride >= 48 and vbase + 48 <= len(vertex_data):
                    w0, w1, w2 = struct.unpack_from('<fff', vertex_data, vbase + 32)
                    w3 = max(0.0, 1.0 - w0 - w1 - w2)
                    li0 = vertex_data[vbase + 44]
                    li1 = vertex_data[vbase + 45]
                    li2 = vertex_data[vbase + 46]
                    li3 = vertex_data[vbase + 47]
                    bi0 = palette[li0] if li0 < num_bones_pal else 0
                    bi1 = palette[li1] if li1 < num_bones_pal else 0
                    bi2 = palette[li2] if li2 < num_bones_pal else 0
                    bi3 = palette[li3] if li3 < num_bones_pal else 0
                    skin_table[(vsi, raw_vi)] = ((bi0, bi1, bi2, bi3),
                                                  (w0, w1, w2, w3))
                else:
                    bidx = palette[0] if num_bones_pal > 0 else 0
                    skin_table[(vsi, raw_vi)] = ((bidx, bidx, bidx, bidx),
                                                  (1.0, 0.0, 0.0, 0.0))

    positions   = []
    face_loops  = []
    face_uvs    = []
    face_mats   = []

    mat_order = list(dict.fromkeys(shaders))
    mat_idx   = {n: i for i, n in enumerate(mat_order)}

    vert_cache = {}
    vert_source = []  # parallel to positions: (vsi, raw_vi)

    def get_vert(vsi, raw_vi):
        key = (vsi, raw_vi)
        if key in vert_cache:
            return vert_cache[key]
        vs  = vertex_sets[vsi]
        off = vs['data_offset'] + raw_vi * vs['stride']
        if off + 12 > len(vertex_data):
            return None
        px, py, pz = struct.unpack_from('<fff', vertex_data, off)
        vi = len(positions)
        positions.append(_pos(px, py, pz))
        vert_source.append(key)
        vert_cache[key] = vi
        return vi

    def get_uv(vsi, raw_vi):
        vs  = vertex_sets[vsi]
        off = vs['data_offset'] + raw_vi * vs['stride'] + 24
        if off + 8 > len(vertex_data):
            return (0.0, 0.0)
        u, v = struct.unpack_from('<ff', vertex_data, off)
        return _uv(u, v)

    def add_tris(tris, vsi, base_v, midx):
        for tri in tris:
            vis  = []
            uvs  = []
            ok   = True
            for raw_i in tri:
                rv = base_v + raw_i
                vi = get_vert(vsi, rv)
                if vi is None:
                    ok = False
                    break
                vis.append(vi)
                uvs.append(get_uv(vsi, rv))
            if ok and len(set(vis)) == 3:
                face_loops.append(tuple(vis))
                face_uvs.extend(uvs)
                face_mats.append(midx)

    for mg in mesh_groups:
        lod  = mg['lods'][0]
        vsi  = mg['vertex_set_index']
        if vsi >= len(vertex_sets):
            continue
        vs = vertex_sets[vsi]
        if vs['count'] == 0:
            continue

        shader_name = shaders[mg['shader_idx']] if mg['shader_idx'] < len(shaders) else ''
        midx = mat_idx.get(shader_name, 0)
        bv   = mg['base_vertex']
        n_idx = len(indices)

        if (lod['strip_count'] >= 3 and
                lod['strip_start'] + lod['strip_count'] <= n_idx):
            raw = indices[lod['strip_start']: lod['strip_start'] + lod['strip_count']]
            add_tris(_strip_to_tris(list(raw)), vsi, bv, midx)

        if (lod['list_count'] >= 3 and
                lod['list_start'] + lod['list_count'] <= n_idx):
            raw  = indices[lod['list_start']: lod['list_start'] + lod['list_count']]
            tris = [(raw[k], raw[k + 1], raw[k + 2])
                    for k in range(0, len(raw) - 2, 3)]
            add_tris(tris, vsi, bv, midx)

    me = bpy.data.meshes.new(name)
    me.from_pydata(positions, [], face_loops)
    me.update()

    uv_lay = me.uv_layers.new(name='UVMap')
    for li, loop in enumerate(me.loops):
        uv_lay.data[li].uv = face_uvs[li] if li < len(face_uvs) else (0.0, 0.0)

    for mn in mat_order:
        mat = bpy.data.materials.get(mn) or bpy.data.materials.new(mn)
        me.materials.append(mat)

    for pi, poly in enumerate(me.polygons):
        if pi < len(face_mats):
            poly.material_index = face_mats[pi]
        poly.use_smooth = True

    ob = bpy.data.objects.new(name, me)

    if bones and skin_table:
        vgroups = {}
        for vi, key in enumerate(vert_source):
            entry = skin_table.get(key)
            if not entry:
                continue
            idxs, ws = entry
            for bi, w in zip(idxs, ws):
                if w <= 0.0 or bi >= len(bones):
                    continue
                gname = f'bone.{bi:03d}'
                grp = vgroups.get(gname)
                if grp is None:
                    grp = ob.vertex_groups.new(name=gname)
                    vgroups[gname] = grp
                grp.add([vi], w, 'ADD')

    if import_armature and bones:
        TAIL_OFFSET = mathutils.Vector((0.0, 0.05, 0.0))

        def bone_head(bi):
            ox, oy, oz = bones[bi]['offset']
            return mathutils.Vector(_pos(-ox, -oy, -oz))

        arm_name = name + '_Armature'
        arm = bpy.data.armatures.new(arm_name)
        arm_ob = bpy.data.objects.new(arm_name, arm)
        bpy.context.collection.objects.link(arm_ob)
        bpy.context.view_layer.objects.active = arm_ob
        bpy.ops.object.mode_set(mode='EDIT')

        eb_list = []
        heads = [bone_head(bi) for bi in range(len(bones))]
        for bi, _ in enumerate(bones):
            eb = arm.edit_bones.new(f'bone.{bi:03d}')
            eb.head = heads[bi]
            eb.tail = heads[bi] + TAIL_OFFSET
            eb.roll = 0.0
            eb_list.append(eb)
        for bi, bone in enumerate(bones):
            par = bone['parent']
            if par != 0xFF and par < len(eb_list):
                eb_list[bi].parent = eb_list[par]
                eb_list[bi].use_connect = False
        bpy.ops.object.mode_set(mode='OBJECT')

        mod = ob.modifiers.new('Armature', 'ARMATURE')
        mod.object = arm_ob
        ob.parent = arm_ob

    return ob

def import_nod_file(filepath, context, import_armature=True):
    with open(filepath, 'rb') as f:
        data = f.read()

    parsed = parse_nod(data)
    name   = os.path.splitext(os.path.basename(filepath))[0]
    ob     = _build_mesh(name, parsed, import_armature)

    coll = context.collection
    coll.objects.link(ob)
    context.view_layer.objects.active = ob
    ob.select_set(True)
    return ob

class ImportNOD(Operator, ImportHelper):
    bl_idname  = 'import_mesh.nod'
    bl_label   = 'Import NOD'
    bl_options = {'UNDO'}

    filename_ext = '.nod'
    filter_glob: StringProperty(default='*.nod;*.NOD', options={'HIDDEN'})
    import_armature: BoolProperty(name='Import Armature', default=True)

    def execute(self, context):
        import_nod_file(self.filepath, context, self.import_armature)
        return {'FINISHED'}

def menu_func(self, context):
    self.layout.operator(ImportNOD.bl_idname, text='NOD Model (.nod)')

def register():
    bpy.utils.register_class(ImportNOD)
    bpy.types.TOPBAR_MT_file_import.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ImportNOD)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func)
