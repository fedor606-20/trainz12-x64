# Track splines in trainz.exe (TS12 build 61388): how a TrackStretch gets its curve

Static reading only (Ghidra, 2026-10-28). Nothing here was checked in game yet.
Context: in the original game some track, fixed-track objects and catenary on existing routes
(Balezino bridges, routes from other Trainz versions) show up shifted against what was laid.

## Objects and fields

- `WorldCoord` (20 bytes): `short tileX, tileY; float x, y, z; byte normalised`. Tile = 720 m.
  `0x8c8240` normalises (moves whole tiles out of x/y). `0x407ac0` gives a vector relative to another
  tile: `x + (tileA - tileB) * 720`.
- `TrackVertex`: `+0x10` own id, `+0x1c` WorldCoord, `+0x30..+0x3c` up to four stretch ids,
  `+0x40..+0x43` side (0/1) of each link, `+0x44` "sides dirty", `+0x16` junction setting byte (saved).
  - Load `vf1` (0x5f3690): version 1-2 read three floats as a position on tile 0,0; version 3 reads a
    WorldCoord. Then four link ids and a byte each. Save `vf2` writes version 3.
  - Sides `0x5f4900`: one link -> side 0; two links -> 0 and 1; three or four -> split by angle.
- `TrackStretch` (ctor 0x5eb320): `+0x8c/+0x90` end vertex ids, `+0xc0` a name/PString (compared by
  `0x5f18a0`), `+0x138` attached object, `+0x13c` flags (bit 0x8 = snap end tangents, bit 0x20),
  `+0x11e` bit 0x4 = curve cached. Curve fields:
  `+0xd0` unit tangent at the start, `+0xdc` unit tangent at the end (points backwards),
  `+0xe8..+0x114` cubic coefficients per axis, `+0xc8` length, `+0x5c` tile of the start vertex.
  - Save `vf2` writes version 6: the two vertex ids, the track spec, `+0x13c`, `+0x11c`, a list.
    **No curve data is stored**: the shape is rebuilt from vertex positions on every load.

## The curve: `0x5ec840` (lazy; sets `+0x11e |= 4` first, then computes)

1. `0x5eca40` end directions. For each end vertex `0x5f43f0` builds Q = the average of the *far*
   endpoints of the other links on the opposite side of the vertex, counting only neighbours whose
   pairing code `0x5f18a0(this, neighbour)` has bit 0 set. No such neighbour -> Q = the vertex itself.
   `0x5ec960`: direction = normalise(normalise(a - b) + normalise(c - d)) with 0.1 m as the "too short
   to normalise" threshold, i.e. the bisector of the unit chords before and after the vertex.
   If `+0x13c & 8` (and some owner checks with a dynamic cast) each direction is snapped: when
   ||x| - |y|| < 0.4 to the diagonal (+-1, +-1), otherwise the smaller component is zeroed.
   **Side effect**: `0x5f43f0` erases a vertex link (and the neighbour stretch's end id) when that
   neighbour stretch does not have this vertex as an end or its far vertex does not exist.
2. `0x5ed310` continuity override. For each end, `0x5f3fa0`/`0x5f3d40` picks the continuing stretch
   through the vertex (at a junction the one chosen by vertex `+0x16`, the lever; if global
   `DAT_00c35608` is set, simply the first other link). If `0x5f18a0` gives exactly 1, the tangent at
   that end is replaced by minus the average of the opposite-side stretches' tangents (`0x5f4150`,
   which calls `0x5ec840` on them: recursive).
   Then the Hermite cubic: p0, p1 relative to the start tile, m0 = d_start * L, m1 = -d_end * L,
   L = chord length (KernelStandard table sqrt, `0x407c10`).
3. `0x5ed880` 32-step length and the 20 m grid cells it covers.

`0x5f18a0` codes: 1 = plain track to plain track (no `+0xc0`, no `+0x138`, no 0x20 flag on either),
3 = same `+0xc0`, 0 = different `+0xc0`, 2/3 when only one side has `+0xc0`, `+0x138` or flag 0x20.

## Invalidation

`0x5ec6e0` drops the cached curve and mesh and re-places trackside objects (`+0x124` list).
Callers: vertex moved `0x5f3bd0` (marks the vertex and its neighbours dirty; invalidates stretches up
to three links away), `0x5f5b80`, stretch load `vf1`, `vf25`, `0x5ec1d0`/`0x5ec250` (spec change),
TrackBridge `vf1`/`vf25`, `0x5f4b20`, `0x667520`, `0x668e70`.

## Why a curve can come out different from the one that was laid (hypotheses, unverified)

1. **Order dependence at junctions.** The override in step 2 copies a neighbour's tangent, and the
   neighbour is computed lazily and recursively. A stretch already marked "cached" returns whatever
   tangent it holds at that moment (before or after its own override). With three links at a vertex
   the copies are not symmetric, so the result depends on which stretch is computed first: load
   order, streaming, where the camera is, what was edited.
2. **State that is not part of the saved data.** The pairing code depends on `+0x138` (the attached
   object, a runtime pointer), the junction lever `+0x16` and the global `DAT_00c35608`. If these
   change after a neighbour's curve was cached, only the stretch itself is invalidated (TrackBridge
   `vf1`/`vf25` call `0x5ec6e0` on themselves only), so neighbours keep a curve built with the old
   pairing until something else rebuilds them. After a save and reload everything is rebuilt with the
   final state, so the track "moves". Fits bridges and fixed-track platforms.
3. **Links erased while computing a curve** (`0x5f43f0`). If a neighbour vertex is missing at that
   moment, the link is cut for good and is saved that way; the curve then loses its smooth neighbour.
4. Version 1-2 vertices (older routes) are read as positions on tile 0,0 with float x/y, which loses
   precision far from the origin; small offsets grow with the chord length through the tangents.

## How to check (next)

A probe that, after a route loads, dumps for every stretch: end ids, Q points, the pairing codes, the
tangents before and after step 2, and the coefficients; then forces a full rebuild (invalidate all,
compute in a fixed order) and diffs. Stretches whose curve changes are the ones that shift.

## Probe result 1 (2026-10-28, original VDK game, route open in Surveyor)

`probe/trackprobe` (WH_GETMESSAGE hook on the window thread; finds the world from a live TrackStretch,
because `DAT_00c3587c` is only set during some world operations and was null). 2041 stretches,
2594 vertices, all 2041 curves cached by the game. Cached vs a fresh rebuild, ascending vs descending
build order: **0 differences over 1 cm**, no build faults, no links cut. The game kept running.
So on this load hypotheses 1-3 do not show: the curve is a deterministic function of the vertex
positions and links. A visible shift has to come from the positions themselves (as stored, or as
moved by fixed-track objects) or from a different curve formula in the Trainz version the route
was made in.
