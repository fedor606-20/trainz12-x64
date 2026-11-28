for l in open('ghidra_maps/jet/graphics/vtnames_WaterSystem_7c.dll.tsv',encoding='utf-8'):
    p=l.rstrip('\n').split('\t')
    if p and p[0] in {'10017830','10017836','100179aa','10007280','100159f0'}:
        print(l.rstrip())
