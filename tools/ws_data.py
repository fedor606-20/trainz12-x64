import pefile, struct, re
pe=pefile.PE(r'D:\trainz12-decomp\bin\WaterSystem_7c.dll',fast_load=True); base=pe.OPTIONAL_HEADER.ImageBase
def rd(va,n): return pe.get_data(va-base,n)
def u32(va): return struct.unpack('<I',rd(va,4))[0]
tsv=[l.split('\t') for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:] if l.strip()]
for e in tsv:
    dem=e[3]
    if any(k in dem for k in ['@2VPString@2@B','@2VUId@2@B','1HA']):
        va=base+int(e[1],16)
        raw=rd(va,16)
        extra=''
        if 'PString' in dem:
            node=u32(va); extra=' node=%08x'%node
            if node:
                # try string at node+8, +0xc
                for off in (8,12,0x10):
                    s=rd(node+off,64).split(b'\0')[0]
                    if all(32<=c<127 for c in s) and s:
                        extra+=' str@%x=%r'%(off,s.decode()); break
        elif 'UId' in dem:
            extra=' guid='+raw[:16].hex()
        else:
            extra=' int='+str(u32(va))
        print(e[1],dem.split('public: ')[-1][:70],extra)
