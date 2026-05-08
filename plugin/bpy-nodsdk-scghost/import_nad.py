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
NAD importer — animation clip for NOD/NMB skeletons.

Binary layout:
  0x00  version       (uint32)  — must be 4
  0x04  num_tracks    (uint32)
  0x08  flags         (uint32)
  0x0C  duration      (float)   — total frames

  per bone track:
    num_keys    (uint32)
    bone_number (uint32)
    type        (uint32)  0=rotation(euler°) / 1=translation
    keys × 52 B:
      frame   (float)
      value   (3×float)
      b_coeff (3×float)
      c_coeff (3×float)
      d_coeff (3×float)

  num_key_tags  (uint32)
  tags × 8 B:
    tag_type  (uint32)
    frame_num (float)

  num_shape_tracks (uint32)
  per shape track:
    group_num (uint8)
    shape_num (uint8)
    num_keys  (uint16)
    keys × 8 B: frame(float) + weight(float)

Rotation evaluation (cubic polynomial):
  val(frame) = k[i].value + k[i].b*(t) + k[i].c*(t²) + k[i].d*(t³)
  where t = frame - k[i].frame,  k[i].frame ≤ frame < k[i+1].frame

Euler convention (from viewer BoneEvaluator.cpp buildEulerRotation):
  euler.x = pitch, euler.y = yaw, euler.z = roll — all in degrees.
  Applied order: Ry * Rx * Rz  (intrinsic ZXY).

Bone naming convention expected on the target armature:
  'bone.000', 'bone.001', … matching the index used in NOD/NMB import.
