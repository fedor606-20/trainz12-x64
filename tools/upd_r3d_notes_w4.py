p = 'notes/Render3DCore.md'
s = open(p, encoding='utf-8').read()
s = s.replace('**505/2500 exports** are local', '**1938/2500 exports** are local')
s += """

## Wave 4 (2026-11-24)

The parallel wave 4 merged: U4 caches (129/129), U5a materials (241/374), U5b MultiPass (115/254),
U9b scene (200/209), U10 primitives (191/230), U11 statics/plugin entry (59/167), and the U1/U2/U3/U7/U8/U9
tails (+16). Module total **1938/2500** local, 562 forwarded.

New tests (all green): `diff_r3d_u4_rem` 1,102, `diff_r3d_u5a_rem` 14,909 (200 rounds), `diff_r3d_u5b_rem`
6,666, `diff_r3d_u9b_rem` 1,600 (200 rounds), `diff_r3d_u10_rem` 739, `diff_r3d_tails` 57,145 (5,000 rounds);
the existing 20 `diff_r3d_*` suites remain 0 mismatches. `diff_r3d_tails` needs a pristine
`KernelStandard_7c.dll` in its fixture (the original's `s_sqrtLUT` import is pointer-to-array).

Known partials carried into wave 5: U5a data tables/dtors/plugin factories/render paths (133), U5b render/display
and program init (139), U10 live-kernel paths (39), seven U9b methods that bridge private original RVAs, U11
plugin-entry helpers (8), 108 template/ITree/Line/StdMaterialPlugin exports, and the 69 small tails.
"""
open(p, 'w', encoding='utf-8').write(s)
print('notes updated')
