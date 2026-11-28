import pefile, struct
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase
def rd(va,n): return pe.get_data(va-base,n)
def u32(va): return struct.unpack('<I',rd(va,4))[0]
tsv=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
for e in tsv:
    d=e[3]
    if 'PString const Jet::' in d or 'UId const Jet::' in d or 'static int Jet::' in d:
        va=base+int(e[1],16); raw=rd(va,16); extra=''
        if 'PString' in d:
            node=u32(va); extra=f' node={node:08x}'
            if node:
                for off in (0,4,8,12,16):
                    s=rd(node+off,80).split(b'\0')[0]
                    if len(s)>=1 and all(32<=c<127 for c in s):
                        extra+=f' str@{off}={s.decode()!r}'
        elif 'UId' in d:
            extra=' bytes='+raw[:16].hex()
        else: extra=f' int={u32(va)}'
        print(e[1], d.split('Jet::')[-1][:55], extra)
