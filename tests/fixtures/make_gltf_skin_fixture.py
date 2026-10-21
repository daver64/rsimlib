# Regenerates tests/fixtures/gltf_skin.glb with Blender 4.x:
#   blender --background --factory-startup --python tests/fixtures/make_gltf_skin_fixture.py -- tests/fixtures/gltf_skin.glb
#
# A 1 x 1 x 4 bar (Blender Z up, standing on the origin) skinned to two bones: "Lower" (z 0..2) and "Upper" (z 2..4).
# The "Bend" clip turns Upper 90 degrees about Blender X over one second, folding the top half of the bar sideways.
import math
import sys

import bpy

out_path = sys.argv[sys.argv.index("--") + 1]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = 24

bpy.ops.mesh.primitive_cube_add(size=1.0)
bar = bpy.context.active_object
bar.name = "Bar"
bar.scale = (1.0, 1.0, 4.0)
bar.location = (0.0, 0.0, 2.0)
bpy.ops.object.transform_apply(location=True, scale=True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.subdivide(number_cuts=7)
bpy.ops.object.mode_set(mode="OBJECT")

lower = bar.vertex_groups.new(name="Lower")
upper = bar.vertex_groups.new(name="Upper")
for vertex in bar.data.vertices:
    w = min(1.0, max(0.0, (vertex.co.z - 1.5) / 1.0))
    if w < 1.0:
        lower.add([vertex.index], 1.0 - w, "REPLACE")
    if w > 0.0:
        upper.add([vertex.index], w, "REPLACE")

armature_data = bpy.data.armatures.new("Rig")
rig = bpy.data.objects.new("Rig", armature_data)
scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="EDIT")
bone_lower = armature_data.edit_bones.new("Lower")
bone_lower.head = (0.0, 0.0, 0.0)
bone_lower.tail = (0.0, 0.0, 2.0)
bone_upper = armature_data.edit_bones.new("Upper")
bone_upper.head = (0.0, 0.0, 2.0)
bone_upper.tail = (0.0, 0.0, 4.0)
bone_upper.parent = bone_lower
bone_upper.use_connect = True
bpy.ops.object.mode_set(mode="OBJECT")

bar.parent = rig
modifier = bar.modifiers.new("Armature", "ARMATURE")
modifier.object = rig

pose = rig.pose.bones["Upper"]
pose.rotation_mode = "XYZ"
rig.animation_data_create()
action = bpy.data.actions.new("Bend")
rig.animation_data.action = action
for frame, angle in ((1, 0.0), (25, math.radians(90.0))):
    pose.rotation_euler = (angle, 0.0, 0.0)
    pose.keyframe_insert("rotation_euler", index=0, frame=frame)
for fcurve in action.fcurves:
    for point in fcurve.keyframe_points:
        point.interpolation = "LINEAR"
track = rig.animation_data.nla_tracks.new()
track.name = "Bend"
track.strips.new("Bend", 1, action)
rig.animation_data.action = None

bpy.ops.export_scene.gltf(
    filepath=out_path,
    export_format="GLB",
    export_yup=True,
    export_animations=True,
    export_nla_strips=True,
    export_skins=True,
    export_apply=True,
)
