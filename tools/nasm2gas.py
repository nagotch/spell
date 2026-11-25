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
# レジスタ名は間接 call のオペランドであって外部シンボルではない
_regs = {'rsp', 'rbp', 'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi'}
_regs |= {f'r{i}' for i in range(8, 16)}
called -= _regs
called -= {f'r{i}{sfx}' for i in range(8, 16) for sfx in ('d', 'w', 'b')}
called -= {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp'}
externs = sorted(called - defined)

out = ['default rel']
out += [f'extern {name}' for name in externs]
for line in src.splitlines():
    if line.startswith('.intel_syntax') or line.startswith('.section'):
        continue
    line = re.sub(r'^\.global\s+', 'global ', line)
    line = line.replace('movzb', 'movzx')
    # GAS の `qword ptr [...]` は NASM では ptr を付けない
    line = re.sub(r'\b(qword|dword|word|byte) ptr ', r'\1 ', line)
    # GAS のセクション指定と .bss の領域確保を NASM 形式へ
    if line == '.bss':
        line = 'section .bss'
    elif line == '.text':
        line = 'section .text'
    else:
        line = re.sub(r'^(\s*)\.zero\s+(\d+)\s*$', r'\1resb \2', line)
    # GAS の [rip + x] は NASM では default rel 前提で [x] と書く
    line = re.sub(r'\[rip \+ ([^\]]+)\]', r'[\1]', line)
    m = re.match(r'^(\s*call\s+)([A-Za-z_][\w]*)\s*$', line)
    if m and m.group(2) in externs:
        line = f'{m.group(1)}{m.group(2)} wrt ..plt'
    out.append(line)
out.append('section .note.GNU-stack noalloc noexec nowrite progbits')
print('\n'.join(header + out))
