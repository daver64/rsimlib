# Regenerates tests/fixtures/gltf_pbr.glb with Blender 4.x:
#   blender --background --factory-startup --python tests/fixtures/make_gltf_pbr_fixture.py -- tests/fixtures/gltf_pbr.glb
#
# Three spheres in a row along Blender X: a polished metal ("Metal"), a rough red plastic ("Plastic") and a glowing
# green one ("Glow", emission strength 2).
import sys

import bpy

out_path = sys.argv[sys.argv.index("--") + 1]

bpy.ops.wm.read_factory_settings(use_empty=True)


def sphere(name, x, base, metallic, roughness, emission=None, strength=0.0):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.8, location=(x, 0.0, 0.0), segments=48, ring_count=24)
    obj = bpy.context.active_object
    obj.name = name
    bpy.ops.object.shade_smooth()
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    bsdf = material.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = base
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission:
        bsdf.inputs["Emission Color"].default_value = emission
        bsdf.inputs["Emission Strength"].default_value = strength
    obj.data.materials.append(material)


sphere("Metal", -2.0, (0.9, 0.7, 0.3, 1.0), 1.0, 0.2)
sphere("Plastic", 0.0, (0.8, 0.1, 0.1, 1.0), 0.0, 0.3)
sphere("Glow", 2.0, (0.1, 0.3, 0.1, 1.0), 0.0, 0.8, (0.2, 1.0, 0.2, 1.0), 2.0)

bpy.ops.export_scene.gltf(filepath=out_path, export_format="GLB", export_yup=True, export_apply=True)
