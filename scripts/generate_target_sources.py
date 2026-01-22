#!/usr/bin/env python3
import sys
from pathlib import Path

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <module_name>")
    sys.exit(1)

module_name = sys.argv[1]

# scripts/ → repo root
script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent

# libsbx-<module>/libsbx/<module>
module_root = repo_root / f"libsbx-{module_name}" / "libsbx" / module_name

if not module_root.exists():
    print(f"Error: module directory not found:\n  {module_root}")
    sys.exit(1)

cmake_var = "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}/${PROJECT_NAME}"

private_exts = {".cpp", ".ipp"}
public_exts = {".hpp"}

private_files = []
public_files = []

for path in sorted(module_root.rglob("*")):
    if not path.is_file():
        continue

    rel = path.relative_to(module_root)

    if path.suffix in private_exts:
        private_files.append(rel)
    elif path.suffix in public_exts:
        public_files.append(rel)

def emit(files, indent="    "):
    return "\n".join(
        f'{indent}"{cmake_var}/{f.as_posix()}"'
        for f in files
    )

print("target_sources(")
print("  ${PROJECT_NAME}")
print("  PRIVATE")
print(emit(private_files, indent="    "))
print("  PUBLIC")
print("    FILE_SET HEADERS")
print("    BASE_DIRS ${PROJECT_SOURCE_DIR}")
print("    FILES")
print(emit(public_files, indent="      "))
print(")")
