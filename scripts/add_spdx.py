#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import fnmatch

SPDX_LINE_DEFAULT = "// SPDX-License-Identifier: MIT"


def detect_newline(raw: bytes) -> str:
    return "\r\n" if b"\r\n" in raw else "\n"


def read_text_preserve_bom(path: Path):
    raw = path.read_bytes()

    bom = b"\xef\xbb\xbf" if raw.startswith(b"\xef\xbb\xbf") else b""
    newline = detect_newline(raw)

    try:
        text = raw.decode("utf-8-sig")
        encoding = "utf-8"
    except UnicodeDecodeError:
        text = raw.decode("latin-1")
        encoding = "latin-1"

    return text, bom, newline, encoding


def has_spdx_in_header(lines, max_lines=25) -> bool:
    for line in lines[:max_lines]:
        if line.strip().startswith("// SPDX-License-Identifier:"):
            return True
    return False


def should_ignore(path: Path, root: Path, ignore_paths, ignore_patterns) -> bool:
    try:
        rel = path.relative_to(root).as_posix()
    except ValueError:
        return False

    for ip in ignore_paths:
        if path == ip or ip in path.parents:
            return True

    for pat in ignore_patterns:
        if fnmatch.fnmatch(rel, pat):
            return True

    return False


def add_spdx_to_file(path: Path, spdx_line: str, dry_run: bool, replace_existing: bool) -> bool:
    text, bom, newline, encoding = read_text_preserve_bom(path)
    lines = text.splitlines(keepends=True)

    if not lines:
        new_text = spdx_line + newline
        if not dry_run:
            path.write_bytes(bom + new_text.encode(encoding))
        return True

    if has_spdx_in_header(lines):
        if not replace_existing:
            return False

        for i in range(min(25, len(lines))):
            if lines[i].strip().startswith("// SPDX-License-Identifier:"):
                line_end = "\n" if lines[i].endswith("\n") else newline
                lines[i] = spdx_line + line_end
                if not dry_run:
                    path.write_bytes(bom + "".join(lines).encode(encoding))
                return True
        return False

    insert_at = 1 if lines[0].startswith("#!") else 0
    lines.insert(insert_at, spdx_line + newline)

    if not dry_run:
        path.write_bytes(bom + "".join(lines).encode(encoding))
    return True


def iter_target_files(root: Path, exts, ignore_paths, ignore_patterns):
    for dirpath, dirnames, filenames in os.walk(root):
        dirpath = Path(dirpath)

        # Prune ignored directories
        dirnames[:] = [
            d for d in dirnames
            if not should_ignore(dirpath / d, root, ignore_paths, ignore_patterns)
        ]

        for name in filenames:
            path = dirpath / name
            if path.suffix.lower() in exts and not should_ignore(
                path, root, ignore_paths, ignore_patterns
            ):
                yield path


def main():
    parser = argparse.ArgumentParser(
        description="Recursively add SPDX header to .hpp/.cpp/.ipp files."
    )
    parser.add_argument("root", nargs="?", default=".")
    parser.add_argument("--spdx", default=SPDX_LINE_DEFAULT)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--replace-existing", action="store_true")

    parser.add_argument(
        "--ext",
        action="append",
        default=[".hpp", ".cpp", ".ipp"],
        help="File extension to include (repeatable).",
    )

    parser.add_argument(
        "--ignore",
        action="append",
        default=[],
        help="Path to ignore (file or directory, repeatable).",
    )

    parser.add_argument(
        "--ignore-pattern",
        action="append",
        default=[],
        help="Glob pattern to ignore (repeatable, matched against relative path).",
    )

    args = parser.parse_args()

    root = Path(args.root).resolve()
    exts = {e if e.startswith(".") else "." + e for e in args.ext}

    ignore_paths = [(root / p).resolve() for p in args.ignore]

    scanned = 0
    changed = 0

    for path in iter_target_files(root, exts, ignore_paths, args.ignore_pattern):
        scanned += 1
        if add_spdx_to_file(
            path, args.spdx, args.dry_run, args.replace_existing
        ):
            changed += 1
            print(f"[CHANGED] {path}")
        else:
            print(f"[OK]      {path}")

    print(f"\nScanned: {scanned}")
    print(f"Changed: {changed}")
    if args.dry_run:
        print("Dry-run mode enabled.")


if __name__ == "__main__":
    main()
