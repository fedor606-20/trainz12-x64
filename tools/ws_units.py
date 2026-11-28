import re
exports=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
impl=open(r'src/jet/graphics/WaterSystem_7c/implemented.txt',encoding='utf-8').read().splitlines()
local=set(l.split('=')[0].strip() for l in impl if l.strip() and not l.startswith('#'))
rem=[e for e in exports if e[2] not in local]
B=0x10000000
units={
'U7b':(B+0x3530,B+0x3700),'U8bump':(B+0x5710,B+0x6b20),'U8fres':(B+0xd290,B+0x11930),
'U8one':(B+0x13040,B+0x13800),'U8ocean':(B+0x13fd0,B+0x15410),
'U9':(B+0x3700,B+0x3f20),'U9b':(B+0x15c10,B+0x15d00),'U9c':(B+0x171a0,B+0x17810),
'U10':(B+0x92a0,B+0xcb90),'entry':(B+0x162f0,B+0x162f4)}
for u,(a,b) in units.items():
    ea=[e for e in rem if a<=B+int(e[1],16)<b]
    print(u, 'exports',len(ea),'unique addrs',len(set(e[1] for e in ea)))
other=[e for e in rem if not any(a<=B+int(e[1],16)<b for a,b in units.values())]
print('other',len(other))
for e in other: print(e[0],e[1],e[2][:90])
