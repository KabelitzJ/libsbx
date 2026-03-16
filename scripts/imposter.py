"""
Generate impostor atlas for tree billboard LODs.

Usage:
  "C:\\Program Files\\Blender Foundation\\Blender 4.4\\blender.exe" --background --python generate_impostor_atlas.py

Configure TREES and OUTPUT below. Each tree is rendered from NUM_ANGLES
evenly spaced camera positions around Y axis. Results are packed into a
single atlas PNG with one row per tree type.

Requirements: Blender 3.6+
"""

import bpy
import math
import os
from mathutils import Vector

# ──────────────────────────── CONFIG ────────────────────────────

TREES = [
    {"name": "tree_1", "path": "D:/Development/sandbox/demo/assets/meshes/trees/tree_1/tree_1.gltf"},
    {"name": "tree_2", "path": "D:/Development/sandbox/demo/assets/meshes/trees/tree_2/tree_2.gltf"},
]

OUTPUT = "D:/Development/sandbox/demo/assets/meshes/trees/impostor_atlas.png"

NUM_ANGLES = 8
CELL_SIZE = 512
SAMPLES = 64
USE_TRANSPARENT = True

# ──────────────────────────── SETUP ─────────────────────────────

def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    for collection in bpy.data.collections:
        bpy.data.collections.remove(collection)

    for mesh in bpy.data.meshes:
        bpy.data.meshes.remove(mesh)

    for material in bpy.data.materials:
        bpy.data.materials.remove(material)

    for image in bpy.data.images:
        bpy.data.images.remove(image)


def setup_render():
    scene = bpy.context.scene

    scene.render.engine = "CYCLES"
    scene.cycles.samples = SAMPLES
    scene.cycles.use_denoising = True

    scene.render.resolution_x = CELL_SIZE
    scene.render.resolution_y = CELL_SIZE
    scene.render.resolution_percentage = 100

    scene.render.film_transparent = USE_TRANSPARENT
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"

    world = bpy.data.worlds.get("World")

    if world is None:
        world = bpy.data.worlds.new("World")

    scene.world = world
    world.use_nodes = True

    nodes = world.node_tree.nodes
    nodes.clear()

    bg = nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = (0.8, 0.85, 0.9, 1.0)
    bg.inputs["Strength"].default_value = 2.0

    output = nodes.new("ShaderNodeOutputWorld")

    world.node_tree.links.new(bg.outputs["Background"], output.inputs["Surface"])


def setup_camera():
    bpy.ops.object.camera_add()

    camera = bpy.context.object
    camera.name = "ImpostorCamera"

    camera.data.type = "ORTHO"
    camera.data.clip_start = 0.01
    camera.data.clip_end = 100.0

    bpy.context.scene.camera = camera

    return camera


def import_tree(path):
    before = set(bpy.data.objects)

    bpy.ops.import_scene.gltf(filepath=path)

    after = set(bpy.data.objects)
    new_objects = after - before

    if not new_objects:
        raise RuntimeError(f"No objects imported from {path}")

    min_corner = Vector((float("inf"),) * 3)
    max_corner = Vector((float("-inf"),) * 3)

    for obj in new_objects:
        if obj.type == "MESH":
            for corner in obj.bound_box:
                world_corner = obj.matrix_world @ Vector(corner)

                min_corner.x = min(min_corner.x, world_corner.x)
                min_corner.y = min(min_corner.y, world_corner.y)
                min_corner.z = min(min_corner.z, world_corner.z)
                max_corner.x = max(max_corner.x, world_corner.x)
                max_corner.y = max(max_corner.y, world_corner.y)
                max_corner.z = max(max_corner.z, world_corner.z)

    return new_objects, min_corner, max_corner


def render_angle(camera, center, radius, height, angle_index):
    angle = (2.0 * math.pi * angle_index) / NUM_ANGLES

    cam_x = center.x + radius * math.cos(angle)
    cam_y = center.y + height * 0.5
    cam_z = center.z + radius * math.sin(angle)

    camera.location = Vector((cam_x, cam_y, cam_z))

    look_at = Vector((center.x, center.y + height * 0.5, center.z))
    direction = look_at - camera.location
    rot_quat = direction.to_track_quat("-Z", "Y")
    camera.rotation_euler = rot_quat.to_euler()


def render_tree(tree_path, tree_index, temp_dir):
    clear_scene()
    setup_render()
    camera = setup_camera()

    objects, bb_min, bb_max = import_tree(tree_path)

    size = bb_max - bb_min
    center = (bb_min + bb_max) * 0.5
    center.y = bb_min.y

    height = size.y
    width = max(size.x, size.z)

    camera.data.ortho_scale = max(height, width) * 1.2

    radius = max(width, height) * 3.0

    paths = []

    for angle_index in range(NUM_ANGLES):
        render_angle(camera, center, radius, height, angle_index)

        filepath = os.path.join(temp_dir, f"tree_{tree_index}_angle_{angle_index}.png")

        bpy.context.scene.render.filepath = filepath
        bpy.ops.render.render(write_still=True)

        paths.append(filepath)

        print(f"  Rendered angle {angle_index + 1}/{NUM_ANGLES}")

    return paths


# ──────────────────────────── ATLAS ─────────────────────────────

def compose_atlas(all_paths, output_path):
    import numpy as np

    atlas_w = NUM_ANGLES * CELL_SIZE
    atlas_h = len(TREES) * CELL_SIZE

    atlas = np.zeros((atlas_h, atlas_w, 4), dtype=np.float32)

    for tree_index, angle_paths in enumerate(all_paths):
        for angle_index, path in enumerate(angle_paths):
            img = bpy.data.images.load(path)

            pixels = np.array(img.pixels[:]).reshape(img.size[1], img.size[0], 4)

            # Flip vertically (Blender images are bottom-up)
            pixels = pixels[::-1, :, :]

            row_start = tree_index * CELL_SIZE
            col_start = angle_index * CELL_SIZE

            atlas[row_start:row_start + CELL_SIZE, col_start:col_start + CELL_SIZE, :] = pixels

            bpy.data.images.remove(img)

    atlas_img = bpy.data.images.new("impostor_atlas", atlas_w, atlas_h, alpha=True)

    # Flip back to Blender convention (bottom-up)
    atlas_img.pixels = atlas[::-1, :, :].flatten().tolist()

    atlas_img.filepath_raw = output_path
    atlas_img.file_format = "PNG"
    atlas_img.save()

    print(f"Atlas saved: {output_path} ({atlas_w}x{atlas_h})")


# ──────────────────────────── MAIN ──────────────────────────────

def main():
    import tempfile

    temp_dir = tempfile.mkdtemp(prefix="impostor_")

    print(f"Temp directory: {temp_dir}")

    all_paths = []

    for tree_index, tree_info in enumerate(TREES):
        print(f"Rendering {tree_info['name']} ({tree_index + 1}/{len(TREES)})...")

        paths = render_tree(tree_info["path"], tree_index, temp_dir)
        all_paths.append(paths)

    compose_atlas(all_paths, OUTPUT)

    for angle_paths in all_paths:
        for path in angle_paths:
            if os.path.exists(path):
                os.remove(path)

    os.rmdir(temp_dir)

    print("Done.")


if __name__ == "__main__":
    main()
