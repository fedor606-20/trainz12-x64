for l in open('ghidra_maps/jet/graphics/vtnames_WaterSystem_7c.dll.tsv',encoding='utf-8'):
    p=l.rstrip('\n').split('\t')
    if p and p[0] in {'1001f6ac','1001f7b0','100049c0','100049b0','10005150','10005170','10004c50','10004a80'}:
        print(l.rstrip())
