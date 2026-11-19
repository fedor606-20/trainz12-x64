import re
path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
targets=set(['1000e600','1000ed10','1000ed60','1000ee00','1000eed0'])
printing=False; buf=[]; found=0
with open(path,'r',encoding='utf-8',errors='ignore') as f:
    for line in f:
        m=re.match(r'^// ==== ([0-9a-fA-F]+)  ', line)
        if m:
            if printing:
                print(''.join(buf))
                found+=1
            addr=m.group(1).lower()
            printing = addr in targets
            buf=[line] if printing else []
            if found>=5: break
        elif printing:
            buf.append(line)
            if len(buf)>260:
                print(''.join(buf)); found+=1; printing=False; buf=[]
                if found>=5: break
    if printing and buf: print(''.join(buf))
