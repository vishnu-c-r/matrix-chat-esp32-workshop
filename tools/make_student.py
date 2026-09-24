#!/usr/bin/env python3
"""
tools/make_student.py
---------------------
Generates the student skeleton from the reference solution.

Usage
-----
  python tools/make_student.py [--out student/]

What it does
------------
1. Copies the entire project tree to <out>/ (excluding .pio, .git, student/).
2. In every .cpp and .h file it finds SOLUTION-BEGIN / SOLUTION-END blocks
   and replaces them with a single TODO comment.

Block format in the source
--------------------------
    // SOLUTION-BEGIN stage:N hint:"what to do"
    ... reference code ...
    // SOLUTION-END

Produces in the skeleton
------------------------
    // TODO(stageN): what to do

Stage guide
-----------
  Stage 1 (~15 min) — Call ledInit() to light up your node colour.
  Stage 2 (~40 min) — Build the Pkt fields and handle incoming chat.
  Stage 3 (~40 min) — Link RSSI filter + Proximity indicator.
"""

import re
import os
import sys
import shutil
import argparse

# Matches a complete SOLUTION block (possibly indented).
BLOCK_RE = re.compile(
    r'(?m)^([ \t]*)//\s*SOLUTION-BEGIN\s+stage:(\d+)'
    r'(?:\s+hint:"([^"]*)")?[ \t]*\r?\n'
    r'.*?'
    r'^[ \t]*//\s*SOLUTION-END[ \t]*\r?\n',
    re.DOTALL,
)


def strip_solutions(content: str) -> str:
    """Replace every SOLUTION block with a TODO comment."""

    def replacer(m: re.Match) -> str:
        indent = m.group(1)
        stage  = m.group(2)
        hint   = m.group(3) or "implement this stage"
        return f"{indent}// TODO(stage{stage}): {hint}\n"

    return BLOCK_RE.sub(replacer, content)


def sanitize_platformio_ini(content: str) -> str:
    """Remove any [env:*smith*] environments and comments from platformio.ini."""
    lines = content.splitlines()
    out = []
    skipping = False
    for line in lines:
        if line.strip().startswith('[') and line.strip().endswith(']'):
            if 'smith' in line.lower():
                skipping = True
            else:
                skipping = False
        elif skipping and line.strip().startswith('['):
            skipping = False

        if not skipping:
            if 'smith' not in line.lower():
                out.append(line)
    return '\n'.join(out) + '\n'


def copy_project(src_root: str, dst_root: str) -> None:
    skip_dirs  = {'.pio', '.git', '__pycache__', os.path.basename(dst_root), 'smith', 'audio', '.vscode', '.cache', 'test_corrupt', 'tools'}
    skip_files = {'INSTRUCTOR.md', 'presentation_smith.html', 'compile_commands.json'}
    code_exts  = {'.cpp', '.h', '.c', '.py'}

    for dirpath, dirnames, filenames in os.walk(src_root):
        # Prune dirs we don't want to recurse into.
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]

        rel_dir = os.path.relpath(dirpath, src_root)
        dst_dir = os.path.join(dst_root, rel_dir)
        os.makedirs(dst_dir, exist_ok=True)

        for fname in filenames:
            if fname in skip_files or 'smith' in fname.lower():
                continue

            src_path = os.path.join(dirpath, fname)
            dst_path = os.path.join(dst_dir, fname)

            _, ext = os.path.splitext(fname)
            if fname == 'platformio.ini':
                with open(src_path, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
                sanitized = sanitize_platformio_ini(content)
                with open(dst_path, 'w', encoding='utf-8') as f:
                    f.write(sanitized)
                print(f"  [sanitized] {rel_dir}/{fname}")
            elif ext in code_exts:
                with open(src_path, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
                skeleton = strip_solutions(content)
                with open(dst_path, 'w', encoding='utf-8') as f:
                    f.write(skeleton)
                changed = skeleton != content
                print(f"  {'[stripped]' if changed else '[copied  ]'} {rel_dir}/{fname}")
            else:
                shutil.copy2(src_path, dst_path)
                print(f"  [binary  ] {rel_dir}/{fname}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate student skeleton.")
    parser.add_argument('--out', default='student',
                        help="Output directory (default: student/)")
    parser.add_argument('--force', action='store_true',
                        help="Overwrite existing output directory without prompting")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    out_dir = os.path.join(project_root, args.out)

    if os.path.exists(out_dir):
        if not args.force:
            # Non-interactive default or automated script check
            pass
        shutil.rmtree(out_dir)

    print(f"\nGenerating student skeleton -> {out_dir}/\n")
    copy_project(project_root, out_dir)

    print(f"""
Done!  Student skeleton written to: {out_dir}/

Students edit ONLY: include/config.h
  Change NODE_ID  (1-50, unique per group)
  Change NODE_NAME (your team handle)

Stage guide
-----------
  Stage 1 (~15 min) LED colour cycle — wire check
  Stage 2 (~40 min) Send & receive chat messages
  Stage 3 (~40 min) Link RSSI filter + Proximity indicator

Flash commands (replace COMx with your port):
  Node  (C3):    pio run -e esp32c3_node -t upload --upload-port COMx
  Node  (S3):    pio run -e esp32s3_node -t upload --upload-port COMx
  Node  (WROOM): pio run -e esp32dev_node -t upload --upload-port COMx
""")


if __name__ == '__main__':
    main()
