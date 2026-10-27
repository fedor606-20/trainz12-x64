// Unit U3a of RenderSystem_7c: VertexProgram, the base of the API modules' vertex programs (0x100492c0..0x1004a510,
// its small members at 0x10021840..0x100219b0 and in shared bodies elsewhere). Behaviour follows the TS12 assembly
// (addresses per function); the float code keeps the original's order of operations, so the constants come out
// bit for bit. The register limits and the support flags stay the original's statics until U9.
#include <jet/GfxLanguage.h>
#include <jet/VCall.h>

#include <cmath>

#include "GfxParse.h"

namespace Jet {

namespace {

// an AnsiString's length (its block: {refs, length, chars})
unsigned int Length(const AnsiString& s) { return *reinterpret_cast<const unsigned int*>(*reinterpret_cast<char* const*>(&s) + 4); }

const float* F(const void* p) { return static_cast<const float*>(p); }
int LightType(const DisplayLight* l) { return *reinterpret_cast<const int*>(reinterpret_cast<const char*>(l) + 4); }

// fcos on the x87 stack, rounded once to float (0x10049ee5)
float X87Cos(double x)
{
#if defined(_M_IX86)
    float r;
    __asm {
        fld x
        fcos
        fstp r
    }
    return r;
#else
    return static_cast<float>(std::cos(x));
#endif
}

// 0x1004a730: the point through the whole matrix, divided by w
Vector3 Transform(const Matrix4& view, const float* p)
{
    const float* m = F(&view);
    const float x = m[8] * p[2] + m[4] * p[1] + m[0] * p[0] + m[12];
    const float y = m[9] * p[2] + m[1] * p[0] + m[5] * p[1] + m[13];
    const float z = m[10] * p[2] + m[2] * p[0] + m[6] * p[1] + m[14];
    const float w = m[11] * p[2] + m[3] * p[0] + m[7] * p[1] + m[15];
    const float inv = 1.0f / w;
    Vector3 r;
    float* o = reinterpret_cast<float*>(&r);
    o[0] = x * inv;
    o[1] = y * inv;
    o[2] = z * inv;
    return r;
}

// 0x1002ae50: scaled by StdMath's table-driven 1/sqrt (inline in the original)
void Normalize(float* v)
{
    const float len2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    const float s = StdMath::InvSqrt(len2);
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
}

// LoadCommonStates' lights sorted by type on first use (0x1004a510): the index-th light of a type below 5, else of
// all, or null
struct LightsByType {
    explicit LightsByType(const DisplayLightList& l) : built(false), all(&l) {}

    const DisplayLight* Find(unsigned int index, int type)
    {
        if (!built) {
            for (DisplayLightList::ConstIterator i = all->First(); i != all->End(); ++i)
                byType[LightType(*i)].Add(*i);
            built = true;
        }
        const DisplayLightList& list = type < 5 ? byType[type] : *all;
        unsigned int count = 0;
        for (DisplayLightList::ConstIterator i = list.First(); i != list.End(); ++i) ++count;
        if (index >= count) return nullptr;
        DisplayLightList::ConstIterator i = list.First();
        i += static_cast<int>(index);
        return *i;
    }

