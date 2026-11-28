impl=open('build32/ws_impl_test.txt',encoding='utf-8').read().splitlines()
local=set(l.split('=',1)[0].strip() for l in impl if l.strip() and not l.startswith('#'))
for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:]:
    p=l.split('\t')
    if p[2] not in local: print(p[0],p[1],p[2],'|',p[3][:100])
