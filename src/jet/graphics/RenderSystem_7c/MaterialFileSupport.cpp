#include <jet/RenderSystem.h>

#include <new>

namespace Jet {
MaterialFileSupport::MaterialFileSupport() = default;
MaterialFileSupport& MaterialFileSupport::operator=(const MaterialFileSupport&) { return *this; }
namespace {

struct NameValue { const char* name; int value; };
const NameValue textureTypes[] = {
    {"ambient", 0}, {"diffuse", 1}, {"specular", 2}, {"shine", 3}, {"shinstrength", 4},
    {"selfillum", 5}, {"opacity", 6}, {"filtercolor", 7}, {"bump", 8}, {"reflect", 9},
    {"refract", 10}, {"displacement", 11}, {"lookup0", 12}, {"lookup1", 13}, {"lookup2", 14}, {"lookup3", 15}
};
const NameValue sources[] = {
    {"zero", 0}, {"one", 1}, {"dstColor", 2}, {"oneMinusDstColor", 3}, {"srcAlpha", 4},
    {"oneMinusSrcAlpha", 5}, {"dstAlpha", 6}, {"oneMinusDstAlpha", 7}, {"srcAlphaSaturate", 8},
    {"dst_Color", 2}, {"one_Minus_Dst_Color", 3}, {"src_Alpha", 4}, {"one_Minus_Src_Alpha", 5},
    {"dst_Alpha", 6}, {"one_Minus_Dst_Alpha", 7}, {"src_Alpha_Saturate", 8}, {"", -1}
};
const NameValue destinations[] = {
    {"zero", 0}, {"one", 1}, {"srcColor", 2}, {"oneMinusSrcColor", 3}, {"srcAlpha", 4},
    {"oneMinusSrcAlpha", 5}, {"dstAlpha", 6}, {"oneMinusDstAlpha", 7}, {"src_Color", 2},
    {"one_Minus_Src_Color", 3}, {"src_Alpha", 4}, {"one_Minus_Src_Alpha", 5}, {"dst_Alpha", 6},
    {"one_Minus_Dst_Alpha", 7}, {"", -1}
};
const NameValue functions[] = {
    {"Never", 0}, {"Less", 1}, {"LessEqual", 2}, {"Equal", 3}, {"GreaterEqual", 4},
    {"Greater", 5}, {"NotEqual", 6}, {"Always", 7}
};

template <unsigned N>
int FindValue(const PString& name, const NameValue (&table)[N], int fallback, unsigned count = N)
{
    for (unsigned i = 0; i < count; ++i)
        if (!name.CompareIgnoreCase(table[i].name)) return table[i].value;
    return fallback;
}

template <unsigned N>
PString* FindName(PString* out, int value, const NameValue (&table)[N], const char* fallback)
{
    for (const auto& item : table)
        if (item.value == value) return ::new (out) PString(item.name);
    return ::new (out) PString(fallback);
}

}  // namespace

int __cdecl RsMaterialTextureType(const PString& name) { return FindValue(name, textureTypes, -1); }
int __cdecl RsMaterialSourceValue(const PString& name) { return FindValue(name, sources, 1, 16); }
int __cdecl RsMaterialDestinationValue(const PString& name) { return FindValue(name, destinations, 0, 14); }
int __cdecl RsMaterialFunctionValue(const PString& name) { return FindValue(name, functions, 7); }
PString* __cdecl RsMaterialSourceName(PString* out, int value) { return FindName(out, value, sources, "InvalidSrcFactor"); }
PString* __cdecl RsMaterialDestinationName(PString* out, int value) { return FindName(out, value, destinations, "InvalidDstFactor"); }

PString* __cdecl RsMaterialFunctionName(PString* out, int value)
{
    // The original tests the function table but reads the name from the destination table.
    if (value >= 0 && value < 8) return ::new (out) PString(destinations[value].name);
    return ::new (out) PString("InvalidDFunction");
}

}  // namespace Jet
