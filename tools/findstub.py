for l in open('ghidra_maps/jet/graphics/vtnames_WaterSystem_7c.dll.tsv',encoding='utf-8'):
    p=l.rstrip('\n').split('\t')
    if p and p[0]=='10015db0': print(l.rstrip())
