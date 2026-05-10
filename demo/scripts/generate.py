import argparse
from pathlib import Path

import colorsys
import numpy as np
import yaml
from opensimplex import OpenSimplex
from PIL import Image, ImageDraw
from scipy.spatial import cKDTree
from scipy.ndimage import gaussian_filter, distance_transform_edt

W, H, N = 4096, 2048, 120
FALLOFF = 1.25
RELAX_ITERS = 3
SEED = 480923423

HILLS_QUANTILE = 0.55
MOUNTAINS_QUANTILE = 0.85
EDGE_EROSION = 0.02

DETAIL_AMPLITUDE = 1.0

LAKE_FRACTION = 0.12
LAKE_INTERIOR_BIAS = 1.0

TERRAIN_OCEAN = 0    # used for lakes too
TERRAIN_PLAINS = 1
TERRAIN_HILLS = 2
TERRAIN_MOUNTAINS = 3

LAND_GRASS = 0
LAND_FOREST = 1
LAND_FARM = 2
LAND_PASTURE = 3

# civ-style profile per terrain class:
#   interior_height: where the class settles deep inside (boundary is always 0.20)
#   peak_distance:   distance (px) at which interior height is reached
#   shape_exponent:  <1 plateau-like, >1 cone-like, 1 linear
EDGE_TARGET         = np.array([0.13, 0.13, 0.13, 0.17], dtype=np.float32)
TERRAIN_INTERIOR    = np.array([0.03, 0.13, 0.17, 0.85], dtype=np.float32)
TERRAIN_PEAK_DIST   = np.array([15.0, 1.0,  10.0, 25.0], dtype=np.float32)
TERRAIN_SHAPE_EXP   = np.array([0.7,  1.0,  0.4,  0.6],  dtype=np.float32)
TERRAIN_NOISE_AMP   = np.array([0.005, 0.005, 0.06, 0.30], dtype=np.float32)


def density_at(x, y, w, h, falloff):
  cx, cy = w * 0.5, h * 0.5
  dx = (x - cx) / (w * 0.5)
  dy = (y - cy) / (h * 0.5)
  r = np.sqrt(dx * dx + dy * dy)
  return np.exp(-r * falloff)


def sample_biased(rng, n, w, h, falloff):
  points = []
  while len(points) < n:
    batch = 4 * n
    cx = rng.uniform(0, w, batch)
    cy = rng.uniform(0, h, batch)
    keep = rng.uniform(0, 1, batch) < density_at(cx, cy, w, h, falloff)
    points.extend(zip(cx[keep], cy[keep]))
  return np.array(points[:n])


def compute_elevation_field(w, h, seed, detail_amplitude, edge_erosion):
  ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)

  xs_1d = np.arange(w, dtype=np.float64)
  ys_1d = np.arange(h, dtype=np.float64)

  n1 = OpenSimplex(seed=seed + 0).noise2array(xs_1d * 0.0012, ys_1d * 0.0012).astype(np.float32)
  n2 = OpenSimplex(seed=seed + 1).noise2array(xs_1d * 0.0035, ys_1d * 0.0035).astype(np.float32)
  n3 = OpenSimplex(seed=seed + 2).noise2array(xs_1d * 0.0080, ys_1d * 0.0080).astype(np.float32)
  n4 = OpenSimplex(seed=seed + 3).noise2array(xs_1d * 0.0200, ys_1d * 0.0200).astype(np.float32)
  n5 = OpenSimplex(seed=seed + 4).noise2array(xs_1d * 0.0600, ys_1d * 0.0600).astype(np.float32)

  e  = n1 * 0.15
  e += n2 * 0.25
  e += n3 * 0.35
  e += n4 * 0.18 * detail_amplitude
  e += n5 * 0.07 * detail_amplitude

  edge_x = np.abs(xs / w - 0.5) * 2.0
  edge_y = np.abs(ys / h - 0.5) * 2.0
  edge = np.maximum(edge_x, edge_y)
  e -= edge * edge_erosion

  return e


