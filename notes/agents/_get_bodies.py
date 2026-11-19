import re
text=open(r'D:/trainz12-decomp/decomp/RenderSystem_7c.dll.c','r',encoding='utf-8',errors='ignore').read()
addrs=['10001ca20','10001ca40','10002020','10003170','10003d50','10004400','10004480','1001b820','10003f60','1001b7a0','10010ec0','10004db0','1000b1a0','10021870','10002990','100029e0','10002a30','10002a60','1000b7a0','1000b6e0','1001ba20','1001b930','1001b8a0','1001b890','1001b8c0','10011d70','10011dd0','1000eed0','1000ee00','1000ed60','1000ed10']
for a in addrs:
    m=re.search(r'^// ==== '+a+r' .*?(?=^// ==== )', text, re.M|re.S)
    if m:
        print('---ADDR:'+a+'---')
        print(m.group(0)[:1800])
    else:
        print('---ADDR:'+a+'--- NO')
