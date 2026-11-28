#include "ShaderLoaderStub.h"

#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>

#include <new>

namespace {

class StubTechnique final : public Jet::CGShaderTechnique {
public:
    int __cdecl Slot0() override { return 1; }
    int __cdecl Slot1() override { return 1; }
    int __cdecl Slot2() override { return 1; }
    int __cdecl Slot3() override { return 1; }
    int __cdecl Slot4() override { return 1; }
};

class StubConstant final : public Jet::CGShaderConstant {
public:
    bool __cdecl Set1f(float) const override { return true; }
    bool __cdecl Set2f(float, float) const override { return true; }
    bool __cdecl Set3f(float, float, float) const override { return true; }
    bool __cdecl Set4f(float, float, float, float) const override { return true; }
    bool __cdecl SetArray1f(const float*, int, int) const override { return true; }
    bool __cdecl SetArray3f(const float*, int, int) const override { return true; }
    bool __cdecl SetArray4f(const float*, int, int) const override { return true; }
    bool __cdecl SetMatrix4x4(const float* const) const override { return true; }
    bool __cdecl SetMatrix4x4Array(const float*, unsigned int, unsigned int) const override { return true; }
    bool __cdecl SetMatrix4x4Index(const float* const, unsigned int) const override { return true; }
    bool __cdecl SetMatrix4x4Transpose(const float* const) const override { return true; }
    bool __cdecl SetTexture(Jet::DisplayTexture*) const override { return true; }
    int __cdecl Slot12() override { return 1; }
};

class StubLoader final : public Jet::CGShaderLoader {
public:
    int __cdecl Slot1() override { return 0; }
    int __cdecl Slot2() override { return 0; }
    int __cdecl Slot3() override { return 0; }
    Jet::CGShaderTechnique* CreateTechnique(const char* name) override
    {
        (void)name;
        void* const memory = Jet::Alloc::New(sizeof(StubTechnique));
        return memory ? ::new (memory) StubTechnique() : nullptr;
    }
    Jet::CGShaderConstant* CreateConstant(const char* name) override
    {
        (void)name;
        void* const memory = Jet::Alloc::New(sizeof(StubConstant));
        return memory ? ::new (memory) StubConstant() : nullptr;
    }
};

}

Jet::CGShaderLoader* __cdecl CreateShaderLoaderStub()
{
    void* const memory = Jet::Alloc::New(sizeof(StubLoader));
    return memory ? ::new (memory) StubLoader() : nullptr;
}
