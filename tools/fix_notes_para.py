p='notes/WaterSystem.md'
s=open(p,encoding='utf-8').read()
s=s.replace('''Module state (statics, vftables, the shader strings `aVswatermat*`/`aPswatermat*` at 100207A0..10039004) stays
the original's until the units that own it are ours; the rest forwards to `WaterSystem_7c_orig.dll` through the
generated .def, like the other modules.''','''All module state (statics, vtables and the shader strings `aVswatermat*`/`aPswatermat*` at
100207A0..10039004) is local as of 2026-11-24; the generated .def has no forwarders.''')
s=s.replace('\n\n\n## Complete','\n\n## Complete')
open(p,'w',encoding='utf-8').write(s)
print('ok')
