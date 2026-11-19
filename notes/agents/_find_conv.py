import re
path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
# get all Convert-related helper functions: 0xde80,0xded0,0xdee0,0xdef0,0xdf40,0xdf90,0xdfa0,0xdff0,0xe040,0xe090,0xe0a0,0xe0f0,0xe140,0xe1a0,0xe200,0xe260,0xe2c0,0xe310,0xe360,0xe3b0,0xe3f0,0xe430,0xe490,0xe4f0,0xe550,0xe590,0xe5d0,0xef00 etc.
# scan for markers in range 0x1000de00-0x1000f000 and also LAB addresses mentioned
targets=set()
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    lines=f.readlines()
# find marker addresses in range
for i,line in enumerate(lines):
    m=re.match(r'^// ==== ([0-9a-fA-F]+)  (.*?)  \[.*?,\s*(\d+)\s+bytes\]', line)
    if m:
        addr=int(m.group(1),16)
        if 0x1000de80 <= addr <= 0x1000f100:
            targets.add(addr)
# also need to capture functions not having markers as "FUN_xxxx" in decomp maybe? markers exist for most.
print(sorted(hex(t) for t in targets))
