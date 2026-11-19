import re
miss = set(l.strip() for l in open(r"D:\universal modder\trainz12-x64\notes\agents\RenderSystem_missing.txt", encoding="utf-8-sig") if l.strip())
path = r"D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c"
# For each marker, collect all names listed in its header comment block
addr_hits = {}
cur = None
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        m = re.match(r'^// ==== ([0-9a-fA-F]+)  (.+?)  \[.*?,\s*(\d+)\s+bytes\]', line)
        if m:
            cur = int(m.group(1),16)
            addr_hits.setdefault(cur, set())
            continue
        if cur is not None:
            # name entries: "                       0x21870  739  ?Begin@Layer@Jet@@MAAXXZ"
            mm = re.match(r'^\s+0x([0-9a-fA-F]+)\s+\d+\s+(\?\S+)', line)
            if mm:
                a = int(mm.group(1),16)
                addr_hits.setdefault(a, set()).add(mm.group(2))
                continue
            # end markers
            if line.startswith('/*') or line.startswith(' *') or line.startswith('*/') or line.startswith('void ') or line.startswith('bool ') or line.startswith('int ') or line.startswith('uint ') or line.startswith('undefined') or line.startswith('float') or line.startswith('double') or line.startswith('char') or line.startswith('uchar') or line.startswith('short') or line.startswith('ushort') or line.startswith('long') or line.startswith('ulong') or line.startswith('Layer') or line.startswith('Matrix') or line.startswith('Display') or line.startswith('PString') or line.startswith('IList') or line.startswith('DFormatId') or line.startswith('Color') or line.startswith('TextureAlpha'):
                # but some function sigs can appear; header is over once we leave indented name lines
                pass
            # stop if we hit a non-indented non-comment content line that's clearly code signature? keep simple: also check for '{'
            if not line.startswith('  ') and not line.startswith('\t') and not line.startswith('//') and not line.startswith('/*') and not line.startswith('*') and line.strip()!='':
                # function definition line - stop collecting for this marker block
                cur = None
            if line.strip()=='':
                # blank line could be separator inside header in some formats, but usually header continues
                pass

targets = [0x10021870,0x100015d0,0x10009500,0x1001dec0,0x10038c50,0x10004500,0x1000ae00,0x1000ae30,0x1000ae40,0x1000ae50,0x1000ae70,0x1000ae10,0x100106d0,0x10010940,0x1000b2b0,0x1000b2c0,0x1000d2d0,0x1000d300,0x1003ea80]
for va in targets:
    names = addr_hits.get(va, set())
    hit = sorted([n for n in names if n in miss])
    if hit:
        print('VA', hex(va), 'MISSING', len(hit))
        for n in hit:
            print(' ', n)
    else:
        print('VA', hex(va), 'MISSING 0 total', len(names))
