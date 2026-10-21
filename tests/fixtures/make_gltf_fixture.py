# Regenerates tests/fixtures/gltf_blender.glb with Blender 4.x:
#   blender --background --factory-startup --python tests/fixtures/make_gltf_fixture.py -- tests/fixtures/gltf_blender.glb
#
# The scene is deliberately lopsided so axis conversion is testable: "Hand" is a 2 x 1 x 3 box (Blender X, Y, Z),
# which glTF stores Y-up as 2 wide, 3 tall and 1 deep. "Arm" is an empty that parents Hand.
import sys

import bpy

out_path = sys.argv[sys.argv.index("--") + 1]

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = 24

# A 4x4 checker image so a texture travels inside the .glb.
image = bpy.data.images.new("checker", 4, 4)
pixels = []
for y in range(4):
    for x in range(4):
        on = (x + y) % 2 == 0
        pixels += [1.0, 0.2, 0.2, 1.0] if on else [0.2, 0.2, 1.0, 1.0]
image.pixels = pixels
image.filepath_raw = "/tmp/simlib_checker.png"
image.file_format = "PNG"
image.save()
image.pack()

material = bpy.data.materials.new("Checker")
material.use_nodes = True
nodes = material.node_tree.nodes
texture = nodes.new("ShaderNodeTexImage")
texture.image = image
material.node_tree.links.new(texture.outputs["Color"], nodes["Principled BSDF"].inputs["Base Color"])

arm = bpy.data.objects.new("Arm", None)
scene.collection.objects.link(arm)
arm.location = (0.0, 0.0, 1.0)

bpy.ops.mesh.primitive_cube_add(size=1.0)
hand = bpy.context.active_object
hand.name = "Hand"
hand.scale = (2.0, 1.0, 3.0)
bpy.ops.object.transform_apply(scale=True)
hand.data.materials.append(material)
hand.parent = arm
hand.location = (1.0, 0.0, 0.0)


def make_action(name, target, key_path, index, keys, interpolation):
    action = bpy.data.actions.new(name)
    target.animation_data_create()
    target.animation_data.action = action
    for frame, value in keys:
        setattr(target, key_path, getattr(target, key_path))
        values = list(getattr(target, key_path))
        values[index] = value
        setattr(target, key_path, values)
        target.keyframe_insert(data_path=key_path, index=index, frame=frame)
    for fcurve in action.fcurves:
        for point in fcurve.keyframe_points:
            point.interpolation = interpolation
    track = target.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, int(keys[0][0]), action)
    target.animation_data.action = None
    return action


import math

# "Swing": the arm turns 90 degrees about Blender Z (glTF Y) over one second.
make_action("Swing", arm, "rotation_euler", 2, [(1, 0.0), (25, math.radians(90.0))], "LINEAR")
# "Slide": the hand moves from x=1 to x=3 and back over two seconds, so it loops smoothly.
make_action("Slide", hand, "location", 0, [(1, 1.0), (25, 3.0), (49, 1.0)], "LINEAR")
# "Hop": STEP interpolation, the hand jumps x=1, 2, 3 once a second-half each.
make_action("Hop", hand, "location", 0, [(1, 1.0), (13, 2.0), (25, 3.0)], "CONSTANT")

bpy.ops.export_scene.gltf(
    filepath=out_path,
    export_format="GLB",
    export_yup=True,
    export_animations=True,
    export_nla_strips=True,
    export_force_sampling=False,
    export_apply=True,
)
