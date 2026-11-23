"""Generate local vtable slot maps and RTTI descriptions from a pristine 32-bit module.

The rebuilt module must not read the retail DLL at load or run time.  A vtable slot can still
point at one of our own exports when the original slot points at a real function; shared
three-byte stubs are replaced by one local stub, and import thunks resolve to the module the
original imported from.  The retail image's RTTI structures are read only to reproduce the
class names and the base-class layout numbers in local descriptors.

The output holds data only: slot-to-symbol names and flat numbers for
TypeDescriptor/BaseClassDescriptor/ClassHierarchyDescriptor/CompleteObjectLocator objects that
LocalTables.cpp builds.  Nothing from the retail image is linked into the module.

usage: gen_local_tables.py <original.dll> <output.h>
"""
import struct
import sys


class PE:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        n = struct.unpack_from("<H", d, pe + 6)[0]
        opt = pe + 24
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        self.secs = [struct.unpack_from("<8sIIII", d, opt + optsz + 40 * i) for i in range(n)]
        self.exports = {}
        self.by_name = {}
        o = self.off(struct.unpack_from("<I", d, opt + 96)[0])
        n_names, f_rva, n_rva, o_rva = struct.unpack_from("<IIII", d, o + 24)
        for i in range(n_names):
            name_off = self.off(struct.unpack_from("<I", d, self.off(n_rva) + 4 * i)[0])
            name = self.cstr(name_off)
            ordinal = struct.unpack_from("<H", d, self.off(o_rva) + 2 * i)[0]
            rva = struct.unpack_from("<I", d, self.off(f_rva) + 4 * ordinal)[0]
            self.exports[rva] = name
            self.by_name.setdefault(name, rva)
        self.imports = {}
        self._read_imports(opt)

    def _read_imports(self, opt):
        d = self.d
        imp_rva = struct.unpack_from("<I", d, opt + 104)[0]
        if not imp_rva:
            return
        count = struct.unpack_from("<I", d, opt + 108)[0]
        for i in range(count):
            entry = self.off(imp_rva + 20 * i)
            if entry is None:
                break
            oat, _ts, _fc, name_rva, iat = struct.unpack_from("<IIIII", d, entry)
            dll = self.cstr(self.off(name_rva))
            thunk = self.off(oat or iat)
            if thunk is None:
                continue
            j = 0
            while True:
                val = struct.unpack_from("<I", d, thunk + 4 * j)[0]
                if val == 0:
                    break
                if val & 0x80000000:
                    self.imports[self.base + iat + 4 * j] = (dll, "#%d" % (val & 0xffff))
                else:
                    name_off = self.off(val)
                    if name_off is not None:
                        self.imports[self.base + iat + 4 * j] = (dll, self.cstr(name_off + 2))
                j += 1

    def off(self, rva):
        for _, vs, va, rs, ro in self.secs:
            if va <= rva < va + max(vs, rs):
                return ro + rva - va
        return None

    def cstr(self, o):
        return self.d[o:self.d.find(b"\0", o)].decode("latin1")

    def code(self, rva, size=8):
        o = self.off(rva)
        return self.d[o:o + size] if o is not None else b""

    def u32(self, rva):
        return struct.unpack_from("<I", self.d, self.off(rva))[0]

    def is_stub(self, rva):
        code = self.code(rva, 3)
        # A one-byte `ret` is followed by int3 padding; the three return-constant stubs are exact.
        return code in (b"\x33\xc0\xc3", b"\x32\xc0\xc3", b"\xb0\x01\xc3") or (code and code[0] == 0xC3)

    def slot_symbol(self, target):
        """Classify one vtable slot target.  Returns a symbol string or None for a shared stub."""
        export = self.exports.get(target)
        if export:
            if self.is_stub(target):
                return None
            return export
        code = self.code(target, 6)
        if len(code) == 6 and code[0] == 0xFF and code[1] == 0x25:
            address = struct.unpack_from("<I", code, 2)[0]
            if address in self.imports:
                dll, name = self.imports[address]
                return "%s!%s" % (dll, name)
        if self.is_stub(target):
            return None
        return None

    def rtti(self, table_rva):
        """Read one vtable's RTTI: (type name, COL signature/offset/cd, class description)."""
        locator = self.u32(table_rva - 4)
        if not locator:
            return None
        entry = self.off(locator - self.base)
        if entry is None:
            return None
        signature, offset, cd_offset, type_rva, class_rva = struct.unpack_from(
            "<IIIII", self.d, entry)
        type_name = self.cstr(self.off(type_rva - self.base) + 8)
        hierarchy = None
        if class_rva:
            class_off = self.off(class_rva - self.base)
            if class_off is not None:
                chd_sig, attributes, base_count, array_rva = struct.unpack_from(
                    "<IIII", self.d, class_off)
                bases = []
                array_off = self.off(array_rva - self.base)
                for i in range(base_count):
                    base_rva = struct.unpack_from("<I", self.d, array_off + 4 * i)[0]
                    base_off = self.off(base_rva - self.base)
                    td, contained, mdisp, pdisp, vdisp, battr = struct.unpack_from(
                        "<IIiiii", self.d, base_off)
                    bases.append((self.cstr(self.off(td - self.base) + 8),
                                  contained, mdisp, pdisp, vdisp, battr))
                hierarchy = (chd_sig, attributes, bases)
        return (type_name, signature, offset, cd_offset, hierarchy)


