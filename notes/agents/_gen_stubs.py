lines_static=[l.strip() for l in open(r'D:/universal modder/trainz12-x64/notes/agents/forwarded_statics.txt',encoding='ascii') if l.strip()]
lines_vtable=[l.strip() for l in open(r'D:/universal modder/trainz12-x64/notes/agents/forwarded_vtables.txt',encoding='ascii') if l.strip()]
lines_vbtable=[l.strip() for l in open(r'D:/universal modder/trainz12-x64/notes/agents/forwarded_vbtables.txt',encoding='ascii') if l.strip()]
out=[]
out.append('// Static data member + vtable stubs (RenderSystem_7c).')
out.append('extern ' + chr(34) + 'C' + chr(34) + ' {')
for i,m in enumerate(lines_static):
    out.append('int rs_static_%d = 0;' % i)
for i,m in enumerate(lines_vtable):
    out.append('int rs_vtable_%d[1] = {0};' % i)
for i,m in enumerate(lines_vbtable):
    out.append('int rs_vbtable_%d[1] = {0};' % i)
out.append('}')
open(r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/StaticStubs.cpp','w',encoding='ascii',newline='\n').write(chr(10).join(out)+chr(10))
aliases=[]
for i,m in enumerate(lines_static):
    aliases.append('%s=_rs_static_%d' % (m, i))
for i,m in enumerate(lines_vtable):
    aliases.append('%s=_rs_vtable_%d' % (m, i))
for i,m in enumerate(lines_vbtable):
    aliases.append('%s=_rs_vbtable_%d' % (m, i))
impl_lines=open(r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/implemented.txt',encoding='ascii',errors='ignore').read().splitlines()
have=set(l.strip().split('=')[0] for l in impl_lines if l.strip() and not l.startswith('#'))
cnt=0
for a in aliases:
    name=a.split('=')[0]
    if name not in have:
        impl_lines.append(a); have.add(name); cnt+=1
open(r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/implemented.txt','w',encoding='ascii',newline='\n').write(chr(10).join(impl_lines).rstrip()+chr(10))
print('aliases added', cnt)
