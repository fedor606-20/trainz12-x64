"""Merge a module's implemented.txt with its implemented.d/*.txt fragments.

The main list stays the module's starting point; each parallel workstream writes its own fragment so two agents
never edit the same file. Duplicate mangled names are rejected (two units claiming one export is a merge bug).

usage: merge_implemented.py <implemented.txt> <out file> [fragment ...]
"""
import sys
from pathlib import Path

base, out = Path(sys.argv[1]), Path(sys.argv[2])
fragments = [Path(p) for p in sys.argv[3:]]

seen = {}
lines = []
for path in [base] + sorted(fragments, key=lambda p: p.name):
    if not path.exists():
        continue
    for line in path.read_text(encoding="utf-8").splitlines():
        name = line.split("#", 1)[0].strip()
        if not name:
            continue
        mangled = name.split("=", 1)[0]
        if mangled in seen:
            sys.exit(f"duplicate implemented export {mangled}: {seen[mangled]} and {path}")
        seen[mangled] = str(path)
        lines.append(line)

out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"{out}: {len(lines)} implemented exports from {1 + len(fragments)} file(s)")
