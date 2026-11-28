// Unit U4 of Render3DCore_7c: MaterialChunkCache, GenericMaterialChunkCache, ParticleFXCache and ProjectorCache.
// Transcribed from the TS12 assembly (addresses per member).
//
// MaterialChunkCache keeps 32 pipes of 8 pointers each (light list, model view and a third list at +0x20, +0x420 and
// +0x820) and calls the chunk's multipass entry points through its vtable (slot 0x3c for one entry, 0x40 for many).
// GenericMaterialChunkCache groups the same three lists by material. ParticleFXCache is a matrix/item pair vector.
// ProjectorCache is a ring of 0x40-byte blocks holding up to 13 projectors each.
#include <jet/Render3DCoreCaches.h>

#include <cstddef>
#include <cstring>

namespace Jet {

namespace {

typedef void(__cdecl* MaterialRenderChunk)(void* material, void* display, void* view, void* chunk,
                                            const void* lights, const void* modelView, int pass);
typedef void(__cdecl* MaterialRenderNumChunks)(void* material, void* display, void* view, void** chunks,
                                               void** lights, void** modelViews, unsigned int count, int pass);

// 0x100030f0: {count, capacity, data} growth to hold need items
void GrowList(int* list, unsigned need)
{
    if (need <= static_cast<unsigned>(list[1])) return;
    const unsigned capacity = (need + 0x7f) & ~0x7fu;
    void** const data = static_cast<void**>(Alloc::New(capacity * 4));
    if (list[2]) {
        std::memcpy(data, reinterpret_cast<void*>(list[2]), static_cast<size_t>(list[0]) * 4);
        Alloc::Delete(reinterpret_cast<void*>(list[2]));
    }
    list[1] = static_cast<int>(capacity);
    list[2] = reinterpret_cast<int>(data);
}

// 0x10025f40: the per-entry lists grow in steps of 32 (the caches' own lists use 128)
void GrowList32(int* list, unsigned need)
{
    if (need <= static_cast<unsigned>(list[1])) return;
    const unsigned capacity = (need + 0x1f) & ~0x1fu;
    void** const data = static_cast<void**>(Alloc::New(capacity * 4));
    if (list[2]) {
        std::memcpy(data, reinterpret_cast<void*>(list[2]), static_cast<size_t>(list[0]) * 4);
        Alloc::Delete(reinterpret_cast<void*>(list[2]));
    }
    list[1] = static_cast<int>(capacity);
    list[2] = reinterpret_cast<int>(data);
}

void AppendPointer32(int* list, void* value)
{
    GrowList32(list, static_cast<unsigned>(list[0]) + 1);
    if (list[2]) reinterpret_cast<void**>(list[2])[list[0]] = value;
    ++list[0];
}

void AppendPointer(int* list, void* value)
{
    GrowList(list, static_cast<unsigned>(list[0]) + 1);
    if (list[2]) reinterpret_cast<void**>(list[2])[list[0]] = value;
    ++list[0];
}

// 0x10053050: the same for 8-byte pairs
void GrowPairs(int* list, unsigned need)
{
    if (need <= static_cast<unsigned>(list[1])) return;
    const unsigned capacity = (need + 0x7f) & ~0x7fu;
    void** const data = static_cast<void**>(Alloc::New(capacity * 8));
    if (list[2]) {
        std::memcpy(data, reinterpret_cast<void*>(list[2]), static_cast<size_t>(list[0]) * 8);
        Alloc::Delete(reinterpret_cast<void*>(list[2]));
    }
    list[1] = static_cast<int>(capacity);
    list[2] = reinterpret_cast<int>(data);
}

inline void* Vtbl(void* object, int byteOffset)
{
    void** const table = *static_cast<void***>(object);
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(table) + byteOffset);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// MaterialChunkCache

// 0x10010280
void __cdecl MaterialChunkCache::Reset()
{
    pipeCount = 0;
}

// 0x10073920 (shared empty Begin)
void __cdecl MaterialChunkCache::Begin(Display*, ViewPort*)
{
}

// 0x1002b710
bool __thiscall MaterialChunkCache::IsEmpty() const
{
    return pipeCount == 0;
}

// 0x100102e0: draw one pipe through the material's RenderChunk/RenderNumChunks entry point
void __cdecl MaterialChunkCache::FlushPipe(int pipe, Display* display, ViewPort* view)
{
    Material* const material = pipeMaterials[pipe];
    const int block = pipe * 8;
    if (pipeSizes[pipe] > 1) {
        reinterpret_cast<MaterialRenderNumChunks>(Vtbl(material, 0x40))(
            material, display, view, &pipeChunks[block], &pipeLights[block], &pipeModelViews[block],
            static_cast<unsigned>(pipeSizes[pipe]), -1);
    } else {
        reinterpret_cast<MaterialRenderChunk>(Vtbl(material, 0x3c))(
            material, display, view, pipeChunks[block], pipeLights[block], pipeModelViews[block], -1);
    }
}

// 0x10010290: flush every pipe, then Reset()
void __cdecl MaterialChunkCache::End(Display* display, ViewPort* view)
{
    for (int i = 0; i < pipeCount; ++i) FlushPipe(i, display, view);
    Reset();
}

// 0x10026030 (shared): Enter/Leave flush the cache when the scene enters or leaves
void __cdecl MaterialChunkCache::Enter(Display* display, ViewPort* view, bool b)
{
    if (b) Flush(display, view);
}

void __cdecl MaterialChunkCache::Leave(Display* display, ViewPort* view, bool b)
{
    if (b) Flush(display, view);
}

// 0x1002b720: find the material's pipe, append while it has fewer than eight chunks, else flush it and reuse
// the slot; a new material gets the next free pipe, or the fullest one is flushed first.
void __cdecl MaterialChunkCache::Add(Display* display, ViewPort* view, const Material* material, GeometryChunk* chunk,
                                     const DisplayLightPtrList* lights, const Matrix4* modelView)
{
    int found = -1;
    int fullest = 0;
    int fullestSize = 0;
    for (int i = 0; i < pipeCount; ++i) {
        if (pipeMaterials[i] == material) {
            found = i;
            break;
        }
        if (pipeSizes[i] > fullestSize) {
            fullestSize = pipeSizes[i];
            fullest = i;
        }
    }

    if (found >= 0) {
        if (pipeSizes[found] < 8) {
            const int slot = found * 8 + pipeSizes[found];
            pipeChunks[slot] = chunk;
            pipeLights[slot] = const_cast<DisplayLightPtrList*>(lights);
            pipeModelViews[slot] = const_cast<Matrix4*>(modelView);
            ++pipeSizes[found];
            return;
        }
        FlushPipe(found, display, view);
        const int slot = found * 8;
        pipeChunks[slot] = chunk;
        pipeLights[slot] = const_cast<DisplayLightPtrList*>(lights);
        pipeModelViews[slot] = const_cast<Matrix4*>(modelView);
        pipeSizes[found] = 1;
        return;
    }

    if (pipeCount >= 32) {
        FlushPipe(fullest, display, view);
        found = fullest;
    } else {
        found = pipeCount;
        ++pipeCount;
    }
    pipeMaterials[found] = const_cast<Material*>(material);
    const int slot = found * 8;
    pipeChunks[slot] = chunk;
    pipeLights[slot] = const_cast<DisplayLightPtrList*>(lights);
    pipeModelViews[slot] = const_cast<Matrix4*>(modelView);
    pipeSizes[found] = 1;
}

// ---------------------------------------------------------------------------------------------------------------
// GenericMaterialChunkCache

// 0x10025c30
void __cdecl GenericMaterialChunkCache::Reset()
{
    for (int i = 0; i < used; ++i) {
        entries[i].count0 = 0;
        entries[i].count1 = 0;
        entries[i].count2 = 0;
    }
    used = 0;
}

// 0x10025b70: Begin is Reset through the vtable (slot 2)
void __cdecl GenericMaterialChunkCache::Begin(Display* display, ViewPort* view)
{
    (void)display;
    (void)view;
    Reset();
}

// 0x10026030 (shared)
void __cdecl GenericMaterialChunkCache::Enter(Display* display, ViewPort* view, bool b)
{
    if (b) Flush(display, view);
}

void __cdecl GenericMaterialChunkCache::Leave(Display* display, ViewPort* view, bool b)
{
    if (b) Flush(display, view);
}

// 0x10025c80
void __cdecl GenericMaterialChunkCache::Add(Display*, ViewPort*, const Material* material, GeometryChunk* chunk,
                                             const DisplayLightPtrList* lights, const Matrix4* modelView)
{
    int index = used;
    for (int i = 0; i < index; ++i) {
        if (entries[i].material == material) {
            index = i;
            goto found;
        }
    }
    if (index < constructed) {
        entries[index].material = const_cast<Material*>(material);
    } else {
        if (static_cast<unsigned>(constructed) + 1 > static_cast<unsigned>(capacity)) {
            const unsigned count = (static_cast<unsigned>(constructed) + 1 + 0x1f) & ~0x1fu;
            Entry* const data = static_cast<Entry*>(Alloc::New(count * 0x28));
            if (entries) {
                std::memcpy(data, entries, static_cast<size_t>(constructed) * 0x28);
                Alloc::Delete(entries);
            }
            entries = data;
            capacity = static_cast<int>(count);
        }
        Entry& entry = entries[constructed];
        std::memset(&entry, 0, sizeof entry);
        entry.material = const_cast<Material*>(material);
        ++constructed;
    }
    index = used;
    ++used;
found:
    {
        Entry& entry = entries[index];
        AppendPointer32(&entry.count0, chunk);
        AppendPointer32(&entry.count1, const_cast<DisplayLightPtrList*>(lights));
        AppendPointer32(&entry.count2, const_cast<Matrix4*>(modelView));
    }
}

// 0x10025f90: one RenderChunk per live entry (fewer than two chunks) or RenderNumChunks with the three lists
void __cdecl GenericMaterialChunkCache::End(Display* display, ViewPort* view)
{
    for (int i = 0; i < used; ++i) {
        Entry& entry = entries[i];
        if (entry.count0 > 1) {
            reinterpret_cast<MaterialRenderNumChunks>(Vtbl(entry.material, 0x40))(
                entry.material, display, view, entry.data0, entry.data1, entry.data2,
                static_cast<unsigned>(entry.count0), -1);
        } else {
            reinterpret_cast<MaterialRenderChunk>(Vtbl(entry.material, 0x3c))(
                entry.material, display, view, entry.data0 ? entry.data0[0] : nullptr,
                entry.data1 ? entry.data1[0] : nullptr, entry.data2 ? entry.data2[0] : nullptr, -1);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// ParticleFXCache

// 0x10052f70: append {matrix, item}; the pair's first word is the matrix
void __cdecl ParticleFXCache::Add(const ParticleFXRenderItem* item, const Matrix4* matrix)
{
    GrowPairs(&count, static_cast<unsigned>(count) + 1);
    if (data) {
        data[count * 2 + 0] = const_cast<Matrix4*>(matrix);
        data[count * 2 + 1] = const_cast<ParticleFXRenderItem*>(item);
    }
    ++count;
}

// 0x10052fb0
void __cdecl ParticleFXCache::Reset()
{
    count = 0;
}

void __cdecl ParticleFXCache::Begin(Display*, ViewPort*)
{
    count = 0;
}

// 0x10052fc0: alpha blend on with (1,0), lighting and depth writes off, then every item's Render(item, display,
// view, matrix); afterwards depth writes on and alpha blending off
void __cdecl ParticleFXCache::End(Display* display, ViewPort* view)
{
    typedef void(__cdecl* RenderFn)(const ParticleFXRenderItem*, Display*, ViewPort*, const Matrix4*);

    display->SetAlphaBlendMode(true);
    display->SetAlphaBlendFunction(static_cast<Src::Factor>(1), static_cast<Dst::Factor>(0));
    display->SetDepthWriteMode(false);
    display->SetLightingMode(false);
    for (int i = 0; i < count; ++i) {
        const ParticleFXRenderItem* const item = static_cast<const ParticleFXRenderItem*>(data[i * 2 + 1]);
        const Matrix4* const matrix = static_cast<const Matrix4*>(data[i * 2 + 0]);
        reinterpret_cast<RenderFn>(Vtbl(const_cast<ParticleFXRenderItem*>(item), 0))(item, display, view, matrix);
    }
    display->SetDepthWriteMode(true);
    display->SetAlphaBlendMode(false);
}

// ---------------------------------------------------------------------------------------------------------------
// ProjectorCache

// 0x10058ab0: free the whole ring
void __cdecl ProjectorCache::Reset()
{
    Block* const first = blocks;
    if (!first) return;
    Block* p = first->next;
    while (p != first) {
        Block* const next = p->next;
        Alloc::Delete(p);
        p = next;
    }
    Alloc::Delete(first);
    blocks = nullptr;
}

void __cdecl ProjectorCache::Begin(Display*, ViewPort*)
{
    Reset();
}

// 0x10058b00: every projector's slot 1 (Render(projector, display, view))
void __cdecl ProjectorCache::End(Display* display, ViewPort* view)
{
    typedef void(__cdecl* ProjectorRender)(Projector*, Display*, ViewPort*);

    Block* const first = blocks;
    if (!first) return;
    Block* block = first;
    for (;;) {
        for (int i = 0; i < block->count; ++i) {
            Projector* const projector = block->items[i];
            reinterpret_cast<ProjectorRender>(Vtbl(projector, 4))(projector, display, view);
        }
        block = block->next;
        if (block == first) return;
    }
}

// 0x10058b60 -> 0x1003ca30: append to the current block, allocating a new 13-slot block when it is full
void __cdecl ProjectorCache::Add(Projector* projector)
{
    Block* block = blocks;
    if (!block) {
        block = static_cast<Block*>(Alloc::New(0x40));
        if (block) {
            block->count = 0;
            blocks = block;
            block->prev = block;
            block->next = block;
        } else {
            blocks = nullptr;
            return;
        }
    } else {
        Block* const last = block->prev;
        if (last->count == 13) {
            Block* const fresh = static_cast<Block*>(Alloc::New(0x40));
            if (fresh) fresh->count = 0;
            fresh->next = blocks;
            fresh->prev = last;
            blocks->prev = fresh;
            last->next = fresh;
            block = fresh;
        } else {
            block = last;
        }
    }
    block->items[block->count] = projector;
    ++block->count;
}

}  // namespace Jet
