// ElementLoader exports for RenderSystem_7c.
// Behaviour/layout follows TS12 exports/decomp/original bytes.
namespace Jet {

class ElementLoader {
public:
    ElementLoader();                                 // 0x10020430
    ElementLoader(const ElementLoader&);             // 0x10020440
    virtual ~ElementLoader();                        // 0x10020420
    ElementLoader& __thiscall operator=(const ElementLoader&);  // 0x10038c50
};

ElementLoader::ElementLoader() {}  // vptr set by compiler

ElementLoader::ElementLoader(const ElementLoader&)
{
    // vptr remains this class's vftable (compiler sets it)
}

ElementLoader::~ElementLoader() {}

ElementLoader& __thiscall ElementLoader::operator=(const ElementLoader&)
{
    return *this;
}

}  // namespace Jet
