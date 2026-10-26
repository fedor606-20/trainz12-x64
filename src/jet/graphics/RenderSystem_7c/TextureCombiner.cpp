// Unit U3a of RenderSystem_7c: TextureCombiner, the base of the API modules' texture combiners (0x100401f0..0x100407f0,
// the bump settings at 0x100102b0..0x100106c0). Behaviour follows the TS12 assembly (addresses per function). The
// support flags stay the original's statics until U9.
#include <jet/GfxLanguage.h>
#include <jet/VCall.h>

#include <cstring>

#include "GfxParse.h"

namespace Jet {

namespace {

const unsigned int* U(const void* p) { return static_cast<const unsigned int*>(p); }

// an AnsiString's length (its block: {refs, length, chars})
unsigned int Length(const AnsiString& s) { return U(*reinterpret_cast<char* const*>(&s))[1]; }

template <class T>
void FreeItems(Array<T*, 128>& a)
{
    for (unsigned int i = 0; i < a.count; ++i) {
        if (a.data[i]) Alloc::Delete(a.data[i]);
        a.data[i] = nullptr;
    }
}

}  // namespace

// 0x10040260
TextureCombiner::TextureCombiner(Display* d)
    : ExtendedGfxLanguage(d), constantTable(nullptr), bumpChanged(false)  // the manager is left as it is
{
    constantTable = static_cast<Constant**>(Alloc::New(0x40));
    std::memset(constantTable, 0, 0x40);
}

// 0x10040370: the items are plain blocks; the arrays free their blocks after the constant table goes
TextureCombiner::~TextureCombiner()
{
    FreeItems(constants);
    FreeItems(bumpEnv);
    FreeItems(bumpMatrices);
    if (constantTable) Alloc::Delete(constantTable);
    constantTable = nullptr;
}

// 0x10040500 / 0x100404d0: the display's capability 0x15, asked once
bool __cdecl TextureCombiner::SupportedByPlatform(Display* d, float)
{
    if (!supportChecked) {
        supported = VCall<bool>(d, 0x1c, 0x15);
        supportChecked = true;
    }
    return supported;
}

bool __cdecl TextureCombiner::SupportedByPlatform(float generation) { return SupportedByPlatform(display, generation); }

void __cdecl TextureCombiner::SetCombinerManager(TextureCombinerManager* m) { manager = m; }
TextureCombinerManager* __cdecl TextureCombiner::GetCombinerManager() { return manager; }
bool __cdecl TextureCombiner::AddInstruction(const Instruction*) { return false; }
int __cdecl TextureCombiner::GetApi() const { return 0; }

// 0x10040550
bool __cdecl TextureCombiner::Load(MemoryFile& file, const PString&, const PString& fileName)
{
    SetFilename(fileName);
    return TextureProgramParse(&file, GetApi(), fileName, this);
}

// 0x10040680 (the table is read before the index is checked, as in the original)
bool __cdecl TextureCombiner::TrackConstant(int index, const AnsiString& name, const Vector4& v)
{
    if (constantTable[index] || static_cast<unsigned int>(index) > 15) return false;
    Constant* const c = static_cast<Constant*>(Alloc::New(sizeof(Constant)));
    if (c) {
        c->index = index;
        c->value = v;
    }
    constants.Add(c);
    constantTable[index] = c;
    if (Length(name)) {
        PString s(static_cast<const char*>(name));
        AddSymbolName(s, index, false, 1);
    }
    return true;
}

// 0x100407b0
bool __cdecl TextureCombiner::SetConstantRegister(int index, const Vector4& v)
{
    Constant* const c = constantTable[index];
    if (!c) return false;
    c->value = v;
    return true;
}

// 0x10010660 / 0x10010670
bool __cdecl TextureCombiner::SetConstantRegister(int index, const Color& c)
{
    return SetConstantRegister(index, reinterpret_cast<const Vector4&>(c));
}
bool __cdecl TextureCombiner::SetConstantRegister(int index, const Vector3& v)
{
    const float* p = reinterpret_cast<const float*>(&v);
    const float q[4] = {p[0], p[1], p[2], 1.0f};
    return SetConstantRegister(index, *reinterpret_cast<const Vector4*>(q));
}

// 0x100102b0 / 0x100103f0 / 0x10010530: only with `apply`; a stage seen before is updated in place, a new one
// marks the settings changed
void __cdecl TextureCombiner::SetBumpEnvScale(int stage, float scale, bool apply)
{
    if (!apply) return;
    for (unsigned int i = 0; i < bumpEnv.count; ++i)
        if (bumpEnv.data[i]->stage == stage) {
            bumpEnv.data[i]->scale = scale;
            return;
        }
    BumpEnv* const b = static_cast<BumpEnv*>(Alloc::New(sizeof(BumpEnv)));
    if (b) {
        b->scale = scale;
        b->bias = 0.0f;
        b->unused0c = 0.0f;
        b->stage = stage;
        b->unused10 = 0.0f;
    }
    bumpEnv.Add(b);
    bumpChanged = true;
}

void __cdecl TextureCombiner::SetBumpEnvBias(int stage, float bias, bool apply)
{
    if (!apply) return;
    for (unsigned int i = 0; i < bumpEnv.count; ++i)
        if (bumpEnv.data[i]->stage == stage) {
            bumpEnv.data[i]->bias = bias;
            return;
        }
    BumpEnv* const b = static_cast<BumpEnv*>(Alloc::New(sizeof(BumpEnv)));
    if (b) {
        b->scale = 1.0f;
        b->bias = bias;
        b->unused0c = 0.0f;
        b->stage = stage;
        b->unused10 = 0.0f;
    }
    bumpEnv.Add(b);
    bumpChanged = true;
}

void __cdecl TextureCombiner::SetBumpEnvMatrix(int stage, const Vector4& matrix, bool apply)
{
    if (!apply) return;
    for (unsigned int i = 0; i < bumpMatrices.count; ++i)
        if (bumpMatrices.data[i]->stage == stage) {
            bumpMatrices.data[i]->matrix = matrix;
            return;
        }
    BumpMatrix* const b = static_cast<BumpMatrix*>(Alloc::New(sizeof(BumpMatrix)));
    if (b) {
        b->stage = stage;
        b->matrix = matrix;
    }
    bumpMatrices.Add(b);
    bumpChanged = true;
}

}  // namespace Jet
