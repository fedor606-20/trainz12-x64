# U1 exact pass: PNoise, FrameAnimation, WaterGrid

Status 2026-11-24: **28001 checks / 0 mismatches** with `diff_ws_u1.exe . 200`, and **140001 / 0** with
1000 rounds. `build32` target `WaterSystem_7c` builds (490/490 exports) and `diff_ws_u1` links clean.
The PNoise constructor bytes are compared strictly and also match: the msvcr80 and UCRT `rand()` streams
are the same LCG, and both sides are reset to seed 1 before constructing.

Files: `src/jet/include/jet/PNoise.h`, `src/jet/graphics/WaterSystem_7c/PNoise.cpp`,
`src/jet/graphics/WaterSystem_7c/WaterGrid.cpp`, `src/jet/tests/diff_ws_u1.cpp`. `implemented.txt` untouched.

## Object layout (byte check against 0x10001E80)

    +0x000  int   numOctaves
    +0x004  int   perm[256]
    +0x404  int   permWrapped[258]
    +0x80C  float grad3D[768]           (256 vectors)
    +0x140C float grad3DWrapped[774]
    +0x2024 float grad2D[512]           (256 vectors)
    +0x2824 float grad2DWrapped[516]
    +0x3034 float grad1D[256]
    +0x3434 float grad1DWrapped[258]
    size 0x383C

`grad3DWrapped` starts at **0x140C (5132)**, not 0x1400: the old header comment said 5120 and was wrong;
the member was already at the right place because grad3D ends at 0x140C.

## Constructor 0x10001E80

Per entry i = 0..255, in this exact `rand()` order:

1. `perm[i] = i`
2. `grad1D[i] = (rand() % 512 - 256) / 256` (single, `CVTSI2SS` + `MULSS`)
3. two components -> `grad2D[2i], grad2D[2i+1]`; `len = (float)sqrt(x*x + y*y)` (x87 FSQRT of the
   float sum, stored back to float), `inv = 1.0f / len` (**single** DIVSS), components `x*inv`, `y*inv`
4. three components -> `grad3D[3i..3i+2]` in x,y,z order; sum is `z*z + x*x + y*y` (left to right, SSE),
   same float sqrt / single reciprocal, then `x*inv`, `y*inv`, `z*inv`

Fisher-Yates: i = 255 down to 1, `j = rand() % 256` (uniform over all 256 entries, not over 0..i; this is
the original's bias). `perm[0]` is a normal array slot and can be swapped; it is not the numOctaves field.
The helpers index the permutation 1-based (`Perm(1) == perm[0]`), so a hypothetical entry 0 maps to the
numOctaves int that sits immediately before the array.

The final copy loop is 0x102 = 258 iterations over every table:

    permWrapped[i]      = i < 256 ? perm[i]      : permWrapped[i - 256]
    grad1DWrapped[i]    = i < 256 ? grad1D[i]    : grad1DWrapped[i - 256]
    grad2DWrapped[j]    = j < 512 ? grad2D[j]    : grad2DWrapped[j - 512]     (j = 0..515)
    grad3DWrapped[j]    = j < 768 ? grad3D[j]    : grad3DWrapped[j - 768]     (j = 0..773)

The source address runs past the array into the destination itself, so the last two entries repeat the
first two wrapped entries (`permWrapped[256] = permWrapped[0]`, `permWrapped[257] = permWrapped[1]`, and
similarly 2D/3D). Earlier code used `permWrapped[i] = perm[i-1]` (shifted) and read `grad2D[512..513]` /
`grad3D[768..773]` out of bounds; both are fixed. The ctor test compares all 0x383C bytes, so this is
confirmed, not just inferred.

## Noise precision split

The binary is SSE for the float dots/smoothsteps and x87 (53-bit) for selected blends. The implementation
follows the instruction stream:

- Noise1 0x100014F0: `dot0` is float (SSE); `(xf-1)*g1 - dot0`, the smoothstep `s` and the final
  `xf*s*d + dot0` are x87 double.
- Noise2 0x10001570: `sx`, all four `dot` values and `row0 = (dot10-dot00)*sx+dot00` are single.
  `dot11`, `row1 = (dot11-dot01)*sx+dot01`, `sy` and the final `row0 + (row1-row0)*sy` are double.
- Noise3 0x10001700: `sx`, `sy`, seven `dot` values and the two y blends are single. `dot111` is an
  x87 sum (`(z*zf1 + y*yf1) + x*xf1`), then `row11`, `row1` and `sz` are double and the final z blend
  is x87. `Turbulence3` doubles only p[0] and p[1], never p[2].
- Turbulence1 0x10001A70: the whole body is single precision; only the returned float is loaded by x87.
- Turbulence2 0x10001B50 / Turbulence3 0x10001BE0: each octave computes `noise * amp + sum` in x87 and
  stores the accumulator as float, **but the function returns the last x87 sum from st0 without reloading
  the float**. So the D helpers return a double whose last octave is unrounded, while the per-octave
  accumulator stays float. This is invisible through the float export but matters inside the tiled code
  and is why `TurbulenceTiled` used to differ.
- TurbulenceTiled 0x10001CF0: `t0`, `t1`, `t2` are each stored to float; the final numerator/division is
  x87 double over `period*period`.
- TurbulenceLoopingTiled 0x10001DF0: the first sample is `TurbulenceTiled(x, y, z - loopPeriod, period)`;
  its **double** result is multiplied by `z` and stored to float (`SCALED`). The second sample is
  `TurbulenceTiled(x, y, z, period)` (z unchanged), and the result is
  `(SCALED + second * (loopPeriod - z)) / loopPeriod`, all in double. The objdump mnemonic `FMUL ST1` is
  destination-first (`ST1 = ST1 * ST0`), which is why the first result is scaled by z instead of becoming
  the second call's z coordinate. `(loopPeriod - z)` is not rounded to float.

## WaterGrid

Grid element stores in the original go through x87 `FLD/FSTP` even for scalar floats (`Set`, copy ctor,
operator=). That quiets signaling NaNs (sets bit 22) while preserving the payload; a plain C++ assignment
copies the raw bits. `WaterGrid.cpp` now reproduces the x87 load/store result for `float` and `Vector2`
elements, which removed the two residual `gridv2 get` mismatches on random byte patterns.

## Test conventions

`diff_ws_u1.cpp` calls the cdecl exports with this as the first stack argument, zeroes every grid element
before copy checks, and now also compares the full PNoise constructor block (`Same(a, b, 0x383C)`) before
copying our object into the original. The CRT reset is still done for both modules; because the streams
coincide no table tolerance is needed. If a future CRT diverges, only that single ctor check may be
relaxed (the maths below runs on our tables copied into the original); nothing else is tolerated.
