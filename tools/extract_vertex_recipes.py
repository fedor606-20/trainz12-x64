"""Read TS12's built-in vertex format descriptions, not executable code."""
import argparse
import json
import re
import struct
import subprocess
from pathlib import Path


def extract(image, disassembly):
    pe = struct.unpack_from("<I", image, 0x3c)[0]
    sections = struct.unpack_from("<H", image, pe + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe + 20)[0]
    base = struct.unpack_from("<I", image, pe + 52)[0]
    if base != 0x10000000:
        raise ValueError("unexpected RenderSystem image base")

    def file_offset(address):
        rva = address - base
        for i in range(sections):
            _, va, size, raw = struct.unpack_from("<IIII", image, pe + 24 + optional_size + 40 * i + 8)
            if va <= rva < va + size:
                return raw + rva - va
        raise ValueError(f"address not present in image: {address:#x}")

    def text(address):
        pos = file_offset(address)
        return image[pos:image.index(0, pos)].decode("ascii")

    def number(operand):
        if operand == "ebp":
            return 0
        if operand == "ebx":
            return -1
        if re.fullmatch(r"[0-9]+|[0-9A-F]+h", operand):
            return int(operand[:-1], 16) if operand.endswith("h") else int(operand)
        raise ValueError(f"unexpected vertex recipe operand: {operand}")

    rows = []
    for line in disassembly.splitlines():
        m = re.match(r"  ([0-9A-F]+): [0-9A-F ]+\s{2,}([a-z]+)\s*(.*)", line)
        if m and 0x10011F7A <= int(m[1], 16) < 0x1001B369:
            rows.append((int(m[1], 16), m[2], m[3].strip()))
    index = {address: i for i, (address, _, _) in enumerate(rows)}
    entries = struct.unpack_from("<99I", image, file_offset(0x1001B3A8))
    attributes, recipes = [], []
    end = len(rows)
    for entry in entries:
        streams = None
        capacities = []
        args = []
        pending_name = None
        literal = None
        first = len(attributes)
        for _, op, operand in rows[index[entry]:end]:
            if op == "push":
                args.append(operand)
            elif op == "mov" and re.fullmatch(r"eax,100[0-9A-F]+h", operand):
                literal = int(operand[4:-1], 16)
            elif op == "call":
                if operand == "10007C70":
                    if literal is None:
                        raise ValueError("missing PString literal")
                    pending_name = text(literal)
                    literal = None
                elif operand in ("10056680", "10056700", "10056800"):
                    count = {"10056680": 2, "10056700": 3, "10056800": 9}[operand]
                    self_, *params = reversed(args[-count:])
                    if self_ != "edi":
                        raise ValueError("unexpected VertexType receiver")
                    if count == 2:
                        streams = number(params[0])
                    elif count == 3:
                        stream, capacity = map(number, params)
                        if stream != len(capacities):
                            raise ValueError("non-sequential stream")
                        capacities.append(capacity)
                    else:
                        stream, item, size, kind, semantic, name_arg, offset, flag = params
                        if name_arg not in ("eax", "ecx", "edx"):
                            raise ValueError("unexpected attribute name argument")
                        # Named attributes consume the pending PString. Other attributes use an empty PString.
                        attributes.append((*map(number, (stream, item, size, kind, semantic, offset, flag)),
                                           pending_name))
                        pending_name = None
                    args.clear()
                elif operand == "10056CD0":
                    break
                elif operand != "10054EB0":
                    raise ValueError(f"unexpected vertex recipe call: {operand}")
            elif op == "jmp":
                if operand not in ("1001B34B", "1001B350"):
                    raise ValueError(f"unexpected vertex recipe jump: {operand}")
                break
        if streams not in (1, 3) or len(capacities) != streams or not pending_name:
            raise ValueError(f"incomplete recipe at {entry:#x}")
        if len(attributes) - first != sum(capacities):
            raise ValueError(f"attribute count mismatch at {entry:#x}")
        recipes.append((first, len(attributes) - first, streams, capacities + [0] * (3 - streams), pending_name))
    if len(attributes) != 517 or sum(row[-1] is not None for row in attributes) != 3:
        raise ValueError("unexpected built-in vertex attribute count")
    return attributes, recipes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    parser.add_argument("dumpbin")
    parser.add_argument("header", type=Path)
    args = parser.parse_args()
    disassembly = subprocess.run([args.dumpbin, "/nologo", "/disasm", str(args.dll)], check=True,
                                 stdout=subprocess.PIPE, encoding="ascii", errors="replace").stdout
    attributes, recipes = extract(args.dll.read_bytes(), disassembly)
    lines = ["// Generated from the locally installed TS12 RenderSystem DLL.",
             "constexpr VertexAttribute attributes[] = {"]
    for *fields, name in attributes:
        lines.append(f"    {{{', '.join(map(str, fields))}, {json.dumps(name) if name else 'nullptr'}}},")
    lines.extend(["};", "constexpr VertexRecipe recipes[] = {"])
    for first, length, streams, capacities, name in recipes:
        lines.append(f"    {{{first}, {length}, {streams}, {{{', '.join(map(str, capacities))}}}, {json.dumps(name)}}},")
    lines.append("};")
    args.header.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
