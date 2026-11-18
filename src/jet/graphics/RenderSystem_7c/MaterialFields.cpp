#include <cstdint>
#include <cstring>
#include <new>
#include <jet/PString.h>
#include <jet/KernelStandard.h>
#include <windows.h>

namespace Jet {
namespace {
template<class T> T& Field(void* p, unsigned offset) { return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset); }
void* Offset(void* p, unsigned offset) { return static_cast<unsigned char*>(p) + offset; }
struct MaterialProperty { PString key, value; };
struct MaterialTextureDefinition { int type; PString name; float weight; };
template<class T> struct MaterialChunk {
    MaterialChunk* next;
    MaterialChunk* prev;
    unsigned count;
    alignas(T) unsigned char bytes[7 * sizeof(T)];
    T* item(unsigned i) { return reinterpret_cast<T*>(bytes) + i; }
};
template<class T> void ClearMaterialList(void* object, unsigned offset)
{
    auto*& head = Field<MaterialChunk<T>*>(object, offset);
    auto* first = head;
    if (!first) return;
    head = nullptr;
    auto* current = first->next;
    while (current != first) {
        auto* next = current->next;
        for (unsigned i = 0; i < current->count; ++i) current->item(i)->~T();
        Alloc::Delete(current);
        current = next;
    }
    for (unsigned i = 0; i < first->count; ++i) first->item(i)->~T();
    Alloc::Delete(first);
}
template<class T> void AppendMaterialItem(void* object, unsigned offset, const T& item)
{
    auto*& head = Field<MaterialChunk<T>*>(object, offset);
    auto* tail = head ? head->prev : nullptr;
    if (!tail || tail->count == 7) {
        auto* chunk = static_cast<MaterialChunk<T>*>(Alloc::New(sizeof(MaterialChunk<T>)));
        chunk->count = 0;
        if (!head) { head = chunk; chunk->next = chunk->prev = chunk; }
        else { chunk->next = head; chunk->prev = tail; tail->next = chunk; head->prev = chunk; }
        tail = chunk;
    }
    T* target = tail->item(tail->count++);
    ::new (target) T(item);
}
}
void* __cdecl RsMatPtr34(void* p) { return Offset(p, 0x34); }
void* __cdecl RsMatPtr24(void* p) { return Offset(p, 0x24); }
void* __cdecl RsMatPtr28(void* p) { return Offset(p, 0x28); }
void* __cdecl RsMatPtr2c(void* p) { return Offset(p, 0x2c); }
void* __cdecl RsMatPtr30(void* p) { return Offset(p, 0x30); }
void* __cdecl RsMatPtr44(void* p) { return Offset(p, 0x44); }
void* __cdecl RsMatPtr54(void* p) { return Offset(p, 0x54); }
void* __cdecl RsMatPtr64(void* p) { return Offset(p, 0x64); }
void* __cdecl RsMatPtr74(void* p) { return Offset(p, 0x74); }
void* __cdecl RsMatPtr80(void* p) { return Offset(p, 0x80); }
void* __cdecl RsMatPtr88(void* p) { return Offset(p, 0x88); }
void* __cdecl RsMatPtr90(void* p) { return Offset(p, 0x90); }
void* __cdecl RsMatManager(void* p) { return Field<void*>(p, 0x98); }
unsigned __cdecl RsMatTextureCount(void* p) { return Field<unsigned>(p, 0x8c); }
unsigned __cdecl RsMatPropertyCount(void* p)
{
    auto* first = Field<unsigned char*>(p, 0x88);
    if (!first) return 0;
    unsigned count = 0;
    auto* chunk = first;
    do { count += Field<unsigned>(chunk, 8); chunk = Field<unsigned char*>(chunk, 0); } while (chunk != first);
    return count;
}
float __cdecl RsMatPriority(void* p) { return Field<float>(p, 0x94); }
float __cdecl RsMatOpacity(void* p) { return Field<float>(p, 0x80); }
unsigned __cdecl RsMatSourceBlend(void* p) { return Field<unsigned>(p, 0x78); }
bool __cdecl RsMatFallback(void* p) { return (Field<unsigned char>(p, 0x84) & 8) != 0; }
bool __cdecl RsMatStandardBlend(void* p) { return (Field<unsigned char>(p, 0x84) & 2) != 0; }
bool __cdecl RsMatTwoSided(void* p) { return (Field<unsigned char>(p, 0x84) & 1) != 0; }
bool __cdecl RsMatZBiased(void* p) { return (Field<unsigned char>(p, 0x84) & 4) != 0; }
void __cdecl RsMatSetFallback(void* p) { Field<unsigned char>(p, 0x84) |= 8; }
void __cdecl RsMatSetTwoSided(void* p, bool v) { auto& f = Field<unsigned char>(p, 0x84); f = (f & ~1) | (v & 1); }
void __cdecl RsMatSetZBiased(void* p, bool v) { auto& f = Field<unsigned char>(p, 0x84); f = (f & ~4) | ((v & 1) << 2); }
void __cdecl RsMatSetOpacity(void* p, float v) { Field<float>(p, 0x80) = v; }
void __cdecl RsMatSetPriority(void* p, float v) { Field<float>(p, 0x94) = v; }
void __cdecl RsMatGetBlend(void* p, int* source, int* destination) { *source = Field<int>(p, 0x78); *destination = Field<int>(p, 0x7c); }
void __cdecl RsMatSetBlend(void* p, int source, int destination)
{
    Field<int>(p, 0x78) = source;
    Field<int>(p, 0x7c) = destination;
    auto& f = Field<unsigned char>(p, 0x84);
    f = (f & ~2) | (source == 1 && destination == 0 ? 2 : 0);
}
void __cdecl RsMatSetMaterial(void* p, void* v) { std::memcpy(Offset(p, 0x34), v, 0x44); }
void __cdecl RsMatSetAmbient(void* p, void* v) { std::memcpy(Offset(p, 0x34), v, 16); }
void __cdecl RsMatSetDiffuse(void* p, void* v) { std::memcpy(Offset(p, 0x44), v, 16); }
void __cdecl RsMatSetSpecular(void* p, void* v) { std::memcpy(Offset(p, 0x54), v, 16); }
void __cdecl RsMatSetEmissive(void* p, void* v) { std::memcpy(Offset(p, 0x64), v, 16); }
void __cdecl RsMatSetRequestName(void* p, PString* v)
{
    auto* target = static_cast<PString*>(Offset(p, 0x30));
    target->~PString();
    ::new (target) PString(*v);
}
void __cdecl RsMatSetCreationType(void* p, PString* v)
{
    auto* target = static_cast<PString*>(Offset(p, 0x2c));
    target->~PString();
    ::new (target) PString(*v);
}
void* __cdecl RsMatManagerDefault(void* p) { return Offset(p, 0x10); }
void __cdecl RsMatManagerSetDefault(void* p, PString* v)
{
    auto* target = static_cast<PString*>(Offset(p, 0x10));
    target->~PString();
    ::new (target) PString(*v);
}
PString* __cdecl RsMatGetProperty(void* p, const PString* key, bool* found)
{
    auto* first = Field<unsigned char*>(p, 0x88);
    if (first) {
        auto* chunk = first;
        do {
            auto* item = reinterpret_cast<PString*>(chunk + 12);
            auto* end = item + 2 * Field<unsigned>(chunk, 8);
            do {
                if (item->node == key->node) {
                    if (found) *found = true;
                    return item + 1;
                }
                item += 2;
            } while (item < end);
            chunk = Field<unsigned char*>(chunk, 0);
        } while (chunk != first);
    }
    if (found) *found = false;
    static auto* const missing = reinterpret_cast<PString*>(GetProcAddress(
        GetModuleHandleA("RenderSystem_7c_orig.dll"), "?STRING_NOT_FOUND@Material@Jet@@2VPString@2@A"));
    return missing;
}
PString* __cdecl RsMatGetPropertySimple(void* p, const PString* key) { return RsMatGetProperty(p, key, nullptr); }
unsigned long __cdecl RsMatSortKey(void* p)
{
    auto* node = Field<PStringCache::Node*>(p, 0x24);
    return node ? PString::Cache()->GetHash(reinterpret_cast<const unsigned char*>(node->text), node->length) : 0;
}
PString* __cdecl RsMatNonUniqueName(void* p, PString* out)
{
    const auto* source = static_cast<PString*>(Offset(p, 0x24));
    const char* text = source->Text();
    const int marker = source->node ? PString::Cache()->Find(source->node, reinterpret_cast<const unsigned char*>("%%"), 8) : -1;
    if (marker == 8) {
        ::new (out) PString();
        if (source->Length() > 9)
            out->node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(text + 10), source->Length() - 10);
        return out;
    }
    return ::new (out) PString(*source);
}
void __cdecl RsMatSetName(void* p, const PString* decorated, const PString* plain)
{
    auto* first = static_cast<PString*>(Offset(p, 0x24));
    auto* second = static_cast<PString*>(Offset(p, 0x28));
    first->~PString(); ::new (first) PString(*decorated);
    second->~PString(); ::new (second) PString(*plain);
}
unsigned __cdecl RsMatGeometryFlags(int type, const void* hint)
{
    static const unsigned defaults[] = {4, 8, 16, 1, 2, 0};
    unsigned flags = defaults[type];
    if (hint && (static_cast<const unsigned char*>(hint)[8] & 1)) {
        const unsigned char bits = static_cast<const unsigned char*>(hint)[0x10];
        if (bits & 1) flags |= 0x20;
        if (bits & 4) flags |= 0x40;
    }
    return flags;
}
float __fastcall RsMatAssetVersion(void* p, void*) { return Field<float>(p, 0x20); }
void __fastcall RsMatSetAssetVersion(void* p, void*, float value) { Field<float>(p, 0x20) = value; }
int __cdecl RsMatDstBlendMode(void* p) { return Field<int>(p, 0x7c); }
void __cdecl RsMaterialInit(void* p)
{
    for (unsigned offset = 0x34; offset < 0x64; offset += 4) Field<float>(p, offset) = 1.0f;
    for (unsigned offset = 0x64; offset < 0x70; offset += 4) Field<float>(p, offset) = 0.0f;
    Field<float>(p, 0x70) = 1.0f;
    Field<unsigned char>(p, 0x84) = (Field<unsigned char>(p, 0x84) & 0xf2) | 2;
    Field<float>(p, 0x74) = 0;
    Field<int>(p, 0x78) = 1;
    Field<int>(p, 0x7c) = 0;
    Field<float>(p, 0x80) = 1.0f;
    Field<float>(p, 0x94) = 0;
    Field<unsigned>(p, 0x8c) = 0;
    ClearMaterialList<MaterialProperty>(p, 0x88);
    ClearMaterialList<MaterialTextureDefinition>(p, 0x90);
}
void __cdecl RsMaterialAddProperty(void* p, const PString* key, const PString* value)
{
    auto* head = Field<MaterialChunk<MaterialProperty>*>(p, 0x88);
    if (head) {
        auto* chunk = head;
        do {
            unsigned i = 0;
            do {
                auto* item = chunk->item(i++);
                if (item->key.node == key->node) {
                    item->value.~PString();
                    ::new (&item->value) PString(*value);
                    return;
                }
            } while (i < chunk->count);
            chunk = chunk->next;
        } while (chunk != head);
    }
    MaterialProperty item{*key, *value};
    AppendMaterialItem(p, 0x88, item);
}
}
