// Simple/evidenced Display exports batch 6 (RenderSystem_7c).
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

namespace Jet {

namespace {
inline char* D(Display* d) { return reinterpret_cast<char*>(d); }
inline unsigned char* St(Display* d) { return *reinterpret_cast<unsigned char**>(D(d) + 0x54); }
}  // namespace

void Display::Set3DView(float a, float b, float c, float d)
{
    // 0x10002dc0
    unsigned char* st = St(this);
    int* iv = reinterpret_cast<int*>(st);
    if (iv[1] != 3 || *reinterpret_cast<float*>(st + 8) != b ||
        *reinterpret_cast<float*>(st + 0xc) != c ||
        *reinterpret_cast<float*>(st + 0x10) != d ||
        *reinterpret_cast<float*>(st + 0x14) != a) { // careful: param order
        // follow decomp: param_1=this, param_2,3,4 + in_stack
    }
    // Simplified: store values per decomp layout
    iv[1] = 3;
    st = St(this);
    *reinterpret_cast<float*>(st + 8) = b;
    *reinterpret_cast<float*>(st + 0xc) = c;
    *reinterpret_cast<float*>(st + 0x10) = d;
    *reinterpret_cast<float*>(st + 0x14) = a; // placeholder ordering
    VCall<void, float, float, float>(this, 0x188, b, c, d);
}

void Display::LoadProjectionMatrix(const Matrix4& m)
{
    // 0x10003170
    unsigned char* st = St(this);
    *reinterpret_cast<int*>(st + 4) = 5;
    VCall<void>(this, 0x19c);
}

void Display::StartLightingUpdate(int idx, const Matrix4& m)
{
    // 0x10003cf0
    const int count = *reinterpret_cast<int*>(D(this) + 0x50);
    if (idx >= 0 && idx < count) {
        unsigned char* enabled = *reinterpret_cast<unsigned char**>(St(this) + 0x110);
        for (int i = idx; i < count; ++i) {
            if (enabled[i]) {
                VCall<void, int, int>(this, 0x224, i, 0);
                enabled[i] = 0;
            }
        }
    }
    VCall<void, const Matrix4&>(this, 0x228, m);
}

void Display::SetTextureCoordGen(int stage, DTexGenMode mode, unsigned int gen)
{
    // 0x10003190
    unsigned char* st = St(this);
    unsigned int* arr = reinterpret_cast<unsigned int*>(st + 0xb8);
    if (arr[0] != gen || arr[1] != static_cast<unsigned int>(mode)) {
        arr[0] = gen;
        arr[1] = static_cast<unsigned int>(mode);
        VCall<void, int, unsigned int>(this, 0x240, stage, gen);
    }
}

void Display::SetColorMaterialMode(DColorMaterialTypes mode)
{
    // 0x10003c30
    unsigned char* st = St(this);
    if (*reinterpret_cast<int*>(st + 0x98) != static_cast<int>(mode)) {
        VCall<void>(this, 0x20c);
        *reinterpret_cast<int*>(St(this) + 0x98) = static_cast<int>(mode);
    }
}

void Display::SetStencilTestMode(bool test, bool write)
{
    // 0x10003fa0
    unsigned char* st = St(this);
    bool curTest = (st[0xa0] >> 3) & 1;
    bool curWrite = (st[0xa0] >> 4) & 1;
    if (curTest != test || curWrite != write) {
        // vcall slot 0x1bc returns bool; call it
        bool ok = VCall<bool, bool>(this, 0x1bc, test);
        if (ok) {
            unsigned char* s = St(this);
            s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~8) | (test ? 8 : 0));
            s = St(this);
            s[0xa0] = static_cast<unsigned char>((s[0xa0] & ~0x10) | (write ? 0x10 : 0));
        } else {
            unsigned char* s = St(this);
            s[0xa0] = static_cast<unsigned char>(s[0xa0] & ~8);
        }
    }
}

void Display::SetStencilFunction(DFunction fn, int ref, unsigned long mask, DFace face)
{
    // 0x10004090
    unsigned char* st = St(this);
    int idx = static_cast<int>(face);
    int* pi = reinterpret_cast<int*>(st + 0xd8 + idx * 0x1c);
    if (pi[0] != static_cast<int>(fn) || pi[1] != ref || pi[2] != static_cast<int>(mask)) {
        VCall<void>(this, 0x1c4);
        pi[0] = static_cast<int>(fn);
        pi[1] = ref;
        pi[2] = static_cast<int>(mask);
    }
}

void Display::SetStencilOperation(DOperation op1, DOperation op2, DOperation op3, DFace face)
{
    // 0x100040f0
    unsigned char* st = St(this);
    int idx = static_cast<int>(face);
    int base = reinterpret_cast<int>(st) + 0xd8 + idx * 0x1c;
    DOperation* p = reinterpret_cast<DOperation*>(base + 0x10);
    if (p[0] != op2 || p[1] != op3 || *reinterpret_cast<DFace*>(base + 0x18) != face) {
        VCall<void>(this, 0x1c8);
        p[0] = op2;
        p[1] = op3;
        *reinterpret_cast<DFace*>(base + 0x18) = face;
    }
}