# Slot maps for the exported tables LocalTables.cpp can build from our own exports.  Sizes are
# in slots; every table is preceded by its complete-object-locator pointer.
SLOT_TABLES = [
    ("??_7Display@Jet@@6B01@@", 152),
    ("??_7Display@Jet@@6BPluginInstance@1@@", 2),
    ("??_7DisplayContext@Display@Jet@@6B@", 1),
    ("??_7DisplaySaveBuffer@Jet@@6B@", 4),
    ("??_7DisplaySelector@Jet@@6B@", 7),
    ("??_7ElementLoader@Jet@@6B@", 2),
    ("??_7NonRenderContext@Jet@@6B@", 5),
    ("??_7SharedContext@Jet@@6B@", 2),
    ("??_7Material@Jet@@6B01@@", 23),
    ("??_7Material@Jet@@6BPluginInstance@1@@", 2),
    ("??_7MaterialPlugin@Jet@@6B@", 6),
    ("??_7?$PluginFactoryBase@VMaterial@Jet@@@Jet@@6B@", 4),
    ("??_7MaterialConfig@Jet@@6B@", 1),
    ("??_7NullMaterial@Jet@@6BMaterial@1@@", 23),
    ("??_7NullMaterial@Jet@@6BPluginInstance@1@@", 2),
]

# Tables built slot-by-slot in their own units (layers) or by private constructors
# (RenderKernel); only the RTTI description is generated for them.
RTTI_ONLY_TABLES = [
    "??_7RenderKernel@Jet@@6BTask@1@@",
    "??_7RenderKernel@Jet@@6BLayerManager@1@@",
    "??_7Layer@Jet@@6B@",
    "??_7LayerGroup@Jet@@6B@",
    "??_7ClearLayer@Jet@@6B@",
    "??_7Overlay@Jet@@6B@",
    "??_7LayerManager@Jet@@6B@",
    "??_7SystemResourceViewer@Jet@@6B@",
    "??_7SystemResourceManager@Jet@@6B@",
]


def ident(name):
    return "RsLocalVt" + "".join(c if c.isalnum() else "_" for c in name)


