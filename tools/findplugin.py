for l in open(r'D:\trainz12-decomp\exports\WaterSystem_7c.tsv',encoding='utf-8-sig').read().splitlines()[1:]:
    p=l.split('\t')
    if 'MaterialPlugin' in p[3] and 'Water' in p[3]: print(p[0],p[1],p[2],p[3][:80])
