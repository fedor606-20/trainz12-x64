import re
for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:]:
    p=l.split('\t')
    if p[1] in ('0x4c50','0x4a80','0x4a90','0x4b10','0x5160','0x5150','0x5170'):
        print(p[1],p[3][:120])
