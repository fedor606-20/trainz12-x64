import glob, re
paths = glob.glob(r'D:\universal modder\trainz12-x64\src\jet\graphics\RenderSystem_7c\DisplaySimple*.cpp') + \
        glob.glob(r'D:\universal modder\trainz12-x64\src\jet\graphics\RenderSystem_7c\SimpleMiscExports.cpp')
for p in paths:
    s = open(p, encoding='utf-8').read()
    # replace " __cdecl Display::" / " __cdecl NonRenderContext::" etc. but keep "static ... __cdecl Display::GetInternalColorFormat" (SA mangling needs __cdecl)
    # also keep ConvertColor if static __cdecl in header? header has static unsigned long __cdecl ConvertColor...
    def repl(m):
        prefix = m.group(1)
        cls = m.group(2)
        # keep static __cdecl on GetInternalColorFormat/ConvertColor
        if 'static' in prefix and cls == 'Display' and ('GetInternalColorFormat' in m.group(0) or 'ConvertColor' in m.group(0)):
            return m.group(0)
        # For static methods generally keep __cdecl if present in prefix before return type? easier: remove __cdecl token before Class::
        return prefix + cls + '::'
    s2 = re.sub(r'([^\n]*?) __cdecl (\w+)::', repl, s)
    if s2 != s:
        open(p,'w',encoding='utf-8',newline='\n').write(s2)
        print('fixed', p)
    else:
        print('nochange', p)
