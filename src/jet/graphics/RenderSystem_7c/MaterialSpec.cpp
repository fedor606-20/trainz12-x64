#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>

#include <cstdint>
#include <new>

namespace Jet {
namespace {

struct TextureEntry { PString first, second; };
struct ParameterEntry { std::uint32_t kind; PString name; std::uint32_t value; };

// MaterialSpec uses seven-element ring chunks; copy preserves the original chunk boundaries.
template <class T> class MaterialEntries {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        unsigned count;
        alignas(T) unsigned char storage[7 * sizeof(T)];
        T* item(unsigned i) { return reinterpret_cast<T*>(storage) + i; }
        const T* item(unsigned i) const { return reinterpret_cast<const T*>(storage) + i; }
    };
public:
    MaterialEntries() : head(nullptr) {}
    MaterialEntries(const MaterialEntries& other) : head(nullptr) { Append(other); }
    ~MaterialEntries() { Clear(); }
    MaterialEntries& operator=(const MaterialEntries& other) { Clear(); Append(other); return *this; }
private:
    void Clear()
    {
        Chunk* first = head;
        if (!first) return;
        head = nullptr;
        Chunk* chunk = first->next;
        while (chunk != first) {
            Chunk* next = chunk->next;
            for (unsigned i = 0; i < chunk->count; ++i) chunk->item(i)->~T();
            Alloc::Delete(chunk);
            chunk = next;
        }
        for (unsigned i = 0; i < first->count; ++i) first->item(i)->~T();
        Alloc::Delete(first);
    }
    void Append(const MaterialEntries& other)
    {
        const Chunk* source = other.head;
        if (!source) return;
        do {
            Chunk* chunk = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
            chunk->count = source->count;
            for (unsigned i = 0; i < source->count; ++i)
                ::new (chunk->item(i)) T(*source->item(i));
            if (!head) {
                head = chunk;
                chunk->prev = chunk->next = chunk;
            } else {
                chunk->next = head;
                chunk->prev = head->prev;
                head->prev->next = chunk;
                head->prev = chunk;
            }
            source = source->next;
        } while (source != other.head);
    }
    Chunk* head;
};
}  // namespace

struct MaterialSpec {
    MaterialSpec();
    explicit MaterialSpec(const PString& name);
    MaterialSpec(const MaterialSpec& other);
    MaterialSpec& operator=(const MaterialSpec& other);
    ~MaterialSpec();

    PString name, shader;
    DisplayMaterial material;
    std::uint32_t values[3];
    unsigned char flag;
    MaterialEntries<TextureEntry> textures;
    MaterialEntries<ParameterEntry> parameters;
    unsigned char flags[8];
};
JET_LAYOUT32(sizeof(MaterialSpec) == 0x6c);

MaterialSpec::MaterialSpec() : flags{} {}
MaterialSpec::MaterialSpec(const PString& n) : name(n), flags{} {}
MaterialSpec::MaterialSpec(const MaterialSpec& o)
    : name(o.name), shader(o.shader), material(o.material), values{o.values[0], o.values[1], o.values[2]},
      flag(o.flag), textures(o.textures), parameters(o.parameters)
{
    for (unsigned i = 0; i < 8; ++i) flags[i] = o.flags[i];
}
MaterialSpec& MaterialSpec::operator=(const MaterialSpec& o)
{
    name.~PString();
    ::new (&name) PString(o.name);
    shader.~PString();
    ::new (&shader) PString(o.shader);
    material = o.material;
    for (unsigned i = 0; i < 3; ++i) values[i] = o.values[i];
    flag = o.flag;
    textures = o.textures;
    parameters = o.parameters;
    for (unsigned i = 0; i < 8; ++i) flags[i] = o.flags[i];
    return *this;
}
MaterialSpec::~MaterialSpec() = default;
}  // namespace Jet
