#!/usr/bin/env python3
import re, sys

header = ["; Original spell source: https://github.com/nagotch/spell"]
for path in sys.argv[1:]:
    try:
        with open(path) as f:
            lines = f.read().splitlines()
    except OSError:
        print(f"cannot read {path}", file=sys.stderr)
        sys.exit(1)
    header.append(f"; --- {path} ---")
    for line in lines:
        header.append("; " + line);

src = sys.stdin.read()
defined = set(re.findall(r'^([A-Za-z_][\w]*):', src, re.M))
called = set(re.findall(r'call\s+([A-Za-z_][\w]*)', src))
externs = sorted(called - defined)

out = [f'extern {name}' for name in externs]
for line in src.splitlines():
    if line.startswith('.intel_syntax') or line.startswith('.section'):
        continue
    line = re.sub(r'^\.global\s+', 'global ', line)
    line = line.replace('movzb', 'movzx')
    m = re.match(r'^(\s*call\s+)([A-Za-z_][\w]*)\s*$', line)
    if m and m.group(2) in externs:
        line = f'{m.group(1)}{m.group(2)} wrt ..plt'
    out.append(line)
print('\n'.join(header + out))
