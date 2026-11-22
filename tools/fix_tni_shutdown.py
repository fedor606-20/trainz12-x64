"""Build a local Trainz 2012 DLL with the static tree's allocator pairing fixed."""

import argparse
import hashlib
import struct
from pathlib import Path


PRISTINE_SHA256 = "4fb237298d2eb93e42a3def70577dfd50d674686b89f15a325e3c6c10c54c1c0"
TEXT_VSIZE_FIELD = 0x1F8
TEXT_RVA = 0x1000
CALL_SITE = 0x50C7
THUNK = 0xE9D0
GLOBAL_TREE_HEAD = 0x12204
TNI_FREE = 0x1B72
VC80_FREE = 0xA25E


def jump(opcode: int, site: int, target: int) -> bytes:
    return struct.pack("<Bi", opcode, target - (site + 5))


def patched(original: bytes) -> bytearray:
    if hashlib.sha256(original).hexdigest() != PRISTINE_SHA256:
        raise ValueError("Expected the pinned pristine TrainzNativeInterface.dll.")

    # This pinned image has identical file offsets and RVAs at the patch sites.
    # Check the module-global tree head only at the destructor's sentinel-free
    # call. The same destructor can be used for other trees, which still use TNI.
    thunk = bytearray.fromhex("e8 00 00 00 00 8b 80")  # call helper; mov eax,[eax+disp32]
    thunk += struct.pack("<i", GLOBAL_TREE_HEAD - (THUNK + 5))
    thunk += bytes.fromhex("85 c0 74 0b 3b 44 24 04 75 05")  # null or different pointer: use TNI
    thunk += jump(0xE9, THUNK + len(thunk), VC80_FREE)
    thunk += jump(0xE9, THUNK + len(thunk), TNI_FREE)
    thunk[:5] = jump(0xE8, THUNK, THUNK + len(thunk))
    thunk += bytes.fromhex("8b 04 24 c3")  # Return the actual call site's next address.
    end = THUNK + len(thunk)

    old_call = jump(0xE8, CALL_SITE, TNI_FREE)
    if original[CALL_SITE:CALL_SITE + 5] != old_call or any(original[THUNK:end]):
        raise ValueError("Unexpected instruction or occupied .text padding.")

    result = bytearray(original)
    result[CALL_SITE:CALL_SITE + 5] = jump(0xE8, CALL_SITE, THUNK)
    result[THUNK:end] = thunk
    struct.pack_into("<I", result, TEXT_VSIZE_FIELD, end - TEXT_RVA)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("output", type=Path, help="New DLL path; existing files are never overwritten.")
    args = parser.parse_args()
    try:
        result = patched(args.original.read_bytes())
        with args.output.open("xb") as out:
            out.write(result)
    except (ValueError, OSError) as exc:
        parser.exit(1, f"{exc}\n")


if __name__ == "__main__":
    main()