def select_lakes(rng, plains_ids, plains_centroids, plains_elevations, map_w, map_h, fraction, interior_bias):
  if fraction <= 0.0 or len(plains_ids) == 0:
    return np.array([], dtype=np.int64)

  cx, cy = map_w * 0.5, map_h * 0.5
  dx = (plains_centroids[:, 0] - cx) / (map_w * 0.5)
  dy = (plains_centroids[:, 1] - cy) / (map_h * 0.5)
  radial = np.sqrt(dx * dx + dy * dy)

  interior_weight = np.clip(1.0 - radial * radial, 0.0, 1.0) ** interior_bias
  elev_norm = 1.0 - (plains_elevations - plains_elevations.min()) / (np.ptp(plains_elevations) + 1e-12)
  weights = interior_weight * (0.3 + 0.7 * elev_norm)

  if weights.sum() <= 0.0:
    return np.array([], dtype=np.int64)

  weights = weights / weights.sum()

  n_target = max(1, int(len(plains_ids) * fraction))
  nonzero = int(np.sum(weights > 0))
  sample_count = min(len(plains_ids), max(1, nonzero))
  candidate_order = rng.choice(len(plains_ids), size=sample_count, replace=False, p=weights)

  avg_radius = np.sqrt(map_w * map_h / float(len(plains_ids))) * 0.5
  min_distance = avg_radius * 1.6

  selected = []
  selected_centroids = []

  for local_idx in candidate_order:
    centroid = plains_centroids[local_idx]
    too_close = any(np.linalg.norm(centroid - other) < min_distance for other in selected_centroids)
    if too_close:
      continue
    selected.append(plains_ids[local_idx])
    selected_centroids.append(centroid)
    if len(selected) >= n_target:
      break

  return np.array(selected, dtype=np.int64)


