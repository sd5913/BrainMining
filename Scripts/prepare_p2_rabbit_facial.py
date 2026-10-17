"""Add lightweight facial morph targets to the supplied rabbit FBX.

The source has a skinned face but no animation clips, blend shapes or particle hair.
The generated target is deliberately small and keeps the original materials.
"""
import bpy
from pathlib import Path

SOURCE = Path(r"E:\涌现taptap2026\assets\rabbit.fbx")
OUTPUT = Path(r"E:\涌现taptap2026\BrainMining\Art\Passages\Echo\P2Rabbit\rabbit_anim.fbx")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(SOURCE))
mesh_obj = next(o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("Bunny"))
for obj in list(bpy.context.scene.objects):
    if obj != mesh_obj and obj.type not in {"ARMATURE"}:
        bpy.data.objects.remove(obj, do_unlink=True)

mesh = mesh_obj.data
plastic = [p for p in mesh.polygons if p.material_index == 1]
adj = {v: set() for p in plastic for v in p.vertices}
for p in plastic:
    for v in p.vertices:
        adj[v].update(p.vertices)
components = []
remaining = set(adj)
while remaining:
    seed = remaining.pop()
    stack = [seed]
    comp = {seed}
    while stack:
        v = stack.pop()
        for n in adj[v]:
            if n in remaining:
                remaining.remove(n)
                comp.add(n)
                stack.append(n)
    coords = [mesh.vertices[i].co for i in comp]
    bounds = tuple((min(v[j] for v in coords), max(v[j] for v in coords)) for j in range(3))
    components.append((comp, bounds))

eyes = [c for c, b in components if b[2][0] > .42]
mouth_candidates = [c for c, b in components if b[2][1] < .405]
mouth = [max(mouth_candidates, key=len)] if mouth_candidates else []
print("FACE_COMPONENTS", [(len(c), b) for c, b in components], "eyes", len(eyes), "mouth", len(mouth))
if len(eyes) != 2 or not mouth:
    raise RuntimeError(f"Unexpected face components: {[(len(c), b) for c, b in components]}")
eye_ids = set().union(*eyes)
mouth_ids = set().union(*mouth)

mesh_obj.shape_key_add(name="Basis")

blink = mesh_obj.shape_key_add(name="Blink")
for i in eye_ids:
    z = mesh.vertices[i].co.z
    blink.data[i].co.z = .437 + (z - .437) * .08
    blink.data[i].co.y -= .002

open_mouth = mesh_obj.shape_key_add(name="MouthOpen")
for i in mouth_ids:
    z = mesh.vertices[i].co.z
    open_mouth.data[i].co.z = .394 + (z - .394) * 1.45 - .010
    open_mouth.data[i].co.y -= .003

angry = mesh_obj.shape_key_add(name="Angry")
for i in eye_ids:
    x = mesh.vertices[i].co.x
    angry.data[i].co.z = .437 + (mesh.vertices[i].co.z - .437) * .78 + (0.006 if x > 0 else -0.006)
    angry.data[i].co.y -= .001
for i in mouth_ids:
    angry.data[i].co.z -= .005

for key in mesh_obj.data.shape_keys.key_blocks:
    key.value = 0.0

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="DESELECT")
mesh_obj.select_set(True)
for obj in bpy.context.scene.objects:
    if obj.type == "ARMATURE":
        obj.select_set(True)
bpy.context.view_layer.objects.active = mesh_obj
bpy.ops.export_scene.fbx(
    filepath=str(OUTPUT),
    use_selection=True,
    object_types={"MESH", "ARMATURE"},
    add_leaf_bones=False,
    bake_anim=False,
    use_mesh_modifiers=True,
    use_armature_deform_only=True,
)
print("P2_RABBIT_FACIAL_FBX_READY", OUTPUT)
