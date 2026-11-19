path=r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/StaticStubs.cpp'
text=open(path,encoding='ascii').read()
text=text.replace('extern "C" {\n','')
text=text.replace('\n}','')
open(path,'w',encoding='ascii',newline='\n').write(text)
print('done')
