# Exports the open .blend as a binary glTF 2.0 (.glb) that simlib loads, using the settings simlib is tested against.
#
#   blender --background model.blend --python tools/blender/export_glb.py -- out.glb [--no-anim] [--selected]
#
# Y-up conversion is on (simlib is Y-up), modifiers are applied, and NLA strips are exported as separate named
# clips (play them with sl::play_model_clip()). Note that Blender starts clip keys at the action's first frame,
# so a clip authored from frame 1 at 24 fps begins at 1/24 s; simlib clamps before the first key.
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
paths = [a for a in argv if not a.startswith("--")]
if not paths:
    sys.exit("Usage: blender --background model.blend --python export_glb.py -- out.glb [--no-anim] [--selected]")
out_path = paths[0]
animations = "--no-anim" not in argv
selected = "--selected" in argv

kwargs = dict(
    filepath=out_path,
    export_format="GLB",
    export_yup=True,
    export_apply=True,
    export_animations=animations,
    use_selection=selected,
)
if animations:
    # One clip per NLA track/action, sampled only where needed so LINEAR/STEP keys stay as authored.
    kwargs.update(export_nla_strips=True, export_force_sampling=False)

result = bpy.ops.export_scene.gltf(**kwargs)
if result != {"FINISHED"}:
    sys.exit("glTF export failed: %s" % result)
print("simlib: wrote", out_path)
