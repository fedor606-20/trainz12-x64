# implemented.d

One fragment per parallel workstream, e.g. `u1b_plane.txt`. Each line is a mangled export (the format of
`../implemented.txt`). `tools/merge_implemented.py` concatenates this directory with the main list for `gen_def.py`;
duplicate names are rejected. Do not edit another unit's fragment.
