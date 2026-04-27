#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
from pathlib import Path
from typing import Optional, List, Tuple, Dict
from concurrent.futures import ThreadPoolExecutor, as_completed
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


def build_slang_job(slangc_path: str, file: Path, shader_root_dir: Path, include_dirs: List[Path]) -> Optional[Tuple[str, Path, List[str], Optional[Path]]]:
    """
    Return a job tuple (kind, out_path, cmd, rename_to)
    If stage is 'pixel', out_path is pixel.spv and rename_to is fragment.spv
    """
    stage = file.stem  # e.g., pixel.slang -> "pixel"
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
        "-capability", "SPV_EXT_physical_storage_buffer",
        "-matrix-layout-column-major",
        "-g",
        "-O0",
        "-o", str(out_spv),
        f"-I{shader_root_dir}",
        f"-I{file.parent}",
    ]

    for include_dir in include_dirs:
        cmd.append(f"-I{include_dir}")

    return ("slang", out_spv, cmd, rename_to)


def execute_job(job: Tuple[str, Path, List[str], Optional[Path]]) -> bool:
    """Execute a prepared job tuple; return True if something was built."""
    kind, out_path, cmd, rename_to = job

    if not run_cmd(cmd):
        return False

    if rename_to is not None:
        os.replace(out_path, rename_to)
        _safe_print(f"[OK] {rename_to} (from {out_path.name})")
    else:
        _safe_print(f"[OK] {out_path}")

    return True


# ----------------------
# Manifest parsing
# ----------------------

def read_manifest(manifest_path: Path) -> List[str]:
    """Read a manifest file, one shader directory entry per line.
    Lines starting with '#' and empty lines are ignored.
    Inline '#' comments are stripped."""
    entries = []

    with manifest_path.open("r", encoding="utf-8") as fp:
        for raw_line in fp:
            line = raw_line.split("#", 1)[0].strip()

            if not line:
                continue

            entries.append(line)

    return entries


# ----------------------
# Discovery & Orchestration
# ----------------------

def gather_jobs(shader_root_dir: Path, shader_entries: List[str], slangc_path: str, include_dirs: List[Path]) -> List[Tuple[str, Path, List[str], Optional[Path]]]:
    jobs = []
    seen_dirs = set()

    for entry in shader_entries:
        shader_dir = (shader_root_dir / entry).resolve()

        if not shader_dir.is_dir():
            _safe_print(f"[WARN] Shader entry '{entry}' is not a directory under '{shader_root_dir}', skipping")
            continue

        if shader_dir in seen_dirs:
            continue

        seen_dirs.add(shader_dir)

        rel_path = shader_dir.relative_to(shader_root_dir)
        _safe_print(f"Compiling shaders: {rel_path}")

        stage_files = sorted([p for p in shader_dir.iterdir() if p.is_file() and p.suffix == ".slang" and p.stem in STAGES], key=lambda p: str(p))

        if not stage_files:
            _safe_print(f"[WARN] No stage files found in '{shader_dir}'")
            continue

        for f in stage_files:
            job = build_slang_job(slangc_path, f, shader_root_dir, include_dirs)

            if job is not None:
                jobs.append(job)

    return jobs


def main(shader_root: str, shader_entries: List[str], include_dirs: List[Path], jobs_arg: Optional[int] = None) -> int:
    shader_root_dir = Path(shader_root).resolve()

    slangc_path = which_or_none("slangc")

    if slangc_path is None:
        print("[ERROR] 'slangc' not found in PATH.")
        return 2

    if not shader_entries:
        print("[ERROR] No shaders specified. Use --manifest or --shader.")
        return 2

    print_badge(f"Compiling shaders in {shader_root_dir}")

    jobs = gather_jobs(shader_root_dir, shader_entries, slangc_path, include_dirs)

    if not jobs:
        print("Done. Built 0 shader stage(s).")
        return 0

    # Resolve parallelism: CLI --jobs, else env SHADER_JOBS, else CPU heuristic
    if jobs_arg is not None and jobs_arg > 0:
        max_workers = jobs_arg
    else:
        env_jobs = os.getenv("SHADER_JOBS")
        if env_jobs and env_jobs.isdigit() and int(env_jobs) > 0:
            max_workers = int(env_jobs)
        else:
            cpu = os.cpu_count() or 4
            max_workers = max(2, min(32, cpu + 2))

    total_built = 0

    with ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix="shader") as ex:
        future_map = {ex.submit(execute_job, j): j for j in jobs}
        for fut in as_completed(future_map):
            try:
                if fut.result():
                    total_built += 1
            except Exception as e:
                _safe_print(f"[ERROR] Unexpected: {e}")

    print(f"Done. Built {total_built} shader stage(s).")
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compile Slang shaders in parallel.")
    parser.add_argument("shader_root_dir", help="Root directory containing per-shader subfolders")
    parser.add_argument("--manifest", "-m", type=Path, default=None,
                        help="Path to a text file listing shader directories (one entry per line, relative to shader_root_dir). Lines starting with '#' are comments.")
    parser.add_argument("--shader", "-s", action="append", default=[],
                        help="Shader directory to compile (relative to shader_root_dir). Repeatable. Combines with --manifest.")
    parser.add_argument("--include", "-I", action="append", default=[], type=Path,
                        help="Additional include directory passed to slangc as '-I'. Repeatable.")
    parser.add_argument("--jobs", "-j", type=int, default=None,
                        help="Number of parallel jobs (default: CPU+2, capped at 32). Alternatively use SHADER_JOBS env var.")

    args = parser.parse_args()

    entries: List[str] = []

    if args.manifest is not None:
        if not args.manifest.is_file():
            print(f"[ERROR] Manifest file '{args.manifest}' does not exist.")
            sys.exit(2)

        entries.extend(read_manifest(args.manifest))

    entries.extend(args.shader)

    # Deduplicate while preserving order
    seen = set()
    unique_entries = []

    for e in entries:
        if e not in seen:
            seen.add(e)
            unique_entries.append(e)

    include_dirs = [d.resolve() for d in args.include]

    sys.exit(main(args.shader_root_dir, unique_entries, include_dirs, args.jobs))