def generate_civ_style_heightmap(width, height, province_ids, terrain_class, seed):
  terrain_per_pixel = terrain_class[province_ids]

  # distance to nearest pixel of a STRICTLY LOWER tier
  # (lake=0, plains=1, hills=2, mountains=3 — higher index = higher elevation)
  distance_to_lower = np.zeros((height, width), dtype=np.float32)

  # for each tier > 0, distance to any pixel of tier strictly below it
  for tc in [1, 2, 3]:
    in_tier = terrain_per_pixel == tc
    if not in_tier.any():
      continue
    lower_mask = terrain_per_pixel < tc
    if not lower_mask.any():
      # no lower tier exists at all — assign maximum so shape stays at 1
      distance_to_lower[in_tier] = TERRAIN_PEAK_DIST[tc]
      continue
    dist = distance_transform_edt(~lower_mask)
    distance_to_lower[in_tier] = dist[in_tier].astype(np.float32)

  # lakes are inverted depressions: ramp from edge (plains height) down to interior
  # use distance to nearest non-lake
  in_lake = terrain_per_pixel == 0
  if in_lake.any():
    non_lake = terrain_per_pixel != 0
    if non_lake.any():
      dist = distance_transform_edt(~non_lake)
      distance_to_lower[in_lake] = dist[in_lake].astype(np.float32)

  interior  = TERRAIN_INTERIOR[terrain_per_pixel]
  edge      = EDGE_TARGET[terrain_per_pixel]
  peak_dist = TERRAIN_PEAK_DIST[terrain_per_pixel]
  exp       = TERRAIN_SHAPE_EXP[terrain_per_pixel]

  # surface noise — three octaves
  xs_1d = np.arange(width, dtype=np.float64)
  ys_1d = np.arange(height, dtype=np.float64)

  n_low  = OpenSimplex(seed=seed + 10).noise2array(xs_1d * 0.008, ys_1d * 0.008).astype(np.float32)
  n_mid  = OpenSimplex(seed=seed + 11).noise2array(xs_1d * 0.020, ys_1d * 0.020).astype(np.float32)
  n_high = OpenSimplex(seed=seed + 12).noise2array(xs_1d * 0.060, ys_1d * 0.060).astype(np.float32)

  # peak modulation — high enough frequency to vary WITHIN a mountain region.
  # combines two octaves so peaks aren't uniform in one direction.
  peak_mod_a = OpenSimplex(seed=seed + 13).noise2array(xs_1d * 0.018, ys_1d * 0.018).astype(np.float32)
  peak_mod_b = OpenSimplex(seed=seed + 15).noise2array(xs_1d * 0.045, ys_1d * 0.045).astype(np.float32)
  peak_mod = 0.85 + peak_mod_a * 0.45 + peak_mod_b * 0.20  # roughly [0.2, 1.5]

  # two ridge octaves — major spines and finer crags
  ridge_a = OpenSimplex(seed=seed + 14).noise2array(xs_1d * 0.025, ys_1d * 0.025).astype(np.float32)
  ridge_b = OpenSimplex(seed=seed + 16).noise2array(xs_1d * 0.080, ys_1d * 0.080).astype(np.float32)
  ridges = 2.0 * (0.5 - np.abs(ridge_a)) * 0.7 + 2.0 * (0.5 - np.abs(ridge_b)) * 0.3

  # local interior height varies per pixel — different parts of a range, different peaks.
  # only modulate uplift (delta > 0); depressions (lakes) keep their full negative delta.
  delta = interior - edge
  delta_modulated = np.where(delta > 0.0, delta * peak_mod, delta)
  local_interior = np.clip(edge + delta_modulated, 0.0, 1.0)

  t = np.clip(distance_to_lower / peak_dist, 0.0, 1.0)
  shape = np.power(t, exp)

  height_field = edge + shape * (local_interior - edge)

  # detail + ridge noise — multiply by shape so noise fades to zero at province boundaries
  noise = n_low * 0.25 + n_mid * 0.20 + n_high * 0.15 + ridges * 0.40
  noise_amp = TERRAIN_NOISE_AMP[terrain_per_pixel]
  height_field = height_field + noise * noise_amp * shape

  # tiny smoothing softens kinks at terrain boundaries
  height_field = gaussian_filter(height_field, sigma=1.0)

  return np.clip(height_field, 0.0, 1.0)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Voronoi world generator: civ-style heightmap + binary engine outputs")

  parser.add_argument("--output", type=str, required=True)
  parser.add_argument("--width", type=int, default=W)
  parser.add_argument("--height", type=int, default=H)
  parser.add_argument("--points", type=int, default=N)
  parser.add_argument("--falloff", type=float, default=FALLOFF)
  parser.add_argument("--relax-iterations", type=int, default=RELAX_ITERS)
  parser.add_argument("--seed", type=int, default=SEED)
  parser.add_argument("--hills-quantile", type=float, default=HILLS_QUANTILE)
  parser.add_argument("--mountains-quantile", type=float, default=MOUNTAINS_QUANTILE)
  parser.add_argument("--edge-erosion", type=float, default=EDGE_EROSION)
  parser.add_argument("--detail-amplitude", type=float, default=DETAIL_AMPLITUDE)
  parser.add_argument("--lake-fraction", type=float, default=LAKE_FRACTION)
  parser.add_argument("--lake-interior-bias", type=float, default=LAKE_INTERIOR_BIAS)

  args = parser.parse_args()

  output_dir = Path(args.output)
  output_dir.mkdir(parents=True, exist_ok=True)

  rng = np.random.default_rng(args.seed)

  #
  # Provinces
  #

  points = sample_biased(rng, args.points, args.width, args.height, args.falloff)

  ys, xs = np.mgrid[0:args.height, 0:args.width]
  pixels = np.stack([xs.ravel(), ys.ravel()], axis=1).astype(np.float32)

  weights = density_at(
    pixels[:, 0],
    pixels[:, 1],
    args.width,
    args.height,
    args.falloff,
  )

  for _ in range(args.relax_iterations):
    _, idx = cKDTree(points).query(pixels)

    new_x = np.zeros(args.points)
    new_y = np.zeros(args.points)
    wsum = np.zeros(args.points)

    np.add.at(new_x, idx, pixels[:, 0] * weights)
    np.add.at(new_y, idx, pixels[:, 1] * weights)
    np.add.at(wsum, idx, weights)

    points = np.column_stack([
      new_x / np.maximum(wsum, 1e-12),
      new_y / np.maximum(wsum, 1e-12),
    ])

  _, idx = cKDTree(points).query(pixels)
  province_ids = idx.reshape(args.height, args.width).astype(np.int64)

  #
  # Province color map (visual)
  #

  hues = (np.arange(args.points) * 0.618033988749) % 1.0

  province_colors = np.array([
    [int(255 * c) for c in colorsys.hsv_to_rgb(h, 0.7, 0.9)]
    for h in hues
  ], dtype=np.uint8)

  Image.fromarray(province_colors[province_ids], "RGB").save(output_dir / "provinces.png")

  #
  # Per-pixel elevation field (drives hills/mountains placement only)
  #

  elevation_field = compute_elevation_field(
    args.width,
    args.height,
    args.seed,
    args.detail_amplitude,
    args.edge_erosion,
  )

  flat_field = elevation_field.ravel()
  flat_pids = province_ids.ravel()

  elevation_sum = np.zeros(args.points, dtype=np.float64)
  pixel_counts = np.zeros(args.points, dtype=np.int64)

  np.add.at(elevation_sum, flat_pids, flat_field)
  np.add.at(pixel_counts, flat_pids, 1)

  elevations = (elevation_sum / np.maximum(pixel_counts, 1)).astype(np.float32)

  #
  # Terrain classification: all land, then sparse lakes
  #

  base = np.full(args.points, TERRAIN_PLAINS, dtype=np.uint8)

  p_hills     = np.quantile(elevations, args.hills_quantile)
  p_mountains = np.quantile(elevations, args.mountains_quantile)

  base[elevations >= p_hills]     = TERRAIN_HILLS
  base[elevations >= p_mountains] = TERRAIN_MOUNTAINS

  plains_ids = np.where(base == TERRAIN_PLAINS)[0]
  if len(plains_ids) > 0:
    lake_rng = np.random.default_rng(args.seed + 555)
    lake_ids = select_lakes(
      lake_rng,
      plains_ids,
      points[plains_ids],
      elevations[plains_ids],
      args.width,
      args.height,
      args.lake_fraction,
      args.lake_interior_bias,
    )
    base[lake_ids] = TERRAIN_OCEAN

  #
  # Moisture
  #

  moisture_gen_a = OpenSimplex(seed=args.seed + 100)
  moisture_gen_b = OpenSimplex(seed=args.seed + 200)

  moisture = np.zeros(args.points, dtype=np.float32)

  for i, (px, py) in enumerate(points):
    m = (moisture_gen_a.noise2(px * 0.002, py * 0.002) + 1.0) * 0.5
    m += (moisture_gen_b.noise2(px * 0.006, py * 0.006) + 1.0) * 0.5 * 0.4

    if base[i] == TERRAIN_OCEAN:
      m += 0.4
    if base[i] == TERRAIN_MOUNTAINS:
      m -= 0.25

    moisture[i] = m

  #
  # Land use
  #

  land_use = np.zeros(args.points, dtype=np.uint8)

  plains = base == TERRAIN_PLAINS
  land_use[plains & (moisture > 0.65)] = LAND_FOREST
  land_use[plains & (moisture > 0.40) & (moisture <= 0.65)] = LAND_FARM
  land_use[plains & (moisture > 0.25) & (moisture <= 0.40)] = LAND_PASTURE
  land_use[plains & (moisture <= 0.25)] = LAND_GRASS

  #
  # Render terrain (visual)
  #

  terrain_colors = np.array([
    [10, 30, 80],
    [70, 140, 70],
    [110, 110, 90],
    [220, 220, 220],
  ], dtype=np.uint8)

  land_colors = np.array([
    [120, 170, 90],
    [40, 120, 40],
    [210, 180, 80],
    [140, 170, 120],
  ], dtype=np.uint8)

  base_per_pixel = base[province_ids]
  land_use_per_pixel = land_use[province_ids]

  img = terrain_colors[base_per_pixel]
  plains_pixels = base_per_pixel == TERRAIN_PLAINS
  img[plains_pixels] = land_colors[land_use_per_pixel[plains_pixels]]

  Image.fromarray(img, "RGB").save(output_dir / "terrain_type.png")

  #
  # Civ-style heightmap
  #

  heightmap = generate_civ_style_heightmap(
    args.width,
    args.height,
    province_ids,
    base,
    args.seed,
  )

  # 8-bit PNG for visual debug
  Image.fromarray((heightmap * 255).astype(np.uint8), "L").save(output_dir / "heightmap.png")

  # 16-bit raw binary for engine
  heightmap_uint16 = (heightmap * 65535.0).astype(np.uint16)
  heightmap_uint16.tofile(output_dir / "heightmap.r16")

  #
  # Province IDs as raw uint32 binary for engine
  #

  province_ids_uint32 = province_ids.astype(np.uint32)
  province_ids_uint32.tofile(output_dir / "province_ids.r32u")

  #
  # Voronoi edges as line geometry for province borders.
  # Each edge is two endpoints in centered pixel coordinates (4 floats per edge).
  # The terrain renderer will lift them onto the heightmap surface in the vertex shader.
  #

  from scipy.spatial import Voronoi
  vor = Voronoi(points)

  edges_list = []

  # for infinite ridges we need a direction to extend the finite endpoint into
  seed_center = vor.points.mean(axis=0)
  far_distance = float(max(args.width, args.height)) * 2.0

  for ridge, seed_pair in zip(vor.ridge_vertices, vor.ridge_points):
    if -1 not in ridge:
      v1 = vor.vertices[ridge[0]]
      v2 = vor.vertices[ridge[1]]
      edges_list.append([v1[0], v1[1], v2[0], v2[1]])
      continue

    # infinite ridge: one vertex is at infinity, build a synthetic far endpoint
    finite_idx = ridge[0] if ridge[1] == -1 else ridge[1]
    finite_v = vor.vertices[finite_idx]

    tangent = vor.points[seed_pair[1]] - vor.points[seed_pair[0]]
    tangent_len = np.linalg.norm(tangent)
    if tangent_len < 1e-9:
      continue
    tangent /= tangent_len

    normal = np.array([-tangent[1], tangent[0]])
    midpoint = vor.points[seed_pair].mean(axis=0)
    direction = np.sign(np.dot(midpoint - seed_center, normal)) * normal

    far_v = finite_v + direction * far_distance

    edges_list.append([finite_v[0], finite_v[1], far_v[0], far_v[1]])

  edges_arr = np.array(edges_list, dtype=np.float32)

  # clip edges to map bounds (Liang-Barsky) so boundary edges aren't dropped
  if len(edges_arr) > 0:
    x1 = edges_arr[:, 0].copy()
    y1 = edges_arr[:, 1].copy()
    x2 = edges_arr[:, 2].copy()
    y2 = edges_arr[:, 3].copy()

    dx = x2 - x1
    dy = y2 - y1

    t_enter = np.zeros(len(edges_arr), dtype=np.float64)
    t_exit  = np.ones(len(edges_arr),  dtype=np.float64)
    fully_outside = np.zeros(len(edges_arr), dtype=bool)

    for p, q in [(-dx, x1 - 0.0), (dx, args.width  - x1),
                 (-dy, y1 - 0.0), (dy, args.height - y1)]:
      parallel = p == 0
      outside_parallel = parallel & (q < 0)
      fully_outside |= outside_parallel

      with np.errstate(divide="ignore", invalid="ignore"):
        t = np.where(parallel, 0.0, q / np.where(parallel, 1.0, p))

      entering = p < 0
      exiting  = p > 0

      t_enter = np.where(entering, np.maximum(t_enter, t), t_enter)
      t_exit  = np.where(exiting,  np.minimum(t_exit,  t), t_exit)

    valid = (~fully_outside) & (t_enter <= t_exit)

    nx1 = x1 + t_enter * dx
    ny1 = y1 + t_enter * dy
    nx2 = x1 + t_exit  * dx
    ny2 = y1 + t_exit  * dy

    edges_arr = np.stack([nx1, ny1, nx2, ny2], axis=1)[valid].astype(np.float32)

  # convert to centered pixel coordinates (origin at map center)
  edges_arr[:, 0] -= args.width * 0.5
  edges_arr[:, 1] -= args.height * 0.5
  edges_arr[:, 2] -= args.width * 0.5
  edges_arr[:, 3] -= args.height * 0.5

  edges_arr.tofile(output_dir / "borders.f32")
  edge_count = int(len(edges_arr))

  # visualization: white background, dark ink lines + dim seed points
  border_image = Image.new("RGB", (args.width, args.height), (245, 240, 230))
  draw = ImageDraw.Draw(border_image)

  half_w = args.width * 0.5
  half_h = args.height * 0.5

  for ex1, ey1, ex2, ey2 in edges_arr:
    draw.line(
      [(ex1 + half_w, ey1 + half_h), (ex2 + half_w, ey2 + half_h)],
      fill=(20, 18, 16),
      width=1,
    )

  for px, py in points:
    draw.ellipse(
      [(px - 1.5, py - 1.5), (px + 1.5, py + 1.5)],
      fill=(180, 60, 60),
    )

  border_image.save(output_dir / "borders.png")

  #
  # Sidecar metadata
  #

  provinces_meta = []
  for i in range(args.points):
    provinces_meta.append({
      "id": int(i),
      "color": province_colors[i].tolist(),
      "centroid": [float(points[i, 0]), float(points[i, 1])],
      "terrain": int(base[i]),
      "land_use": int(land_use[i]),
      "elevation": float(elevations[i]),
      "moisture": float(moisture[i]),
    })

  output_data = {
    "metadata": {
      "width": args.width,
      "height": args.height,
      "province_count": args.points,
      "heightmap_format": "r16_uint_le",
      "province_ids_format": "r32u_uint_le",
      "borders_format": "f32_le_quads_world_centered_pixels",
      "edge_count": edge_count,
    },
    "provinces": provinces_meta,
  }

  with open(output_dir / "provinces.yaml", "w") as f:
    yaml.safe_dump(output_data, f, sort_keys=False)

  n_lakes = int(np.sum(base == TERRAIN_OCEAN))
  n_plains = int(np.sum(base == TERRAIN_PLAINS))
  n_hills = int(np.sum(base == TERRAIN_HILLS))
  n_mountains = int(np.sum(base == TERRAIN_MOUNTAINS))

  print(f"Provinces: {args.points} (lakes={n_lakes}, plains={n_plains}, hills={n_hills}, mountains={n_mountains})")
  print(f"Generated: {output_dir / 'provinces.png'}        ({args.width}x{args.height})")
  print(f"Generated: {output_dir / 'terrain_type.png'}     ({args.width}x{args.height})")
  print(f"Generated: {output_dir / 'heightmap.png'}        (8-bit, visual)")
  print(f"Generated: {output_dir / 'heightmap.r16'}        ({args.width * args.height * 2} bytes, engine)")
  print(f"Generated: {output_dir / 'province_ids.r32u'}    ({args.width * args.height * 4} bytes, engine)")
  print(f"Generated: {output_dir / 'borders.f32'}          ({edge_count * 16} bytes, {edge_count} edges)")
  print(f"Generated: {output_dir / 'borders.png'}          ({args.width}x{args.height} visualization)")
  print(f"Generated: {output_dir / 'provinces.yaml'}       (metadata + per-province)")
  