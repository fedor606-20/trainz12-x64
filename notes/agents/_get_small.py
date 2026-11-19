import re
path=r'D:\trainz12-decomp\decomp\RenderSystem_7c.dll.c'
# get Layer simple methods and Display simple getters/setters that are just field reads/writes (small, but need class layout known)
targets=set(['100015d0','10021870','1000ae00','1000ae10','1000ae30','1000ae40','1000ae50','1000ae70','1000aed0','1000aef0','100106d0','10010940','10021a60','10038c50','1000d150','1000d160','1000d170'])
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
            if found>=30: break
        elif printing:
            buf.append(line)
            if len(buf)>120:
                print(''.join(buf)); found+=1; printing=False; buf=[]
                if found>=30: break
    if printing and buf: print(''.join(buf))
