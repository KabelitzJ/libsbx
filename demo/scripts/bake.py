"""
Map baker.

Inputs:
  <map_dir>/provinces.png
  <map_dir>/provinces.yaml
  <map_dir>/heightmap.png
  <map_dir>/regions.yaml      (optional)

Outputs:
  <map_dir>/map.bin
  <map_dir>/province_id.r32u
  <map_dir>/heightmap.r16
"""

import argparse
import struct
import sys
from pathlib import Path

import numpy as np
import yaml
from PIL import Image

MAGIC = b"LMAP"
VERSION = 1

INVALID_PROVINCE = 0xFFFFFFFF
INVALID_REGION = 0xFFFF

TERRAIN_OCEAN = 0

BORDER_LAND = 0
BORDER_COAST = 1
BORDER_SEA = 2


def load_provinces_png(path):
  return np.array(Image.open(path).convert("RGB"))


def rasterize_province_ids(provinces_png, provinces_meta):
  h, w, _ = provinces_png.shape

  packed = (
    provinces_png[:, :, 0].astype(np.uint32) << 16
    | provinces_png[:, :, 1].astype(np.uint32) << 8
    | provinces_png[:, :, 2].astype(np.uint32)
  )

  unique_keys, inverse = np.unique(packed, return_inverse=True)
  id_for_key = np.full(len(unique_keys), INVALID_PROVINCE, dtype=np.uint32)

  for entry in provinces_meta:
    c = entry["color"]
    key = (c[0] << 16) | (c[1] << 8) | c[2]
    idx = np.searchsorted(unique_keys, key)
    if idx < len(unique_keys) and unique_keys[idx] == key:
      id_for_key[idx] = entry["id"]

  return id_for_key[inverse].reshape(h, w)


def validate(province_ids, provinces_meta):
  unique_ids = np.unique(province_ids)
  if (unique_ids == INVALID_PROVINCE).any():
    raise RuntimeError("provinces.png contains colors not in provinces.yaml")

  yaml_ids = {p["id"] for p in provinces_meta}
  bitmap_ids = set(unique_ids.tolist())

  orphans = yaml_ids - bitmap_ids
  if orphans:
    print(f"WARN: provinces with zero pixels: {sorted(orphans)}", file=sys.stderr)

  unknown = bitmap_ids - yaml_ids
  if unknown:
    raise RuntimeError(f"province ids in bitmap not in YAML: {sorted(unknown)}")


def compute_adjacency(province_ids):
  h_a = province_ids[:, :-1]
  h_b = province_ids[:, 1:]
  h_mask = h_a != h_b
  h_pairs = np.column_stack([h_a[h_mask], h_b[h_mask]])

  v_a = province_ids[:-1, :]
  v_b = province_ids[1:, :]
  v_mask = v_a != v_b
  v_pairs = np.column_stack([v_a[v_mask], v_b[v_mask]])

  pairs = np.vstack([h_pairs, v_pairs])
  pairs = np.sort(pairs, axis=1)
  unique, counts = np.unique(pairs, axis=0, return_counts=True)

  return unique, counts.astype(np.float32)


def classify_edges(edge_pairs, provinces_meta):
  max_id = max(p["id"] for p in provinces_meta)
  terrain_lut = np.zeros(max_id + 1, dtype=np.uint8)
  for p in provinces_meta:
    terrain_lut[p["id"]] = p["terrain"]

  ta = terrain_lut[edge_pairs[:, 0]]
  tb = terrain_lut[edge_pairs[:, 1]]

  classes = np.full(len(edge_pairs), BORDER_LAND, dtype=np.uint8)
  classes[(ta == TERRAIN_OCEAN) & (tb == TERRAIN_OCEAN)] = BORDER_SEA
  classes[((ta == TERRAIN_OCEAN) ^ (tb == TERRAIN_OCEAN))] = BORDER_COAST

  return classes


def build_string_table(strings):
  table = bytearray()
  offsets = []
  for s in strings:
    offsets.append(len(table))
    table.extend(s.encode("utf-8"))
    table.append(0)
  return bytes(table), offsets


def write_map_bin(path, w, h, provinces_meta, edge_pairs, edge_lengths,
                  edge_classes, regions_meta):
  strings = [p.get("name", "") for p in provinces_meta]
  strings += [r.get("name", "") for r in regions_meta]

  string_table, str_offsets = build_string_table(strings)
  region_str_base = len(provinces_meta)

  with open(path, "wb") as f:
    f.write(struct.pack(
      "<4sIIIIIII",
      MAGIC, VERSION,
      w, h,
      len(provinces_meta), len(edge_pairs), len(regions_meta),
      len(string_table),
    ))

    for i, p in enumerate(provinces_meta):
      c = p["color"]
      color_packed = (c[0] << 16) | (c[1] << 8) | c[2]
      f.write(struct.pack(
        "<IIffBBHffI",
        p["id"],
        color_packed,
        float(p["centroid"][0]), float(p["centroid"][1]),
        p["terrain"], p["land_use"],
        p.get("region_id", INVALID_REGION),
        float(p["elevation"]), float(p["moisture"]),
        str_offsets[i],
      ))

    for i in range(len(edge_pairs)):
      a, b = edge_pairs[i]
      f.write(struct.pack(
        "<IIB3xfHHHH",
        int(a), int(b),
        int(edge_classes[i]),
        float(edge_lengths[i]),
        0, 0, 0, 0,
      ))

    for j, r in enumerate(regions_meta):
      f.write(struct.pack(
        "<H2xIII",
        r["id"],
        str_offsets[region_str_base + j],
        r.get("capital_province_id", INVALID_PROVINCE),
        r.get("traits", 0),
      ))

    f.write(string_table)


def write_textures(map_dir, province_ids, heightmap_png_path):
  province_ids.astype(np.uint32).tofile(map_dir / "province_id.r32u")

  height_8 = np.array(Image.open(heightmap_png_path).convert("L"))
  height_16 = (height_8.astype(np.uint32) * 257).astype(np.uint16)
  height_16.tofile(map_dir / "heightmap.r16")


if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--map-dir", type=str, required=True)
  args = parser.parse_args()

  map_dir = Path(args.map_dir)

  with open(map_dir / "provinces.yaml") as f:
    provinces_meta = yaml.safe_load(f)

  regions_meta = []
  regions_path = map_dir / "regions.yaml"
  if regions_path.exists():
    with open(regions_path) as f:
      regions_meta = yaml.safe_load(f) or []

  provinces_png = load_provinces_png(map_dir / "provinces.png")
  province_ids = rasterize_province_ids(provinces_png, provinces_meta)

  validate(province_ids, provinces_meta)

  edge_pairs, edge_lengths = compute_adjacency(province_ids)
  edge_classes = classify_edges(edge_pairs, provinces_meta)

  h, w = province_ids.shape
  write_map_bin(
    map_dir / "map.bin",
    w, h,
    provinces_meta, edge_pairs, edge_lengths, edge_classes,
    regions_meta,
  )

  write_textures(map_dir, province_ids, map_dir / "heightmap.png")

  print(f"Baked {len(provinces_meta)} provinces, {len(edge_pairs)} edges, "
        f"{len(regions_meta)} regions")
  print(f"Wrote: {map_dir / 'map.bin'}")
  print(f"Wrote: {map_dir / 'province_id.r32u'}")
  print(f"Wrote: {map_dir / 'heightmap.r16'}")
