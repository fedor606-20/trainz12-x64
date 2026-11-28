p='src/jet/tests/diff_ws_u1.cpp'
s=open(p,encoding='utf-8').read()
old='''        ctorS(a, nullptr, &sz);
        ctorS1(b, nullptr, &sz);
        Same(a + 4, b + 4, 8, std::string(part) + " ctor size");  // width, height (vftable and data ptr differ)'''
new='''        ctorS(a, nullptr, &sz);
        ctorS1(b, nullptr, &sz);
        Same(a + 4, b + 4, 8, std::string(part) + " ctor size");  // width, height (vftable and data ptr differ)
        // The original ctor leaves the elements uninitialised, so make the entire grid deterministic before
        // any copy comparison (otherwise heap garbage differs between the two modules by design).
        unsigned char zero[16] = {};
        for (int y = 0; y < sz.h; ++y)
            for (int x = 0; x < sz.w; ++x) {
                set(a, x, y, zero);
                set1(b, x, y, zero);
            }'''
if old not in s: raise SystemExit('anchor')
s=s.replace(old,new)
open(p,'w',encoding='utf-8').write(s)
print('ok')
