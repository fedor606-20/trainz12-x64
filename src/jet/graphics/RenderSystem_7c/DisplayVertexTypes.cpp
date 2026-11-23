// Build the legacy vertex formats from the declarative descriptions extracted at build time.
#include <jet/Geometry.h>
#include <jet/RenderSystem.h>

#include <iterator>
#include <new>

namespace Jet {
namespace {
struct VertexAttribute {
    unsigned stream;
    int item, count, kind, semantic, offset, flag;
    const char* name;
};

struct VertexRecipe {
    unsigned first, length, streams;
    unsigned capacities[3];
    const char* name;
};

#include "rs_vertex_recipes.h"
}

void __cdecl RsDisplayCreateVertexType(Display* display, int id)
{
    if (id < 0 || static_cast<unsigned>(id) >= std::size(recipes)) return;
    auto*& slot = reinterpret_cast<VertexType**>(reinterpret_cast<unsigned char*>(display) + 0x80)[id];
    auto* storage = Alloc::New(sizeof(VertexType));
    slot = storage ? ::new (storage) VertexType(static_cast<VertexTypeID>(id)) : nullptr;
    if (!slot) return;

    const auto& recipe = recipes[id];
    slot->CreateStreams(recipe.streams);
    for (unsigned stream = 0; stream < recipe.streams; ++stream)
        slot->CreateItems(stream, recipe.capacities[stream]);
    for (unsigned i = recipe.first; i < recipe.first + recipe.length; ++i) {
        const auto& attr = attributes[i];
        const auto name = attr.name ? PString(attr.name) : PString();
        slot->AddItem(attr.stream, static_cast<VertexItemType>(attr.item), attr.count,
                      static_cast<DVertexItemDataType>(attr.kind), static_cast<VertexItemSemanticType>(attr.semantic),
                      name, attr.offset, static_cast<VertexType::BoolEnum>(attr.flag));
    }
    slot->RegisterName(PString(recipe.name));
    VCall<void>(display, 0x38, slot, 0, 0);
}

}  // namespace Jet
