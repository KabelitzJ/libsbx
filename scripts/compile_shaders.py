#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
from pathlib import Path
from typing import Optional, List, Tuple, Dict
from concurrent.futures import ThreadPoolExecutor, as_completed
import itertools
import threading
import argparse

# ----------------------
# Configuration & Tables
# ----------------------

STAGES = ["vertex", "fragment", "compute", "geometry", "tesscontrol", "tesseval", "pixel"]

# Slang stage → (profile, entry)
SLANG_STAGE_INFO: Dict[str, Tuple[str, str]] = {
  "vertex":      ("vs_6_0", "main"),
  "fragment":    ("ps_6_0", "main"),
  "pixel":       ("ps_6_0", "main"),
  "compute":     ("cs_6_0", "main"),
  "geometry":    ("gs_6_0", "main"),
  "tesscontrol": ("hs_6_0", "main"),
  "tesseval":    ("ds_6_0", "main"),
}

# -------------
# Helper utils
# -------------

def which_or_none(name: str) -> Optional[str]:
  return shutil.which(name)


def print_badge(message: str, character: str = "=") -> None:
  print()
  print(character * len(message))
  print(message)
  print(character * len(message))
  print()


def run_cmd(cmd: List[str]) -> bool:
  try:
    subprocess.run(cmd, check=True)
    return True
  except FileNotFoundError:
    # Tool missing; discovery handles messaging, treat as failure here.
    return False
  except subprocess.CalledProcessError as e:
    print(f"[ERROR] Command failed ({e.returncode}): {' '.join(cmd)}")
    return False


# ------------------------
# Per-file compile actions
# ------------------------

_print_lock = threading.Lock()

def _safe_print(*args, **kwargs):
  with _print_lock:
    print(*args, **kwargs)


def build_glsl_job(glslc_path: str, file: Path, shader_root_dir: Path) -> Optional[Tuple[str, Path, List[str]]]:
  """Return a job tuple (kind, out_path, cmd) for a GLSL file, or None if stage unknown."""
  stage = file.stem  # e.g., vertex.glsl → "vertex"
  if stage not in STAGES:
    return None

  binary_dir = file.parent / "bin"
  binary_dir.mkdir(parents=True, exist_ok=True)

  output = binary_dir / f"{stage}.spv"

  cmd = [
    glslc_path,
    f"-fshader-stage={stage}",
    "-c", str(file),
    "-o", str(output),
    "--target-env=vulkan1.4",
    "--target-spv=spv1.5",
    "-std=460core",
    f"-I{shader_root_dir}",
  ]
  return ("glsl", output, cmd)


def build_slang_job(slangc_path: str, file: Path, shader_root_dir: Path) -> Optional[Tuple[str, Path, List[str], Optional[Path]]]:
  """
  Return a job tuple (kind, out_path, cmd, rename_to)
  If stage is 'pixel', out_path is pixel.spv and rename_to is fragment.spv
  """
  stage = file.stem  # e.g., pixel.slang → "pixel"
  info = SLANG_STAGE_INFO.get(stage)
  if not info:
    return None

  profile, entry = info

  binary_dir = file.parent / "bin"
  binary_dir.mkdir(parents=True, exist_ok=True)

  if stage == "fragment":
    out_spv = binary_dir / "fragment.spv"
    rename_to = None
  elif stage == "pixel":
    out_spv = binary_dir / "pixel.spv"
    rename_to = binary_dir / "fragment.spv"
  else:
    out_spv = binary_dir / f"{stage}.spv"
    rename_to = None

  cmd = [
    slangc_path,
    str(file),
    "-target", "spirv",
    "-profile", profile,
    "-entry", entry,
    # "-capability", "spirv_1_5",
    # "-capability", "spirv",
    "-capability", "SPV_EXT_physical_storage_buffer",
    "-matrix-layout-column-major",
    # "-g",
    # "-O0",
    # "-minimum-slang-optimization",
    "-o", str(out_spv),
    f"-I{shader_root_dir}",
  ]

  return ("slang", out_spv, cmd, rename_to)


def execute_job(job) -> bool:
  """Execute a prepared job tuple; return True if something was built."""
  if job[0] == "glsl":
    kind, out_path, cmd = job
    if run_cmd(cmd):
      _safe_print(f"[OK] GLSL → {out_path}")
      return True
    return False

  if job[0] == "slang":
    kind, out_path, cmd, rename_to = job
    if not run_cmd(cmd):
      return False
    if rename_to is not None:
      # Move pixel.spv → fragment.spv atomically
      os.replace(out_path, rename_to)
      _safe_print(f"[OK] SLANG → {rename_to} (from {out_path.name})")
    else:
      _safe_print(f"[OK] SLANG → {out_path}")
    return True

  return False


