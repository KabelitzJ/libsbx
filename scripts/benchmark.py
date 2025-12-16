import json
import subprocess
from pathlib import Path
import tempfile
import os

BIN_DIR = Path(r"./build/x86_64/gcc/release/bin")

def run_benchmark(exe_path: Path):
  print(f"Running {exe_path.name}...")

  with tempfile.NamedTemporaryFile(delete=False, suffix=".json") as tmp:
    json_path = Path(tmp.name)

  try:
    # Run benchmark and write JSON output
    with open(json_path, "w") as f:
      subprocess.run(
        [str(exe_path), "--benchmark_format=json"],
        stdout=f,
        check=True
      )

    # Parse JSON
    with open(json_path, "r") as f:
      data = json.load(f)

    benchmarks = data.get("benchmarks", [])

    # Collect baseline (std)
    baseline = {}
    for b in benchmarks:
      name = b["name"]
      if "std" in name:
        baseline[name] = b["cpu_time"]

    # Compute speedups
    for b in benchmarks:
      name = b["name"]
      if "local" in name:
        std_name = name.replace("local", "std")
        if std_name in baseline:
          speedup = baseline[std_name] / b["cpu_time"]
          print(f"{name:<60} {speedup:6.2f}x")

  finally:
    os.remove(json_path)
    print()

def main():
  exes = sorted(BIN_DIR.glob("*-benchmarks.exe"))
  if not exes:
    print("No benchmark executables found.")
    return

  for exe in exes:
    run_benchmark(exe)

if __name__ == "__main__":
  main()