"""

import math
import os
import struct

import bpy
from bpy.props import BoolProperty, FloatProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ImportHelper
from mathutils import Euler, Matrix, Quaternion, Vector
from .binary_reader import BinaryReader

def _eval_track(keys, frame):
    if not keys:
        return (0.0, 0.0, 0.0)
    if frame <= keys[0]['frame']:
        return keys[0]['value']
    last = keys[-1]
    if frame >= last['frame']:
        return last['value']
    for i in range(len(keys) - 1):
        if frame <= keys[i + 1]['frame']:
            k  = keys[i]
            t  = frame - k['frame']
            t2 = t * t
            t3 = t2 * t
            bx, by, bz = k['b']
            cx, cy, cz = k['c']
            dx, dy, dz = k['d']
            vx, vy, vz = k['value']
            return (vx + bx*t + cx*t2 + dx*t3,
                    vy + by*t + cy*t2 + dy*t3,
                    vz + bz*t + cz*t2 + dz*t3)
    return last['value']

def _euler_to_quat(ex, ey, ez):
    px = math.radians(ex)
    py = math.radians(ey)
    pz = math.radians(ez)
    sP, cP = math.sin(px), math.cos(px)
    sY, cY = math.sin(py), math.cos(py)
    sR, cR = math.sin(pz), math.cos(pz)

    mat_game = Matrix((
        ( cY*cR,                  sY*sP*cR - cP*sR,  sY*cP*cR + sP*sR),
        ( cY*sR,                  sY*sP*sR + cP*cR,  sY*cP*sR - sP*cR),
        (-sY,                     cY*sP,              cP*cY),
    ))

    C = Matrix((
        (1,  0,  0),
        (0,  0,  1),
        (0, -1,  0),
    ))
    mat_bl = C @ mat_game @ C.transposed()
    return mat_bl.to_4x4().to_quaternion()

def _cvt_pos(x, y, z):
    return Vector((x, z, -y))

def parse_nad(data: bytes) -> dict:
    r = BinaryReader(data)
    if len(data) < 16:
        raise ValueError('File too small')

    version    = r.u32()
    num_tracks = r.u32()
    flags      = r.u32()
    duration   = r.f32()

    tracks = []
    for _ in range(num_tracks):
        nk          = r.u32()
        bone_number = r.u32()
        ttype       = r.u32()
        keys = []
        for _ in range(nk):
            frame = r.f32()
            val   = r.vec3()
            b     = r.vec3()
            c     = r.vec3()
            d     = r.vec3()
            keys.append({'frame': frame, 'value': val, 'b': b, 'c': c, 'd': d})
        tracks.append({'bone_number': bone_number, 'type': ttype, 'keys': keys})

    num_tags = r.u32() if r.remaining() >= 4 else 0
    r.skip(num_tags * 8)

    return {'version': version, 'duration': duration, 'tracks': tracks}

def apply_nad_to_armature(arm_ob, parsed, fps=24.0, frame_offset=1):
    """Bake animation from parsed NAD onto arm_ob's pose bones."""
    duration = parsed['duration']
    tracks   = parsed['tracks']
    total_frames = max(1, int(duration))

    bpy.context.view_layer.objects.active = arm_ob
    arm_ob.select_set(True)

    rot_tracks   = {}
    trans_tracks = {}
    for t in tracks:
        bn = t['bone_number']
        if t['type'] == 0:
            rot_tracks[bn] = t['keys']
        else:
            trans_tracks[bn] = t['keys']

    for bi, pbone in enumerate(arm_ob.pose.bones):
        pbone.rotation_mode = 'QUATERNION'

    scene = bpy.context.scene
    scene.frame_start = frame_offset
    scene.frame_end   = frame_offset + total_frames

    for frame_int in range(total_frames + 1):
        frame = float(frame_int)
        scene.frame_set(frame_offset + frame_int)

        for bi in range(len(arm_ob.pose.bones)):
            bone_name = f'bone.{bi:03d}'
            try:
                pbone = arm_ob.pose.bones[bone_name]
            except KeyError:
                continue

            rot   = rot_tracks.get(bi)
            trans = trans_tracks.get(bi)

            if rot is not None:
                ex, ey, ez = _eval_track(rot, frame)
                q = _euler_to_quat(ex, ey, ez)
                pbone.rotation_quaternion = q
                pbone.keyframe_insert('rotation_quaternion')

            if trans is not None:
                tx, ty, tz = _eval_track(trans, frame)
                # Add to rest position (rest is encoded in bone head/tail)
                pbone.location = _cvt_pos(tx, ty, tz)
                pbone.keyframe_insert('location')

    scene.frame_set(frame_offset)

def import_nad_file(filepath, context, fps=24.0):
    with open(filepath, 'rb') as f:
        data = f.read()

    parsed = parse_nad(data)

    arm_ob = context.object
    if arm_ob is None or arm_ob.type != 'ARMATURE':
        raise ValueError('Select an Armature object before importing a NAD animation')

    apply_nad_to_armature(arm_ob, parsed, fps)
    return arm_ob

class ImportNAD(Operator, ImportHelper):
    bl_idname  = 'import_anim.nad'
    bl_label   = 'Import NAD Animation'
    bl_options = {'UNDO'}

    filename_ext = '.nad'
    filter_glob: StringProperty(default='*.nad;*.NAD', options={'HIDDEN'})
    fps: FloatProperty(name='Source FPS', default=24.0, min=1.0, max=120.0)

    def execute(self, context):
        try:
            import_nad_file(self.filepath, context, self.fps)
        except ValueError as e:
            self.report({'ERROR'}, str(e))
            return {'CANCELLED'}
        return {'FINISHED'}

    def invoke(self, context, event):
        if context.object is None or context.object.type != 'ARMATURE':
            self.report({'ERROR'}, 'Select an Armature object first')
            return {'CANCELLED'}
        return super().invoke(context, event)

def menu_func(self, context):
    self.layout.operator(ImportNAD.bl_idname, text='NAD Animation (.nad)')

def register():
    bpy.utils.register_class(ImportNAD)
    bpy.types.TOPBAR_MT_file_import.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ImportNAD)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func)