# ----------------------
# Discovery & Orchestration
# ----------------------

def gather_jobs(shader_root_dir: Path, glslc_path: Optional[str], slangc_path: Optional[str]) -> List[tuple]:
  jobs = []

  for shader_dir in sorted(p for p in shader_root_dir.iterdir() if p.is_dir()):
    if shader_dir.name == "libsbx" or shader_dir.name == "deferred_pbr_material" or shader_dir.name == "depthpre"  or shader_dir.name == "shadow":
      continue

    # Slang first (sorted for determinism when both pixel/fragment exist)
    if slangc_path is not None:
      slang_files = sorted(shader_dir.glob("*.slang"), key=lambda p: p.name)
      if slang_files:
        _safe_print(f"Compiling SLANG shaders: {shader_dir}")
      for f in slang_files:
        job = build_slang_job(slangc_path, f, shader_root_dir)
        if job is not None:
          jobs.append(job)
    else:
      slang_files = list(shader_dir.glob("*.slang"))
      if slang_files:
        _safe_print(f"[WARN] Found Slang files in {shader_dir} but slangc not found — skipping these:")
        for f in slang_files:
          _safe_print(f"  - {f.name}")

    # GLSL
    if glslc_path is not None:
      glsl_files = sorted(shader_dir.glob("*.glsl"), key=lambda p: p.name)
      if glsl_files:
        _safe_print(f"Compiling GLSL shaders: {shader_dir}")
      for f in glsl_files:
        job = build_glsl_job(glslc_path, f, shader_root_dir)
        if job is not None:
          jobs.append(job)
    else:
      glsl_files = list(shader_dir.glob("*.glsl"))
      if glsl_files:
        _safe_print(f"[WARN] Found GLSL files in {shader_dir} but glslc not found — skipping these:")
        for f in glsl_files:
          _safe_print(f"  - {f.name}")

  return jobs


def main(shader_root: str, jobs_arg: Optional[int] = None) -> int:
  shader_root_dir = Path(shader_root).resolve()

  glslc_path = which_or_none("glslc")
  slangc_path = which_or_none("slangc")

  if glslc_path is None and slangc_path is None:
    print("[ERROR] Neither 'glslc' nor 'slangc' found in PATH. Install at least one.")
    return 2

  if glslc_path is None:
    print("[WARN] glslc not found — GLSL (*.glsl) files will be skipped.")

  if slangc_path is None:
    print("[WARN] slangc not found — Slang (*.slang) files will be skipped.")

  print_badge(f"Compiling shaders in {shader_root_dir}")

  jobs = gather_jobs(shader_root_dir, glslc_path, slangc_path)

  if not jobs:
    print("Done. Built 0 shader stage(s).")
    return 1

  # Resolve parallelism: CLI --jobs, else env SHADER_JOBS, else CPU heuristic
  if jobs_arg is not None and jobs_arg > 0:
    max_workers = jobs_arg
  else:
    env_jobs = os.getenv("SHADER_JOBS")
    if env_jobs and env_jobs.isdigit() and int(env_jobs) > 0:
      max_workers = int(env_jobs)
    else:
      # A slight over-subscription helps hide process startup/IO latency
      cpu = os.cpu_count() or 4
      max_workers = max(2, min(32, cpu + 2))

  total_built = 0

  with ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix="shader") as ex:
    future_map = {ex.submit(execute_job, j): j for j in jobs}
    for fut in as_completed(future_map):
      ok = False
      try:
        ok = fut.result()
      except Exception as e:
        _safe_print(f"[ERROR] Unexpected: {e}")
        ok = False
      if ok:
        total_built += 1

  print(f"Done. Built {total_built} shader stage(s).")
  return 0 if total_built > 0 else 1


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Compile GLSL and Slang shaders in parallel.")
  parser.add_argument("shader_root_dir", help="Root directory containing per-shader subfolders")
  parser.add_argument("--jobs", "-j", type=int, default=None, help="Number of parallel jobs (default: CPU+2, capped at 32). Alternatively use SHADER_JOBS env var.")

  args = parser.parse_args()
  sys.exit(main(args.shader_root_dir, args.jobs))
