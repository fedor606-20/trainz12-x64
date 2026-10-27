// Unit U3b2 of RenderSystem_7c: CGShaderLoader / CGShaderTechnique / CGShaderConstant (0x10009180..0x1000a940,
// with the MSVC8 std::map code they instantiate) and GLSLShaderBase (0x1002faa0..0x10031350). Behaviour follows
// the TS12 assembly (addresses per function). The loader's two maps keep the original's tree layout and the
// Dinkumware (VC8) insertion, copy and clear algorithms, since the API modules read them inline.
#include <jet/GfxLanguage.h>
#include <jet/Math.h>
#include <jet/VCall.h>

#include <cstring>
#include <vector>

namespace Jet {

namespace {

// ---- the MSVC8 _Tree<map<const char*, T*>> pieces the loader uses

using Node = ShaderMapNode;
constexpr char kRed = 0, kBlack = 1;

bool Less(const char* a, const char* b) { return std::strcmp(a, b) < 0; }  // 0x100092b0

// 0x1000a2e0: the head (no key or value)
Node* BuyHead()
{
    Node* n = static_cast<Node*>(Alloc::New(sizeof(Node)));
    n->left = nullptr;
    n->parent = nullptr;
    n->right = nullptr;
    n->color = kBlack;
    n->isNil = 0;
    return n;
}
// 0x1000a820
Node* BuyNode(Node* l, Node* p, Node* r, char* key, void* value, char color)
{
    Node* n = static_cast<Node*>(Alloc::New(sizeof(Node)));
    n->left = l;
    n->parent = p;
    n->right = r;
    n->key = key;
    n->value = value;
    n->color = color;
    n->isNil = 0;
    return n;
}

Node*& Root(ShaderMap& m) { return m.head->parent; }
Node* Min(Node* n)
{
    while (!n->left->isNil) n = n->left;
    return n;
}
Node* Max(Node* n)
{
    while (!n->right->isNil) n = n->right;
    return n;
}
Node* Next(Node* n)  // ++iterator
{
    if (!n->right->isNil) return Min(n->right);
    Node* p = n->parent;
    while (!p->isNil && n == p->right) {
        n = p;
        p = p->parent;
    }
    return p;
}
Node* Prev(Node* n)  // --iterator
{
    if (n->isNil) return n->right;
    if (!n->left->isNil) return Max(n->left);
    Node* p = n->parent;
    while (!p->isNil && n == p->left) {
        n = p;
        p = p->parent;
    }
    return p;
}

// the map's constructor (0x10009180 inline): an empty tree
void Init(ShaderMap& m)
{
    m.head = BuyHead();
    m.head->isNil = 1;
    m.head->parent = m.head;
    m.head->left = m.head;
    m.head->right = m.head;
    m.size = 0;
}

// 0x10009c30: a subtree, right first
void EraseTree(Node* n)
{
    while (!n->isNil) {
        EraseTree(n->right);
        Node* left = n->left;
        Alloc::Delete(n);
        n = left;
    }
}
// erase(begin(), end()): the whole tree
void Clear(ShaderMap& m)
{
    EraseTree(Root(m));
    Root(m) = m.head;
    m.size = 0;
    m.head->left = m.head;
    m.head->right = m.head;
}
// 0x10009c70: the map's destructor
void Destroy(ShaderMap& m)
{
    Clear(m);
    Alloc::Delete(m.head);
    m.head = nullptr;
    m.size = 0;
}

// 0x1000a1b0: a copy of a subtree, each node before its children
Node* CopyTree(ShaderMap& m, const Node* from, Node* parent)
{
    Node* root = m.head;
    if (!from->isNil) {
        Node* n = BuyNode(m.head, parent, m.head, from->key, from->value, from->color);
        if (root->isNil) root = n;
        n->left = CopyTree(m, from->left, n);
        n->right = CopyTree(m, from->right, n);
    }
    return root;
}
// 0x10009bc0
void Copy(ShaderMap& m, const ShaderMap& o)
{
    Root(m) = CopyTree(m, o.head->parent, m.head);
    m.size = o.size;
    if (!Root(m)->isNil) {
        m.head->left = Min(Root(m));
        m.head->right = Max(Root(m));
    } else {
        m.head->left = m.head;
        m.head->right = m.head;
    }
}

void RotateLeft(ShaderMap& m, Node* w)
{
    Node* p = w->right;
    w->right = p->left;
    if (!p->left->isNil) p->left->parent = w;
    p->parent = w->parent;
    if (w == Root(m)) Root(m) = p;
    else if (w == w->parent->left) w->parent->left = p;
    else w->parent->right = p;
    p->left = w;
    w->parent = p;
}
void RotateRight(ShaderMap& m, Node* w)
{
    Node* p = w->left;
    w->left = p->right;
    if (!p->right->isNil) p->right->parent = w;
    p->parent = w->parent;
    if (w == Root(m)) Root(m) = p;
    else if (w == w->parent->right) w->parent->right = p;
    else w->parent->left = p;
    p->right = w;
    w->parent = p;
}

// 0x1000a470: a new red node under `where`, then the red-black fix-up
Node* InsertAt(ShaderMap& m, bool addLeft, Node* where, char* key, void* value)
{
    Node* n = BuyNode(m.head, where, m.head, key, value, kRed);
    ++m.size;
    if (where == m.head) {
        Root(m) = n;
        m.head->left = n;
        m.head->right = n;
    } else if (addLeft) {
        where->left = n;
        if (where == m.head->left) m.head->left = n;
    } else {
        where->right = n;
        if (where == m.head->right) m.head->right = n;
    }
    for (Node* p = n; p->parent->color == kRed;) {
        if (p->parent == p->parent->parent->left) {
            Node* u = p->parent->parent->right;
            if (u->color == kRed) {
                p->parent->color = kBlack;
                u->color = kBlack;
                p->parent->parent->color = kRed;
                p = p->parent->parent;
            } else {
                if (p == p->parent->right) {
                    p = p->parent;
                    RotateLeft(m, p);
                }
                p->parent->color = kBlack;
                p->parent->parent->color = kRed;
                RotateRight(m, p->parent->parent);
            }
        } else {
            Node* u = p->parent->parent->left;
            if (u->color == kRed) {
                p->parent->color = kBlack;
                u->color = kBlack;
                p->parent->parent->color = kRed;
                p = p->parent->parent;
            } else {
                if (p == p->parent->left) {
                    p = p->parent;
                    RotateRight(m, p);
                }
                p->parent->color = kBlack;
                p->parent->parent->color = kRed;
                RotateLeft(m, p->parent->parent);
            }
        }
    }
    Root(m)->color = kBlack;
    return n;
}

// 0x1000a320: insert(value) (the key is new here)
Node* Insert(ShaderMap& m, char* key, void* value)
{
    Node* t = Root(m);
    Node* w = m.head;
    bool addLeft = true;
    while (!t->isNil) {
        w = t;
        addLeft = Less(key, t->key);
        t = addLeft ? t->left : t->right;
    }
    Node* where = w;
    if (addLeft) {
        if (where == m.head->left) return InsertAt(m, true, w, key, value);
        where = Prev(where);
    }
    if (Less(where->key, key)) return InsertAt(m, addLeft, w, key, value);
    return where;
}

// 0x10009cb0: insert(hint, value)
Node* InsertHint(ShaderMap& m, Node* where, char* key, void* value)
{
    if (m.size == 0) return InsertAt(m, true, m.head, key, value);
    if (where == m.head->left) {
        if (Less(key, where->key)) return InsertAt(m, true, where, key, value);
    } else if (where == m.head) {
        if (Less(m.head->right->key, key)) return InsertAt(m, false, m.head->right, key, value);
    } else {
        if (Less(key, where->key)) {
            Node* prev = Prev(where);
            if (Less(prev->key, key)) {
                if (prev->right->isNil) return InsertAt(m, false, prev, key, value);
                return InsertAt(m, true, where, key, value);
            }
        }
        if (Less(where->key, key)) {
            Node* next = Next(where);
            if (next == m.head || Less(key, next->key)) {
                if (where->right->isNil) return InsertAt(m, false, where, key, value);
                return InsertAt(m, true, next, key, value);
            }
        }
    }
    return Insert(m, key, value);
}

// 0x1000a260: lower_bound
Node* LowerBound(ShaderMap& m, const char* key)
{
    Node* p = Root(m);
    Node* w = m.head;
    while (!p->isNil) {
        if (Less(p->key, key)) {
            p = p->right;
        } else {
            w = p;
            p = p->left;
        }
    }
    return w;
}
// 0x100099d0: find
Node* Find(ShaderMap& m, const char* key)
{
    Node* w = LowerBound(m, key);
    return w == m.head || Less(key, w->key) ? m.head : w;
}
// 0x10009a60: operator[]
void*& At(ShaderMap& m, char* key)
{
    Node* w = LowerBound(m, key);
    if (w == m.head || Less(key, w->key)) w = InsertHint(m, w, key, nullptr);
    return w->value;
}

std::vector<Node*> InOrder(ShaderMap& m)
{
    std::vector<Node*> v;
    for (Node* n = m.head->left; n != m.head; n = Next(n)) v.push_back(n);
    return v;
}

// a copy of `name` on the Jet heap (the map's key)
char* KeyCopy(const char* name)
{
    const size_t n = std::strlen(name);
    char* k = static_cast<char*>(Alloc::New(static_cast<unsigned int>(n + 1)));
    std::memcpy(k, name, n);
    k[n] = 0;
    return k;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// CGShaderTechnique / CGShaderConstant (0x10009460..0x10009590): empty bases

CGShaderTechnique::CGShaderTechnique() {}
CGShaderTechnique::CGShaderTechnique(const CGShaderTechnique&) {}
CGShaderTechnique& CGShaderTechnique::operator=(const CGShaderTechnique&) { return *this; }
CGShaderTechnique::~CGShaderTechnique() {}
int __cdecl CGShaderTechnique::Slot0() { return 0; }
int __cdecl CGShaderTechnique::Slot1() { return 0; }
int __cdecl CGShaderTechnique::Slot2() { return 0; }
int __cdecl CGShaderTechnique::Slot3() { return 0; }
int __cdecl CGShaderTechnique::Slot4() { return 0; }

CGShaderConstant::CGShaderConstant() {}
CGShaderConstant::CGShaderConstant(const CGShaderConstant&) {}
CGShaderConstant& CGShaderConstant::operator=(const CGShaderConstant&) { return *this; }
CGShaderConstant::~CGShaderConstant() {}
bool __cdecl CGShaderConstant::Set1f(float) const { return false; }
bool __cdecl CGShaderConstant::Set2f(float, float) const { return false; }
bool __cdecl CGShaderConstant::Set3f(float, float, float) const { return false; }
bool __cdecl CGShaderConstant::Set4f(float, float, float, float) const { return false; }
bool __cdecl CGShaderConstant::SetArray1f(const float*, int, int) const { return false; }
bool __cdecl CGShaderConstant::SetArray3f(const float*, int, int) const { return false; }
bool __cdecl CGShaderConstant::SetArray4f(const float*, int, int) const { return false; }
bool __cdecl CGShaderConstant::SetMatrix4x4(const float* const) const { return false; }
bool __cdecl CGShaderConstant::SetMatrix4x4Array(const float*, unsigned int, unsigned int) const { return false; }
bool __cdecl CGShaderConstant::SetMatrix4x4Index(const float* const, unsigned int) const { return false; }
bool __cdecl CGShaderConstant::SetMatrix4x4Transpose(const float* const) const { return false; }
bool __cdecl CGShaderConstant::SetTexture(DisplayTexture*) const { return false; }
int __cdecl CGShaderConstant::Slot12() { return 0; }

// ---------------------------------------------------------------------------------------------------------------
// CGShaderLoader

// 0x10009180
CGShaderLoader::CGShaderLoader() : lastError("")
{
    Init(techniques);
    Init(constants);
}

// 0x100092f0 / 0x10009940: the trees copied node by node (the names and objects shared)
CGShaderLoader::CGShaderLoader(const CGShaderLoader& o) : lastError(static_cast<const AnsiString&>(o.lastError))
{
    Init(techniques);
    Copy(techniques, o.techniques);
    Init(constants);
    Copy(constants, o.constants);
}

// 0x10009370
CGShaderLoader& CGShaderLoader::operator=(const CGShaderLoader& o)
{
    lastError = o.lastError;
    if (&techniques != &o.techniques) {
        Clear(techniques);
        Copy(techniques, o.techniques);
    }
    if (&constants != &o.constants) {
        Clear(constants);
        Copy(constants, o.constants);
    }
    return *this;
}

// 0x10009230
CGShaderLoader::~CGShaderLoader()
{
    Release();
    Destroy(constants);
    Destroy(techniques);
}

int __cdecl CGShaderLoader::Slot1() { return 0; }
int __cdecl CGShaderLoader::Slot2() { return 0; }
int __cdecl CGShaderLoader::Slot3() { return 0; }
CGShaderTechnique* CGShaderLoader::CreateTechnique(const char*) { return nullptr; }
CGShaderConstant* CGShaderLoader::CreateConstant(const char*) { return nullptr; }

// 0x10009590: each map's names and objects in key order, then its nodes
void CGShaderLoader::Release()
{
    for (Node* n : InOrder(techniques)) {
        Alloc::Delete(n->key);
        if (n->value) delete static_cast<CGShaderTechnique*>(n->value);
    }
    Clear(techniques);
    for (Node* n : InOrder(constants)) {
        Alloc::Delete(n->key);
        if (n->value) delete static_cast<CGShaderConstant*>(n->value);
    }
    Clear(constants);
}

// 0x10009760: by name, else made by slot 4 and kept under a copy of the name
CGShaderTechnique* __cdecl CGShaderLoader::GetShader(const char* name)
{
    Node* n = Find(techniques, name);
    if (n != techniques.head) return static_cast<CGShaderTechnique*>(n->value);
    CGShaderTechnique* t = CreateTechnique(name);
    if (!t) {
        lastError.Setf("Cannot find/create technique named [%s]", name);
        return nullptr;
    }
    At(techniques, KeyCopy(name)) = t;
    return t;
}

// 0x10009850
CGShaderConstant* __cdecl CGShaderLoader::GetShaderConstant(const char* name)
{
    Node* n = Find(constants, name);
    if (n != constants.head) return static_cast<CGShaderConstant*>(n->value);
    CGShaderConstant* c = CreateConstant(name);
    if (!c) {
        lastError.Setf("Failed to find/create shader constant [%s]", name);
        return nullptr;
    }
    At(constants, KeyCopy(name)) = c;
    return c;
}

const String& __cdecl CGShaderLoader::GetLastError() const { return lastError; }  // 0x1003ebc0

// ---------------------------------------------------------------------------------------------------------------
// GLSLShaderBase

// 0x1002fb10: the default attribute names
GLSLShaderBase::GLSLShaderBase(bool modes)
    : paramCount(0), paramCapacity(0), params(nullptr), attributeCount(0), attributeCapacity(0), attributes(nullptr),
      lightModes(modes)
{
    BindVertexAttribute(PString("position"), static_cast<VertexItemType>(1));
    BindVertexAttribute(PString("texture0uv"), static_cast<VertexItemType>(0xf));
    BindVertexAttribute(PString("color0"), static_cast<VertexItemType>(2));
    BindVertexAttribute(PString("normal"), static_cast<VertexItemType>(4));
}

// 0x1002fcc0
GLSLShaderBase::~GLSLShaderBase()
{
    for (unsigned int i = 0; i < paramCount; ++i) {
        if (Parameter* p = params[i]) {
            p->name.~PString();
            Alloc::Delete(p);
        }
        params[i] = nullptr;
    }
    for (unsigned int i = 0; i < attributeCount; ++i) attributes[i].name.~PString();
    attributeCount = 0;
    if (attributes) Alloc::Delete(attributes);
    paramCount = 0;
    if (params) Alloc::Delete(params);
}

// 0x1002fde0: the name of the attribute of that type, or a new one
void __cdecl GLSLShaderBase::BindVertexAttribute(const PString& n, VertexItemType type)
{
    for (unsigned int i = 0; i < attributeCount; ++i)
        if (attributes[i].type == type) {
            PString& a = attributes[i].name;
            if (a.node) PString::Cache()->Destroy(a.node);
            ::new (&a) PString(n);
            return;
        }
    const PString copy(n);
    if (attributeCapacity < attributeCount + 1) {
        const unsigned int quads = (attributeCount + 4) >> 2;
        auto* d = static_cast<Attribute*>(Alloc::New(quads * 0x20));
        if (attributes) {
            std::memcpy(d, attributes, attributeCount * sizeof(Attribute));
            Alloc::Delete(attributes);
        }
        attributes = d;
        attributeCapacity = quads * 4;
    }
    Attribute* a = &attributes[attributeCount];
    if (a) {
        ::new (&a->name) PString(copy);
        a->type = type;
    }
    ++attributeCount;
}

int __cdecl GLSLShaderBase::IndexForLightMode(LightMode mode) const { return lightModes ? mode : 0; }  // 0x1002fb00
bool __cdecl GLSLShaderBase::IsTextureUsed(LightMode mode, const PString& n) const  // 0x10030100
{
    return GetUniformLocation(mode, n) >= 0;
}
const PString& __cdecl GLSLShaderBase::GetName() const { return name; }  // 0x1001d560

int __cdecl GLSLShaderBase::Slot2() { return 0; }
int __cdecl GLSLShaderBase::Slot3() { return 0; }
int __cdecl GLSLShaderBase::Slot4() { return 0; }
int __cdecl GLSLShaderBase::Slot5() { return 0; }
int __cdecl GLSLShaderBase::Slot6() { return 0; }
int __cdecl GLSLShaderBase::Slot7() { return 0; }
int __cdecl GLSLShaderBase::GetUniformLocation(int, const PString&) const { return 0; }

// 0x100309a0..0x10031030 (one per value size): the uniform of that name, or a new record; a new one asks the API
// for its location in each light mode and is found (state 2) under `type` when any answers
GLSLShaderBase::Parameter* GLSLShaderBase::Resolve(const PString& n, unsigned int size, int type)
{
    Parameter* p = nullptr;
    for (unsigned int i = 0; i < paramCount; ++i)
        if (params[i]->name.node == n.node) {
            p = params[i];
            break;
        }
    if (!p) {
        p = static_cast<Parameter*>(Alloc::New(size));
        if (p) {
            p->name.node = nullptr;
            p->state = 0;
            for (int k = 0; k < 3; ++k) {
                p->location[k] = -1;
                p->dirty[k] = false;
            }
        }
        ::new (&p->name) PString(n);
        if (paramCapacity < paramCount + 1) {
            const unsigned int quads = (paramCount + 4) >> 2;
            auto* d = static_cast<Parameter**>(Alloc::New(quads * 0x10));
            if (params) {
                std::memcpy(d, params, paramCount * 4);
                Alloc::Delete(params);
            }
            params = d;
            paramCapacity = quads * 4;
        }
        if (Parameter** slot = &params[paramCount]) *slot = p;
        ++paramCount;
    }
    if (p->state == 0) {
        p->state = 1;
        const int last = lightModes ? 2 : 0;
        for (int k = 0; k <= last; ++k) {
            p->location[k] = GetUniformLocation(k, n);
            if (p->location[k] >= 0) {
                p->type = type;
                p->state = 2;
            }
        }
    }
    return p->state == 2 && p->type == type ? p : nullptr;
}

namespace {

void MarkChanged(GLSLShaderBase::Parameter* p, bool transpose, int count)
{
    p->transpose = transpose;
    p->dirty[0] = p->dirty[1] = p->dirty[2] = true;
    p->count = count;
}

// a float-by-float change (NaN counts as changed)
bool Differs(const float* a, const float* b, int n)
{
    for (int i = 0; i < n; ++i)
        if (a[i] != b[i]) return true;
    return false;
}

}  // namespace

// 0x1002ffc0..0x100300e0 with 0x10030170..0x100308b0
void __cdecl GLSLShaderBase::SetParameter(const PString& n, float v)
{
    if (Parameter* p = Resolve(n, 0x28, 0); p && p->value[0] != v) {
        p->value[0] = v;
        MarkChanged(p, false, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameter(const PString& n, const Vector2& v)
{
    const float* f = reinterpret_cast<const float*>(&v);
    if (Parameter* p = Resolve(n, 0x2c, 1); p && Differs(p->value, f, 2)) {
        std::memcpy(p->value, f, 8);
        MarkChanged(p, false, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameter(const PString& n, const Vector3& v)
{
    const float* f = reinterpret_cast<const float*>(&v);
    if (Parameter* p = Resolve(n, 0x30, 2); p && Differs(p->value, f, 3)) {
        std::memcpy(p->value, f, 12);
        MarkChanged(p, false, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameter(const PString& n, const Vector4& v)
{
    const float* f = reinterpret_cast<const float*>(&v);
    if (Parameter* p = Resolve(n, 0x34, 3); p && Differs(p->value, f, 4)) {
        std::memcpy(p->value, f, 16);
        MarkChanged(p, false, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameter(const PString& n, const Color& v)
{
    const float* f = reinterpret_cast<const float*>(&v);
    if (Parameter* p = Resolve(n, 0x34, 4); p && Differs(p->value, f, 4)) {
        std::memcpy(p->value, f, 16);
        MarkChanged(p, false, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameter(const PString& n, const Matrix3& m, bool transpose)
{
    const float* f = reinterpret_cast<const float*>(&m);
    if (Parameter* p = Resolve(n, 0x48, 5); p && Differs(f, p->value, 9)) {
        std::memcpy(p->value, f, 36);
        MarkChanged(p, transpose, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameter(const PString& n, const Matrix4& m, bool transpose)
{
    const float* f = reinterpret_cast<const float*>(&m);
    if (Parameter* p = Resolve(n, 0x64, 6); p && Differs(p->value, f, 16)) {
        std::memcpy(p->value, f, 64);
        MarkChanged(p, transpose, 1);
    }
}
void __cdecl GLSLShaderBase::SetParameterArray(const PString& n, const float* v, int count)
{
    if (Parameter* p = Resolve(n, 0x28, 7)) {
        *reinterpret_cast<const float**>(p->value) = v;
        MarkChanged(p, false, count);
    }
}
void __cdecl GLSLShaderBase::SetParameterArray(const PString& n, const Color* v, int count)
{
    if (Parameter* p = Resolve(n, 0x28, 8)) {
        *reinterpret_cast<const Color**>(p->value) = v;
        MarkChanged(p, false, count);
    }
}
void __cdecl GLSLShaderBase::SetParameterArray(const PString& n, const Matrix4* m, int count, bool transpose)
{
    if (Parameter* p = Resolve(n, 0x28, 9)) {
        *reinterpret_cast<const Matrix4**>(p->value) = m;
        MarkChanged(p, transpose, count);
    }
}

}  // namespace Jet