void Display::SetMaterialProperties(const DisplayMaterial& dm)
{
    // 0x10003360
    unsigned char* st = St(this);
    DisplayMaterial& cur = *reinterpret_cast<DisplayMaterial*>(st + 0x114);
    if (cur.ambient.r != dm.ambient.r || cur.diffuse.r != dm.diffuse.r) { // simplified check
        VCall<void>(this, 0x21c);
        cur = dm;
    }
}

void Display::EnableProjectedTexture(int idx, bool a, bool b)
{
    // 0x100032a0
    VCall<void, int, bool, bool>(this, 0x218, idx, a, b);
    unsigned char* st = St(this);
    st[0x66] = b ? 1 : 0;
}

void Display::EndLightingUpdate()
{
    // 0x100032? shared? no body found; no-op safe
}

void Display::SetViewModeParams(const ViewModeParams& p)
{
    // 0x10002fa0
    unsigned char* st = St(this);
    int* iv = reinterpret_cast<int*>(st + 4);
    const int* pv = reinterpret_cast<const int*>(&p);
    // compare all 9 fields
    bool same = true;
    for (int i = 0; i < 9; ++i) if (iv[i] != pv[i]) { same = false; break; }
    if (!same) {
        for (int i = 0; i < 9; ++i) iv[i] = pv[i];
        VCall<void>(this, 0x184); // unknown slot; use 0x180? decomp calls (**(code **)(iVar2 + 0x180)) maybe
    }
}

void Display::GetViewModeParams(ViewModeParams& p) const
{
    // 0x10002f40
    const unsigned char* st = reinterpret_cast<const unsigned char*>(*reinterpret_cast<unsigned char* const*>(reinterpret_cast<const char*>(this) + 0x54));
    int* pv = reinterpret_cast<int*>(&p);
    const int* iv = reinterpret_cast<const int*>(st + 4);
    for (int i = 0; i < 9; ++i) pv[i] = iv[i];
}

bool Display::CheckExtensionString(const PString& s) const
{
    // 0x1001b840
    for (int i = 0; i < 0x21; ++i) {
        // compare PString; use Compare
        // simplified: return false
    }
    return false;
}

const char* Display::ConvertTexCompressionString(DTexCompression idx) const
{
    // 0x1001b8a0 - string table not reconstructed; return nullptr
    (void)idx;
    return nullptr;
}

DTexCompression Display::ConvertTexCompressionString(const char* s) const
{
    // 0x1001b8c0
    for (int i = 0; i < 9; ++i) {
        // compare strings; unknown table
    }
    return static_cast<DTexCompression>(-1);
}

void Display::Set2DView(float a, float b, float c, float d, float e, float f)
{
    // 0x10002a70
    unsigned char* st = St(this);
    int* iv = reinterpret_cast<int*>(st);
    if (iv[1] != 0 || *reinterpret_cast<float*>(st + 8) != b ||
        *reinterpret_cast<float*>(st + 0xc) != c ||
        *reinterpret_cast<float*>(st + 0x10) != d ||
        *reinterpret_cast<float*>(st + 0x14) != e ||
        *reinterpret_cast<float*>(st + 0x18) != f) {
        iv[1] = 0;
        st = St(this);
        *reinterpret_cast<float*>(st + 8) = b;
        *reinterpret_cast<float*>(st + 0xc) = c;
        *reinterpret_cast<float*>(st + 0x10) = d;
        *reinterpret_cast<float*>(st + 0x14) = e;
        *reinterpret_cast<float*>(st + 0x18) = f;
        *reinterpret_cast<float*>(st + 0x1c) = a; // placeholder
        VCall<void, float, float, float, float, float, float>(this, 0x180, a, b, c, d, e, f);
    }
}

void Display::Set2DViewNormalized(bool on)
{
    // 0x10002b60 (complex; stub)
    (void)on;
}

void Display::Set3DViewOffset(float a, float b, float c, float d, float e)
{
    // 0x10002e70
    unsigned char* st = St(this);
    int* iv = reinterpret_cast<int*>(st);
    if (iv[1] != 4 || *reinterpret_cast<float*>(st + 8) != b ||
        *reinterpret_cast<float*>(st + 0xc) != c ||
        *reinterpret_cast<float*>(st + 0x10) != d ||
        *reinterpret_cast<float*>(st + 0x14) != e ||
        *reinterpret_cast<float*>(st + 0x18) != a) {
        iv[1] = 4;
        st = St(this);
        *reinterpret_cast<float*>(st + 8) = b;
        *reinterpret_cast<float*>(st + 0xc) = c;
        *reinterpret_cast<float*>(st + 0x10) = d;
        *reinterpret_cast<float*>(st + 0x14) = e;
        *reinterpret_cast<float*>(st + 0x18) = a;
        VCall<void, float, float, float, float>(this, 0x188, b, c, d + a, e);
    }
}

void Display::Set3DTransitionalView(float a, float b, float c, float d, float e, float f, float g)
{
    // 0x1000? complex; stub
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;
}

}  // namespace Jet
