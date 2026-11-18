// RenderSystem_7c unit U7 (notes/RenderSystem.md), the material manager. TS12 layout and slot order (vftable
// 0x100714fc): the hash table of materials keyed by their decorated name node, the texture manager it hands
// out and the default material type ("notex" until set). Only the lifetime and the table are ours so far:
// Create (0x10035670) still runs the original's plugin-type walk.
#pragma once

#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Result.h>

namespace Jet {

class Material;
class RenderKernel;
class TextureManager;

class MaterialManager : public ReferenceCount {
public:
    enum Results {};
    static __declspec(dllimport) const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    explicit MaterialManager(TextureManager* textures);               // ??0MaterialManager@Jet@@QAE@PAVTextureManager@1@@Z
    ~MaterialManager() override;                                      // 1 (0 is ReferenceCount::DeleteThis)

    virtual bool __cdecl Init(RenderKernel* kernel);                  // 2 (0x1000c920)
    virtual Result __cdecl Create(Material*& material, const PString& name);  // 3 (0x10035670: the original's)
    virtual void __cdecl Add(Material* material);                     // 4 (0x10035ad0)
    virtual Result __cdecl Remove(Material* material);                // 5 (0x10035af0)
    virtual Material* __cdecl Find(const PString& name);              // 6 (0x10035b50)

    JET_HEAP_OBJECT

    struct MaterialHash {};
    // the table by the decorated name node (0x10042d80 init, 0x10035db0 clear, the buckets freed last):
    // 97 FastDynamicArray buckets of Material*, chunks of 13
    struct Materials : HashTable<void, Material*, MaterialHash, 13> {
        Materials() { Init(); }
        ~Materials() { Destroy(); }
    };

    // (public for the layout checks and the module's own field access, like PStringCache)
    RenderKernel* kernel;      // +0x08
    TextureManager* textures;  // +0x0c (referenced)
    PString defaultType;       // +0x10 "notex" until SetDefaultMaterialType
    Materials materials;       // +0x14
};

JET_LAYOUT32(sizeof(MaterialManager) == 0x20);
JET_LAYOUT32(offsetof(MaterialManager, kernel) == 0x08 && offsetof(MaterialManager, textures) == 0x0c);
JET_LAYOUT32(offsetof(MaterialManager, defaultType) == 0x10 && offsetof(MaterialManager, materials) == 0x14);

}  // namespace Jet