def quote(text):
    return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    pe = PE(sys.argv[1])
    out = []
    out.append("// Generated by tools/gen_local_tables.py from the installed original module.\n")
    out.append("// Data only: slot-to-symbol names and RTTI layout numbers.  No retail code or addresses.\n")
    out.append("// A slot name prefixed with \"module.dll!\" resolves in that module; nullptr is a shared\n")
    out.append("// no-op/pure slot and is filled with one local stub.  LocalTables.cpp overrides the\n")
    out.append("// compiler-generated adjustors and deleting destructors that have no export.\n\n")
    out.append("#pragma once\n\n")

    unknown = []
    for name, slots in SLOT_TABLES:
        rva = pe.by_name.get(name)
        if rva is None:
            out.append("// missing in the original: %s\n\n" % name)
            continue
        entries = []
        for i in range(slots):
            target = pe.u32(rva + 4 * i) - pe.base
            symbol = pe.slot_symbol(target)
            if symbol is None and not pe.is_stub(target):
                unknown.append((name, i, target))
            entries.append(quote(symbol) if symbol else "nullptr")
        out.append("static const char* const %s[%d] = {\n" % (ident(name), slots))
        for i in range(0, slots, 4):
            out.append("    " + ", ".join(entries[i:i + 4]) + ",\n")
        out.append("};\n\n")

    # RTTI: one type descriptor per decorated name, one class hierarchy per distinct base list,
    # and one complete-object locator per table (offset and cd offset differ between the tables
    # of one class).  Names and layout numbers come from the retail structures; the objects are
    # built locally by LocalTables.cpp.
    table_names = [name for name, _ in SLOT_TABLES] + RTTI_ONLY_TABLES
    type_names = []
    type_index = {}

    def intern(text):
        if text not in type_index:
            type_index[text] = len(type_names)
            type_names.append(text)
        return type_index[text]

    classes = []
    class_index = {}
    tables = []
    for name in table_names:
        rva = pe.by_name.get(name)
        info = pe.rtti(rva) if rva else None
        if info is None:
            out.append("// no RTTI in the original: %s\n" % name)
            continue
        type_name, signature, offset, cd_offset, hierarchy = info
        own_type = intern(type_name)
        if hierarchy is None:
            out.append("// no class hierarchy in the original: %s\n" % name)
            continue
        chd_sig, attributes, bases = hierarchy
        base_indices = []
        for base_name, contained, mdisp, pdisp, vdisp, battr in bases:
            base_indices.append((intern(base_name), contained, mdisp, pdisp, vdisp, battr))
        key = (own_type, chd_sig, attributes, tuple(base_indices))
        if key not in class_index:
            class_index[key] = len(classes)
            classes.append(key)
        tables.append((name, class_index[key], signature, offset, cd_offset))

    base_rows = []
    class_rows = []
    for own_type, chd_sig, attributes, bases in classes:
        first = len(base_rows)
        base_rows.extend(bases)
        class_rows.append((own_type, chd_sig, attributes, first, len(bases)))

    max_name = max(len(n) for n in type_names) + 1
    max_name = (max_name + 3) & ~3

    out.append("#define RS_LOCAL_RTTI_TYPE_COUNT %d\n" % len(type_names))
    out.append("#define RS_LOCAL_RTTI_BASE_COUNT %d\n" % len(base_rows))
    out.append("#define RS_LOCAL_RTTI_CLASS_COUNT %d\n" % len(class_rows))
    out.append("#define RS_LOCAL_RTTI_TABLE_COUNT %d\n" % len(tables))
    out.append("#define RS_LOCAL_RTTI_MAX_NAME %d\n\n" % max_name)

    out.append("static const char* const RsLocalRttiTypeNames[RS_LOCAL_RTTI_TYPE_COUNT] = {\n")
    for i in range(0, len(type_names), 4):
        out.append("    " + ", ".join(quote(n) for n in type_names[i:i + 4]) + ",\n")
    out.append("};\n\n")

    out.append("// { type index, numContainedBases, mdisp, pdisp, vdisp, attributes }\n")
    out.append("static const int RsLocalRttiBases[RS_LOCAL_RTTI_BASE_COUNT][6] = {\n")
    for row in base_rows:
        out.append("    {%d, %d, %d, %d, %d, %d},\n" % row)
    out.append("};\n\n")

    out.append("static const unsigned RsLocalRttiClassType[RS_LOCAL_RTTI_CLASS_COUNT] = {\n")
    out.append("    " + ", ".join(str(r[0]) for r in class_rows) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiClassSignature[RS_LOCAL_RTTI_CLASS_COUNT] = {\n")
    out.append("    " + ", ".join(str(r[1]) for r in class_rows) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiClassAttributes[RS_LOCAL_RTTI_CLASS_COUNT] = {\n")
    out.append("    " + ", ".join(str(r[2]) for r in class_rows) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiClassFirstBase[RS_LOCAL_RTTI_CLASS_COUNT] = {\n")
    out.append("    " + ", ".join(str(r[3]) for r in class_rows) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiClassBaseCount[RS_LOCAL_RTTI_CLASS_COUNT] = {\n")
    out.append("    " + ", ".join(str(r[4]) for r in class_rows) + ",\n};\n\n")

    out.append("static const char* const RsLocalRttiTableName[RS_LOCAL_RTTI_TABLE_COUNT] = {\n")
    for i in range(0, len(tables), 3):
        out.append("    " + ", ".join(quote(t[0]) for t in tables[i:i + 3]) + ",\n")
    out.append("};\n")
    out.append("static const unsigned RsLocalRttiTableClass[RS_LOCAL_RTTI_TABLE_COUNT] = {\n")
    out.append("    " + ", ".join(str(t[1]) for t in tables) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiTableSignature[RS_LOCAL_RTTI_TABLE_COUNT] = {\n")
    out.append("    " + ", ".join(str(t[2]) for t in tables) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiTableOffset[RS_LOCAL_RTTI_TABLE_COUNT] = {\n")
    out.append("    " + ", ".join(str(t[3]) for t in tables) + ",\n};\n")
    out.append("static const unsigned RsLocalRttiTableCdOffset[RS_LOCAL_RTTI_TABLE_COUNT] = {\n")
    out.append("    " + ", ".join(str(t[4]) for t in tables) + ",\n};\n\n")

    if unknown:
        out.append("// Real functions with no export and no import thunk; LocalTables.cpp installs a local\n")
        out.append("// override where the slot is reachable (adjustors, deleting destructors), else one stub.\n")
        for table, slot, _target in unknown:
            out.append("//   %s slot %u\n" % (table, slot))

    open(sys.argv[2], "w", encoding="utf-8", newline="\n").write("".join(out))


if __name__ == "__main__":
    main()
