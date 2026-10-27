// Unit U5a of RenderSystem_7c: VertexType (0x100562b0..0x100572a0, with the accessors at 0x100106e0..0x10010940)
// and its registry. Behaviour follows the TS12 assembly (addresses per function). The module statics (`display`,
// the registry) stay the original's; the item-type defaults (0x100b8968, filled at module start) and the element
// sizes (0x10062760) are this module's own copies.
#include <jet/Geometry.h>
#include <jet/VCall.h>

#include <new>

// VS2005 mangled `T const * const` statics with P..B, today's compilers with Q..B
#pragma comment(linker, "/alternatename:__imp_?display@VertexType@Jet@@1QBVDisplay@2@B=__imp_?display@VertexType@Jet@@1PBVDisplay@2@B")

namespace Jet {

namespace {

using Node = PStringCache::Node;
using Item = VertexType::Item;
using Stream = VertexType::Stream;

// 0x100b8968: per item type {elements, data type, semantic, name, the type itself} (0x14), made at module start
struct ItemDefaults {
    int count;
    int dataType;
    int semantic;
    PString name;
    int type;
};
const struct {
    int count, dataType, semantic;
    const char* name;
} kItemDefaults[kVertexItemTypes] = {
    {0, 0, 0, "Undefined"},      {3, 7, 2, "Position"},       {1, 8, 5, "Color0"},         {1, 8, 5, "Color1"},
    {3, 7, 3, "Normal"},         {1, 7, 7, "Weights1"},       {2, 7, 7, "Weights2"},       {3, 7, 7, "Weights3"},
    {4, 7, 7, "Weights4"},       {1, 8, 7, "Weights4b"},      {1, 8, 6, "WeightIndex"},    {2, 7, 6, "Weights1Index"},
    {3, 7, 6, "Weights2Index"},  {4, 7, 6, "Weights3Index"},  {1, 7, 0, "Fog"},            {2, 7, 4, "Texture0"},
    {2, 7, 4, "Texture1"},       {2, 7, 4, "Texture2"},       {2, 7, 4, "Texture3"},       {2, 7, 4, "Texture4"},
    {2, 7, 4, "Texture5"},       {2, 7, 4, "Texture6"},       {2, 7, 4, "Texture7"},       {4, 7, 0, "Attrib0"},
    {4, 7, 0, "Attrib1"},        {4, 7, 0, "Attrib2"},        {4, 7, 0, "Attrib3"},        {4, 7, 0, "Attrib4"},
    {4, 7, 0, "Attrib5"},        {4, 7, 0, "Attrib6"},        {4, 7, 0, "Attrib7"},        {4, 7, 0, "Attrib8"},
    {4, 7, 0, "Attrib9"},        {4, 7, 0, "AttribA"},        {4, 7, 0, "AttribB"},        {4, 7, 0, "AttribC"},
    {4, 7, 0, "AttribD"},        {4, 7, 0, "AttribE"},        {4, 7, 0, "AttribF"},
};

struct ItemTable {
    ItemDefaults e[kVertexItemTypes];
    ItemTable()
    {
        for (int k = 0; k < kVertexItemTypes; ++k) {
            e[k].count = kItemDefaults[k].count;
            e[k].dataType = kItemDefaults[k].dataType;
            e[k].semantic = kItemDefaults[k].semantic;
            e[k].name.node =
                PString::Cache()->Create(reinterpret_cast<const unsigned char*>(kItemDefaults[k].name));
            e[k].type = k;
        }
    }
};
ItemTable& Defaults()
{
    static ItemTable table;
    return table;
}
// made with the module, like the original's
const ItemTable& kMadeAtStart = Defaults();

// 0x10062760: per data type {bytes per element, the BoolEnum default, the type itself}
const struct {
    unsigned int size;
    unsigned int flag;
    unsigned int type;
} kElements[9] = {{0, 0, 0}, {1, 1, 1}, {1, 1, 2}, {2, 1, 3}, {2, 1, 4}, {4, 1, 5}, {4, 1, 6}, {4, 0, 7}, {4, 1, 8}};

// 0x1000ddd0: the old node goes first
void Assign(PString& dst, const PString& src)
{
    if (dst.node) PString::Cache()->Destroy(dst.node);
    ::new (&dst) PString(src);
}

// an Alloc block of `n` (saturating like the original's new[])
void* NewBlock(unsigned int n, unsigned int each, unsigned int extra)
{
    const unsigned long long bytes = static_cast<unsigned long long>(n) * each;
    unsigned int size = bytes >> 32 ? 0xffffffffu : static_cast<unsigned int>(bytes);
    if (extra) size = size > 0xffffffffu - extra ? 0xffffffffu : size + extra;
    return Alloc::New(size);
}

// 0x10056700 / 0x10056f70: the count, then `n` items with only their names made
Item* NewItems(unsigned int n)
{
    unsigned int* const block = static_cast<unsigned int*>(NewBlock(n, sizeof(Item), 4));
    if (!block) return nullptr;
    *block = n;
    Item* const items = reinterpret_cast<Item*>(block + 1);
    for (unsigned int k = 0; k < n; ++k) ::new (&items[k]) Item();
    return items;
}
// the vector destructor: last to first, then the block
void DeleteItems(Item* items)
{
    unsigned int* const block = reinterpret_cast<unsigned int*>(items) - 1;
    for (unsigned int k = *block; k-- > 0;) items[k].~Item();
    Alloc::Delete(block);
}

// 0x10056680 / 0x10056400: `n` zeroed streams
Stream* NewStreams(unsigned int n)
{
    Stream* const s = static_cast<Stream*>(NewBlock(n, sizeof(Stream), 0));
    if (s)
        for (unsigned int k = 0; k < n; ++k) s[k] = Stream{0, 0, 0, nullptr};
    return s;
}

// 0x10056f70: a stream with copies of all its items (the ones not added yet too)
void CopyStream(Stream& d, const Stream& s)
{
    d.stride = s.stride;
    d.capacity = s.capacity;
    d.count = s.count;
    d.items = NewItems(s.capacity);
    for (unsigned int k = 0; k < d.capacity; ++k) {  // 0x10056ef0
        Item& a = d.items[k];
        const Item& b = s.items[k];
        a.type = b.type;
        Assign(a.name, b.name);
        a.offset = b.offset;
        a.count = b.count;
        a.dataType = b.dataType;
        a.semantic = b.semantic;
        a.sharedWith = b.sharedWith;
        a.flag = b.flag;
    }
}

// 0x10056310
bool SameStream(const Stream& a, const Stream& b)
{
    if (a.stride != b.stride || a.capacity != b.capacity) return false;
    for (unsigned int k = 0; k < a.capacity; ++k) {
        const Item& x = a.items[k];
        const Item& y = b.items[k];
        if (x.type != y.type || x.offset != y.offset || x.count != y.count || x.dataType != y.dataType ||
            x.semantic != y.semantic || x.sharedWith != y.sharedWith || x.flag != y.flag)
            return false;
    }
    return true;
}

const void* RegistrationKey(VertexType::RegistrationData* r) { return r->name.node; }

}  // namespace

// 0x100563c0
VertexType::VertexType(VertexTypeID i)
    : id(i), streamCount(0), streams(nullptr), apiData(0), apiFlags(0), user(0)
{
}

// 0x10056400: the API data is announced to the display again (slot 0x3c)
VertexType::VertexType(const VertexType& o)
    : ReferenceCount(),
      id(o.id),
      streamCount(o.streamCount),
      apiData(o.apiData),
      apiFlags(o.apiFlags),
      user(o.user),
      registeredName(o.registeredName)
{
    if (apiData && display) VCall<void>(display, 0x3c, &apiData);
    streams = NewStreams(streamCount);
    for (unsigned int k = 0; k < streamCount; ++k) CopyStream(streams[k], o.streams[k]);
}

// 0x10056550: the API data goes back to the display (slot 0x40)
VertexType::~VertexType()
{
    for (unsigned int k = 0; k < streamCount; ++k) {
        if (streams[k].items) DeleteItems(streams[k].items);
        streams[k].items = nullptr;
    }
    if (streams) Alloc::Delete(streams);
    streams = nullptr;
    if (apiData && display) VCall<void>(display, 0x40, &apiData);
}

// 0x10056c40
bool __cdecl VertexType::operator==(const VertexType& o) const
{
    if (!CompareVertex(o)) return false;
    return id == o.id && registeredName.node == o.registeredName.node;
}

// 0x10056c80
bool __cdecl VertexType::CompareVertex(const VertexType& o) const
{
    if (streamCount != o.streamCount) return false;
    for (unsigned int k = 0; k < streamCount; ++k)
        if (!SameStream(streams[k], o.streams[k])) return false;
    return true;
}

// 0x10056680: the old streams are not freed
void __cdecl VertexType::CreateStreams(unsigned int count)
{
    streams = NewStreams(count);
    streamCount = count;
    for (unsigned int k = 0; k < count; ++k) {
        streams[k].count = 0;
        streams[k].capacity = 0;
    }
}

// 0x10056700
void __cdecl VertexType::CreateItems(unsigned int stream, unsigned int count)
{
    streams[stream].items = NewItems(count);
    streams[stream].capacity = count;
}

// 0x10056800: -1 elements, data type 0, semantic 1 and an empty name take the item type's defaults
void __cdecl VertexType::AddItem(unsigned int stream, VertexItemType type, int count, DVertexItemDataType dataType,
                                 VertexItemSemanticType semantic, const PString& name, int offset, BoolEnum flag)
{
    Stream& s = streams[stream];
    if (s.count >= s.capacity) return;
    Item& i = s.items[s.count];
    const ItemDefaults& d = Defaults().e[type];
    i.offset = offset;
    i.type = type;
    i.sharedWith = -1;
    i.count = count == -1 ? d.count : count;
    i.dataType = dataType == 0 ? d.dataType : dataType;
    i.semantic = semantic == 1 ? d.semantic : semantic;
    i.flag = flag == -1 ? kElements[i.dataType].flag != 0 : flag == 1;
    Assign(i.name, name.node && name.node->length ? name : d.name);
    ++s.count;
    ConfigureStream(&s);
}

// 0x100568e0: a second name for the data of `sharedWith` (same stream, offset and format; the flag is left as is)
void __cdecl VertexType::AddItem(VertexItemType type, VertexItemType sharedWith, const PString& name)
{
    const int k = GetItemStream(sharedWith);
    if (k < 0) return;
    Stream& s = streams[k];
    if (s.count >= s.capacity) return;
    Item* const i = &s.items[s.count];
    unsigned int j = 0;
    for (; j < s.count; ++j)
        if (s.items[j].type == sharedWith) break;
    if (j == s.count) return;
    const Item& o = s.items[j];
    i->type = type;
    i->sharedWith = sharedWith;
    i->offset = o.offset;
    i->count = o.count;
    i->dataType = o.dataType;
    i->semantic = o.semantic;
    Assign(i->name, name.node && name.node->length ? name : Defaults().e[type].name);
    ++s.count;
    ConfigureStream(&s);
}

// 0x100569a0: items placed one after another (an offset given stays), shared ones on the item they share (their
// sharedWith then moves down by 15); the stride is the furthest end
void __cdecl VertexType::ConfigureStream(Stream* s)
{
    if (s->count != s->capacity) return;
    int end = 0;
    int stride = 0;
    for (unsigned int k = 0; k < s->capacity; ++k) {
        Item* const items = s->items;
        Item& i = items[k];
        const int shared = i.sharedWith;
        if (shared < 0) {
            if (i.offset == -1) i.offset = end;
            end = static_cast<int>(kElements[i.dataType].size) * i.count + i.offset;
            if (end > stride) stride = end;
        } else {
            for (unsigned int j = 0; j < k; ++j)
                if (items[j].type == shared) {
                    i.offset = items[j].offset;
                    i.sharedWith = shared - 0xf;
                    break;
                }
        }
    }
    s->stride = stride;
}

// 0x100567b0: the old data goes back to the new display (slot 0x40); the display is kept for every type
void __cdecl VertexType::SetApiData(const Display* d, unsigned long data)
{
    if (apiData && d) VCall<void>(d, 0x40, &apiData);
    apiData = data;
    *const_cast<const Display**>(&display) = d;
}

unsigned long __cdecl VertexType::GetApiData() const { return apiData; }      // 0x10004aa0
unsigned long __cdecl VertexType::GetApiFlags() const { return apiFlags; }    // 0x1000ae00
void __cdecl VertexType::SetApiFlags(unsigned long f) { apiFlags = f; }       // 0x10021a60
unsigned long __cdecl VertexType::GetUserData() const { return user; }        // 0x10010940
void __cdecl VertexType::SetUserData(unsigned long d) { user = d; }           // 0x10021b90
VertexTypeID __cdecl VertexType::GetVertexTypeID() const { return id; }       // 0x1000cab0
unsigned int __cdecl VertexType::GetNumStreams() const { return streamCount; }  // 0x100106c0
const VertexType::Stream* __cdecl VertexType::GetStreamList() const { return streams; }  // 0x100106d0
const PString& __cdecl VertexType::GetRegisteredName() const { return registeredName; }  // 0x1000b2b0

// 0x100106e0
unsigned int __cdecl VertexType::GetVertexSize(unsigned int stream) const { return streams[stream].stride; }

// 0x10010700: the first added item of that type
const VertexType::Item* __cdecl VertexType::GetItem(VertexItemType type) const
{
    for (unsigned int k = 0; k < streamCount; ++k)
        for (unsigned int j = 0; j < streams[k].count; ++j)
            if (streams[k].items[j].type == type) return &streams[k].items[j];
    return nullptr;
}

// 0x10056a70
int __cdecl VertexType::GetItemOffset(VertexItemType type) const
{
    const Item* const i = GetItem(type);
    return i ? i->offset : -1;
}

// 0x10056ad0
int __cdecl VertexType::GetItemStream(VertexItemType type) const
{
    for (unsigned int k = 0; k < streamCount; ++k)
        for (unsigned int j = 0; j < streams[k].count; ++j)
            if (streams[k].items[j].type == type) return static_cast<int>(k);
    return -1;
}

// 0x10056b20
DVertexItemDataType __cdecl VertexType::GetItemDataType(VertexItemType type) const
{
    const Item* const i = GetItem(type);
    return static_cast<DVertexItemDataType>(i ? i->dataType : 9);
}

// 0x10056b80
VertexItemSemanticType __cdecl VertexType::GetItemSemanticType(VertexItemType type) const
{
    const Item* const i = GetItem(type);
    return static_cast<VertexItemSemanticType>(i ? i->semantic : 0);
}

// 0x10056be0
int __cdecl VertexType::GetItemElementCount(VertexItemType type) const
{
    const Item* const i = GetItem(type);
    return i ? i->count : -1;
}

// 0x10010750: the first added item with that name
bool __cdecl VertexType::GetNamedItemData(const PString& name, unsigned int& stream, unsigned int& item,
                                          const Stream*& s, const Item*& i) const
{
    for (unsigned int k = 0; k < streamCount; ++k)
        for (unsigned int j = 0; j < streams[k].count; ++j)
            if (streams[k].items[j].name.node == name.node) {
                stream = k;
                item = j;
                i = &streams[k].items[j];
                s = &streams[k];
                return true;
            }
    return false;
}

// 0x100107e0: the item types in stream order, added at the end
void __cdecl VertexType::GetVertexItemTypes(
    FastDynamicArray<VertexItemType, 13, Less<VertexItemType, VertexItemType>>& out) const
{
    for (unsigned int k = 0; k < streamCount; ++k)
        for (unsigned int j = 0; j < streams[k].count; ++j) out.Add(streams[k].items[j].type);
}

// 0x100108d0: every item type with that semantic (`max` is not looked at)
int __cdecl VertexType::GetCompatibleVertexItemTypes(VertexItemSemanticType semantic, VertexItemType* out,
                                                     int) const
{
    int n = 0;
    for (unsigned int k = 0; k < streamCount; ++k)
        for (unsigned int j = 0; j < streams[k].count; ++j)
            if (streams[k].items[j].semantic == semantic) out[n++] = streams[k].items[j].type;
    return n;
}

// 0x10056cd0
bool __cdecl VertexType::RegisterName(const PString& name)
{
    Assign(registeredName, name);
    if (registrations.Find(name.node, RegistrationKey)) return false;
    RegistrationData* const r = new RegistrationData;  // 0x10056220
    if (r) {
        r->type = this;
        ::new (&r->name) PString(name);
        AddReference();
    }
    registrations.Insert(r, r->name.node);
    return true;
}

// 0x10056dc0 / 0x10056dd0
unsigned int __cdecl VertexType::GetElementTypeSize(DVertexItemDataType t) { return kElements[t].size; }
const PString& __cdecl VertexType::GetDefaultVertexItemName(VertexItemType t) { return Defaults().e[t].name; }

// 0x10056de0
VertexType* __cdecl VertexType::GetVertexType(const PString& name)
{
    RegistrationData** const r = registrations.Find(name.node, RegistrationKey);
    if (!r) return nullptr;
    (*r)->type->AddReference();
    return (*r)->type;
}

// 0x10056e20
void __cdecl VertexType::Shutdown()
{
    registrations.ForEach([](RegistrationData*& r) {
        if (r) {
            r->type->ReleaseReference();
            delete r;
        }
        r = nullptr;
    });
    registrations.Clear();  // 0x10057180
}

}  // namespace Jet
