// Unit U7 of RenderSystem_7c: MaterialSpec's lifetime (0x10034af0 / 0x10034b80 / 0x10033730 / 0x100337d0 /
// 0x10033910). The struct and its seven-item ring chunks live in jet/Material.h; the copy preserves the
// original chunk boundaries.
#include <jet/Material.h>

#include <new>

namespace Jet {

MaterialSpec::MaterialSpec() : flags{} {}
MaterialSpec::MaterialSpec(const PString& n) : name(n), flags{} {}
MaterialSpec::MaterialSpec(const MaterialSpec& o)
    : name(o.name), rename(o.rename), material(o.material), values{o.values[0], o.values[1], o.values[2]},
      flag(o.flag), properties(o.properties), textures(o.textures)
{
    for (unsigned i = 0; i < 8; ++i) flags[i] = o.flags[i];
}
MaterialSpec& MaterialSpec::operator=(const MaterialSpec& o)
{
    name.~PString();
    ::new (&name) PString(o.name);
    rename.~PString();
    ::new (&rename) PString(o.rename);
    material = o.material;
    for (unsigned i = 0; i < 3; ++i) values[i] = o.values[i];
    flag = o.flag;
    properties = o.properties;
    textures = o.textures;
    for (unsigned i = 0; i < 8; ++i) flags[i] = o.flags[i];
    return *this;
}
MaterialSpec::~MaterialSpec() = default;
}  // namespace Jet
