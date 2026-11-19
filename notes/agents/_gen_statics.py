lines=[l.split('=')[0].strip() for l in open(r'D:/universal modder/trainz12-x64/notes/agents/forwarded_now.txt',encoding='ascii') if l.strip()]
statics=[]; methods=[]
for m in lines:
    idx=m.find('@@')
    after=m[idx+2:] if idx!=-1 else ''
    if after and after[0].isdigit():
        statics.append(m)
    else:
        methods.append(m)
out=[]
out.append('// Static data member stubs (RenderSystem_7c).')
out.append('extern "C" {')
for i,m in enumerate(statics):
    gname='rs_static_%d' % i
    out.append('int %s = 0;' % gname)
out.append('}')
open(r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/StaticGlobals.cpp','w',encoding='ascii',newline='\n').write(chr(10).join(out)+chr(10))
aliases=[]
for i,m in enumerate(statics):
    aliases.append('%s=rs_static_%d' % (m, i))
impl_lines=open(r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/implemented.txt',encoding='ascii',errors='ignore').read().splitlines()
have=set(l.strip().split('=')[0] for l in impl_lines if l.strip() and not l.startswith('#'))
cnt=0
for a in aliases:
    name=a.split('=')[0]
    if name not in have:
        impl_lines.append(a); have.add(name); cnt+=1
open(r'D:/universal modder/trainz12-x64/src/jet/graphics/RenderSystem_7c/implemented.txt','w',encoding='ascii',newline='\n').write(chr(10).join(impl_lines).rstrip()+chr(10))
print('statics', len(statics), 'aliases added', cnt)