    bool built;
    const DisplayLightList* all;
    DisplayLightList byType[5];
};

const float kZero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------

VertexProgram::VertexProgram(Display* d, VertexType* type)
    : ExtendedGfxLanguage(d), vertexType(type), influenceBase(-1), influenceMax(-1), field40(false), manager(nullptr)
{
    if (type) reinterpret_cast<ReferenceCount*>(type)->AddReference();
}

VertexProgram::~VertexProgram()
{
    if (vertexType) reinterpret_cast<ReferenceCount*>(vertexType)->ReleaseReference();
}

// 0x10049430
void __cdecl VertexProgram::InitInstance(VertexProgram& copy) const
{
    ExtendedGfxLanguage::InitInstance(copy);
    copy.vertexType = vertexType;
    copy.influenceBase = influenceBase;
    copy.influenceMax = influenceMax;
    copy.field40 = field40;
    copy.manager = manager;
    if (vertexType) reinterpret_cast<ReferenceCount*>(vertexType)->AddReference();
}

// 0x10049480 / 0x100494e0: the display's capability 0x11 + generation
bool __cdecl VertexProgram::SupportedByPlatform(Display* d, float generation)
{
    if (!(1.0f <= generation) || !(generation <= 2.0f)) return false;
    const int i = static_cast<int>(generation) - 1;
    if (!supportChecked[i]) {
        supported[i] = VCall<bool>(d, 0x1c, i + 0x12);
        supportChecked[i] = true;
    }
    return supported[i];
}

bool __cdecl VertexProgram::SupportedByPlatform(float generation) { return SupportedByPlatform(display, generation); }

// 0x10049540: the pre-parser's output through the vertex program parser
bool __cdecl VertexProgram::Load(MemoryFile& file, const PString& name, const PString& fileName)
{
    SetFilename(fileName);
    struct Ctx {
        const PString* name;
        const PString* fileName;
        VertexProgram* self;
    } ctx{&name, &fileName, this};
    return GfxPreParse(GetApi(), &file, name, fileName, this,
                       [](const AnsiString& out, void* p) {
                           const Ctx& c = *static_cast<Ctx*>(p);
                           MemoryFile text(static_cast<const char*>(out), Length(out), false, 0);
                           return VertexProgramParse(&text, *c.name, *c.fileName, c.self);
                       },
                       &ctx);
}

// 0x10049920
bool __cdecl VertexProgram::LoadNativeFormat(const PString& name, const char* source)
{
    SetProgramName(name);
    AnsiString text(source);
    return LoadNativeProgram(text);
}

VertexProgram* __cdecl VertexProgram::CreateShared() { return nullptr; }
void __cdecl VertexProgram::SetProgramManager(VertexProgramManager* m) { manager = m; }
VertexProgramManager* __cdecl VertexProgram::GetProgramManager() { return manager; }
int __cdecl VertexProgram::Slot12() { return 0; }
int __cdecl VertexProgram::Slot13() { return 0; }
int __cdecl VertexProgram::Slot14() { return 0; }
int __cdecl VertexProgram::Slot15() { return 0; }
bool __cdecl VertexProgram::RestoreProgram() { return true; }
void __cdecl VertexProgram::SetProgramState(const DisplayLightList&, Color, const DisplayMaterial&, const Matrix4&) const {}
void __cdecl VertexProgram::LoadConstantRegisters() const {}
int __cdecl VertexProgram::Slot20() { return 0; }
int __cdecl VertexProgram::Slot21() { return 0; }
int __cdecl VertexProgram::Slot22() { return 0; }
bool __cdecl VertexProgram::SetLocalConstant(int, const Vector4&) const { return false; }
bool __cdecl VertexProgram::SetLocalConstantDirect(int, const Vector4&) const { return false; }
int __cdecl VertexProgram::Slot25() { return 0; }
bool __cdecl VertexProgram::SetLocalConstants(int, const Vector4*, int) const { return false; }
bool __cdecl VertexProgram::SetEnvConstant(int, const Vector4&) const { return false; }
int __cdecl VertexProgram::GetMaxLocalRegisters() const { return 0; }
int __cdecl VertexProgram::GetMaxEnvRegisters() const { return 0; }
int __cdecl VertexProgram::GetMaxTotalRegisters() const { return 0; }
int __cdecl VertexProgram::GetApi() const { return 0; }
bool __cdecl VertexProgram::LoadNativeProgram(AnsiString&) { return false; }

// 0x10049760
bool __cdecl VertexProgram::SetVertexType(VertexType* type)
{
    if (vertexType == type) return false;
    if (vertexType) reinterpret_cast<ReferenceCount*>(vertexType)->ReleaseReference();
    reinterpret_cast<ReferenceCount*>(type)->AddReference();
    vertexType = type;
    return true;
}

VertexType* __cdecl VertexProgram::GetVertexType() const { return vertexType; }
int __cdecl VertexProgram::GetInfluenceBaseRegister() const { return influenceBase; }
int __cdecl VertexProgram::GetInfluenceMaxRegister() const { return influenceMax; }

// 0x10021880..0x100219a0: w = 1; the Color forms hand the colour on as a Vector4
bool __cdecl VertexProgram::SetLocalConstant(int reg, const Vector3& v) const
{
    const float* p = F(&v);
    const float q[4] = {p[0], p[1], p[2], 1.0f};
    return SetLocalConstant(reg, *reinterpret_cast<const Vector4*>(q));
}
bool __cdecl VertexProgram::SetLocalConstant(int reg, const Color& c) const
{
    return SetLocalConstant(reg, reinterpret_cast<const Vector4&>(c));
}
bool __cdecl VertexProgram::SetLocalConstantDirect(int reg, const Vector3& v) const
{
    const float* p = F(&v);
    const float q[4] = {p[0], p[1], p[2], 1.0f};
    return SetLocalConstantDirect(reg, *reinterpret_cast<const Vector4*>(q));
}
bool __cdecl VertexProgram::SetLocalConstantDirect(int reg, const Color& c) const
{
    return SetLocalConstantDirect(reg, reinterpret_cast<const Vector4&>(c));
}
bool __cdecl VertexProgram::SetEnvConstant(int reg, const Vector3& v) const
{
    const float* p = F(&v);
    const float q[4] = {p[0], p[1], p[2], 1.0f};
    return SetEnvConstant(reg, *reinterpret_cast<const Vector4*>(q));
}
bool __cdecl VertexProgram::SetEnvConstant(int reg, const Color& c) const
{
    return SetEnvConstant(reg, reinterpret_cast<const Vector4&>(c));
}

// 0x10049790: each matrix transposed, three rows from the influence base on
void __cdecl VertexProgram::LoadInfluences(const Array<MatrixMap*, 128>& matrices) const
{
    if (influenceBase < 0) return;
    int reg = 0;
    for (unsigned int i = 0; i < matrices.count; ++i) {
        const float* m = F(matrices.data[i]->matrix);
        float t[16];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) t[r * 4 + c] = m[c * 4 + r];
        SetLocalConstants(influenceBase + reg, reinterpret_cast<const Vector4*>(t), 3);
        reg += 3;
    }
}

// 0x1004a4b0: from a throw-away program of the display's (its slot 0x144)
void __cdecl VertexProgram::GetMaxRegisters(Display* d)
{
    VertexProgram* const p = VCall<VertexProgram*>(d, 0x144, static_cast<VertexType*>(nullptr));
    maxLocalRegs = p->GetMaxLocalRegisters();
    maxEnvRegs = p->GetMaxEnvRegisters();
    maxTotalRegs = p->GetMaxTotalRegisters();
    p->ReleaseReference();
}

int __cdecl VertexProgram::GetMaxLocalRegisters(Display* d)
{
    if (maxLocalRegs == 0) GetMaxRegisters(d);
    return maxLocalRegs;
}
int __cdecl VertexProgram::GetMaxEnvRegisters(Display* d)
{
    if (maxEnvRegs == 0) GetMaxRegisters(d);
    return maxEnvRegs;
}
int __cdecl VertexProgram::GetMaxTotalRegisters(Display* d)
{
    if (maxTotalRegs == 0) GetMaxRegisters(d);
    return maxTotalRegs;
}

// 0x100499b0. Lights: +8 ambient, +0x18 diffuse, +0x28 specular, +0x38 attenuation, +0x48 spot cut-off (degrees),
// slot 3 its position, slot 5 its direction (type 3: directional). The display's render state block (+0x54) holds
// the ambient colour at +0x7c and the fog colour/start/end at +0x8c.
void __cdecl VertexProgram::LoadCommonStates(const Array<ConstantData*, 3>& constants, const DisplayLightList& lights,
                                             Color ambient, const DisplayMaterial& material,
                                             const Matrix4& view) const
{
    const float* v = F(&view);
    const float m0 = v[0], m4 = v[4], m8 = v[8], m1 = v[1], m5 = v[5], m9 = v[9], m2 = v[2], m6 = v[6], m10 = v[10];
    const float* mat = F(&material);
    const float* col = F(&ambient);
    LightsByType sorted(lights);
    auto put = [this](int reg, const float* q) { SetLocalConstantDirect(reg, *reinterpret_cast<const Vector4*>(q)); };

    for (unsigned int i = 0; i < constants.count; ++i) {
        const ConstantData* const c = constants.data[i];
        if (c->kind != 4) continue;
        const int reg = c->reg;
        float q[4];
        switch (c->state) {
        case 0x12: put(reg, mat); break;       // material ambient
        case 0x13: put(reg, mat + 4); break;   // diffuse
        case 0x14: put(reg, mat + 8); break;   // specular
        case 0x15: put(reg, mat + 12); break;  // emissive
        case 0x16:                             // power
            q[0] = mat[16];
            q[1] = 0.0f;
            q[2] = 0.0f;
            q[3] = 1.0f;
            put(reg, q);
            break;
        case 0x17:
        case 0x18:
        case 0x19: {  // light ambient / diffuse / specular
            const DisplayLight* l = sorted.Find(c->lightIndex, c->lightType);
            put(reg, l ? F(l) + 2 + 4 * (c->state - 0x17) : kZero);
            break;
        }
        case 0x1a: {  // light position (direction) in view space
            const DisplayLight* l = sorted.Find(c->lightIndex, c->lightType);
            if (!l) {
                put(reg, kZero);
            } else if (LightType(l) == 3) {
                const float* d = VCall<const float*>(l, 0x14);
                const float x0 = -0.0f - d[0], x1 = -0.0f - d[1], x2 = -0.0f - d[2];
                q[0] = m8 * x2 + m4 * x1 + m0 * x0;
                q[1] = m9 * x2 + m5 * x1 + m1 * x0;
                q[2] = m10 * x2 + m6 * x1 + m2 * x0;
                q[3] = 0.0f;
                put(reg, q);
            } else {
                const Vector3 p = Transform(view, VCall<const float*>(l, 0xc));
                SetLocalConstantDirect(reg, p);
            }
            break;
        }
        case 0x1b: {  // attenuation
            const DisplayLight* l = sorted.Find(c->lightIndex, c->lightType);
            put(reg, l ? F(l) + 14 : kZero);
            break;
        }
        case 0x1c: {  // spot direction in view space, w = cos(cut-off)
            const DisplayLight* l = sorted.Find(c->lightIndex, c->lightType);
            if (!l) {
                put(reg, kZero);
                break;
            }
            const float cutoff = F(l)[18];
            const float* d = VCall<const float*>(l, 0x14);
            const float d0 = d[0], d1 = d[1], d2 = d[2];
            q[3] = X87Cos(static_cast<double>(cutoff) * static_cast<double>(0.017453292f));
            q[0] = d0 * m0 + d2 * m8 + d1 * m4;
            q[1] = d0 * m1 + d2 * m9 + d1 * m5;
            q[2] = d0 * m2 + d2 * m10 + d1 * m6;
            put(reg, q);
            break;
        }
        case 0x1d: {  // half vector: (to the light) + (0, 0, 1), normalised
            const DisplayLight* l = sorted.Find(c->lightIndex, c->lightType);
            if (!l) {
                put(reg, kZero);
                break;
            }
            float h[3];
            if (LightType(l) == 3) {
                const float* d = VCall<const float*>(l, 0x14);
                const float x0 = -0.0f - d[0], x1 = -0.0f - d[1], x2 = -0.0f - d[2];
                h[0] = x1 * m4 + x2 * m8 + x0 * m0;
                h[1] = x1 * m5 + x2 * m9 + m1 * x0;
                h[2] = x1 * m6 + x2 * m10 + m2 * x0;
            } else {
                const Vector3 p = Transform(view, VCall<const float*>(l, 0xc));
                const float* pp = F(&p);
                h[0] = pp[0];
                h[1] = pp[1];
                h[2] = pp[2];
            }
            h[2] = h[2] + 1.0f;
            Normalize(h);
            q[0] = h[0];
            q[1] = h[1];
            q[2] = h[2];
            q[3] = 1.0f;
            put(reg, q);
            break;
        }
        case 0x1e: put(reg, col); break;  // the ambient colour
        case 0x1f:
        case 0x20:  // ambient * material ambient + emissive, alpha of the diffuse
            q[0] = col[0] * mat[0] + mat[12];
            q[1] = mat[13] + mat[1] * col[1];
            q[2] = mat[14] + mat[2] * col[2];
            q[3] = mat[7];
            put(reg, q);
            break;
        case 0x21:
        case 0x22:
        case 0x23: {  // light colour * material colour (ambient / diffuse / specular)
            const DisplayLight* l = sorted.Find(c->lightIndex, c->lightType);
            if (!l) {
                q[0] = 0.0f;
                q[1] = 0.0f;
                q[2] = 0.0f;
            } else {
                const int k = 4 * (c->state - 0x21);
                const float* lc = F(l) + 2 + k;
                q[0] = mat[k] * lc[0];
                q[1] = lc[1] * mat[k + 1];
                q[2] = lc[2] * mat[k + 2];
            }
            q[3] = mat[7];
            put(reg, q);
            break;
        }
        case 0x24:
        case 0x25:
        case 0x26: {
            const char* state = *reinterpret_cast<char* const*>(reinterpret_cast<const char*>(display) + 0x54);
            put(reg, F(state + 0x7c));
            break;
        }
        case 0x27: {  // fog: colour value, start, end, 1 / (end - start)
            const float* s = F(*reinterpret_cast<char* const*>(reinterpret_cast<const char*>(display) + 0x54) + 0x8c);
            q[0] = s[0];
            q[1] = s[1];
            q[2] = s[2];
            q[3] = 1.0f / (s[2] - s[1]);
            put(reg, q);
            break;
        }
        default:
            break;
        }
    }
}

}  // namespace Jet
