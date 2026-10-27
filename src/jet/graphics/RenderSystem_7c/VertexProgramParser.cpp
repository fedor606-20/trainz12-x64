// Unit U2b2 of RenderSystem_7c: VertexProgramParser, the front end VertexProgram::Load runs over the pre-parser's
// output (scanner vp 0x10051bd0, Lemon driver 0x100518c0 with reduce 0x1004e710, run 0x10054f00). Its Lemon is a
// later generation than the other three (int actions, 16-byte values: LemonMinor16). The reduce actions build the
// program's symbols (VertexSymbolEntry, added to a VertexSymbolTable), operands (RegisterItem) and instructions
// (VertexInstruction); the run hands them to the program through its virtuals. The DFA and parser tables are the
// original's (gfxvp_*.h, read from the installed module at build time).
#include "GfxParse.h"

#include <jet/GfxLanguage.h>
#include <jet/Geometry.h>
#include <jet/VCall.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

#include "gfxvp_accept.h"
#include "gfxvp_acclist.h"
#include "gfxvp_base.h"
#include "gfxvp_chk.h"
#include "gfxvp_def.h"
#include "gfxvp_ec.h"
#include "gfxvp_lemon.h"
#include "gfxvp_meta.h"
#include "gfxvp_nxt.h"

namespace Jet {

namespace {

const unsigned short accList[] = {GFXVP_ACCLIST_DATA};
const unsigned short accept[] = {GFXVP_ACCEPT_DATA};
const unsigned char ec[] = {GFXVP_EC_DATA};
const unsigned int meta[] = {GFXVP_META_DATA};
const unsigned short base[] = {GFXVP_BASE_DATA};
const unsigned short def[] = {GFXVP_DEF_DATA};
const unsigned short nxt[] = {GFXVP_NXT_DATA};
const unsigned short chk[] = {GFXVP_CHK_DATA};
const FlexTables tables = {accList, accept, ec, meta, base, def, nxt, chk, 0x261, 0x78, true};

// an array of pointers growing in steps of 128 (0x10051b70), as the program's instruction list
struct PtrArray {
    unsigned count, capacity;
    void** data;
    void Add(void* p)  // 0x1001cae0
    {
        if (count + 1 > capacity) {
            const unsigned items = (count + 1 + 127) / 128 * 128;
            void** const block = static_cast<void**>(Alloc::New(items * 4));
            if (data) {
                std::memcpy(block, data, count * 4);
                if (data) Alloc::Delete(data);
            }
            capacity = items;
            data = block;
        }
        data[count++] = p;
    }
};

// a list of {?, owned block, next} (0x10032160); nothing in the parser fills it
struct BlockItem {
    int unused;
    void* block;
    BlockItem* next;
};

}  // namespace

class VertexProgramParser;

struct VpContext {  // the run's state (0x10054f00 locals, 0x48 bytes)
    float version;                     // +0
    ExtendedGfxLanguage* program;      // +4 the VertexProgram
    unsigned long symbolsRaw[3];       // +8 VertexSymbolTable (zeroed = empty)
    PtrArray instructions;             // +0x14
    const char* fileName;              // +0x20
    const char* programName;           // +0x24
    VertexProgramParser* parser;       // +0x28
    GfxNameList names;                 // +0x2c
    PStringCache::Node* vertexType;    // +0x30 PString
    BlockItem* blocksHead;             // +0x34
    BlockItem* blocksTail;
    int blocksCount;
    char* native;                      // +0x40 the native text (Jet::Alloc)
    bool error;                        // +0x44

    VertexSymbolTable& Symbols() { return *reinterpret_cast<VertexSymbolTable*>(symbolsRaw); }
    PString& VertexTypeName() { return *reinterpret_cast<PString*>(&vertexType); }
    void Destroy();  // 0x10032050
};
JET_LAYOUT32(sizeof(VpContext) == 0x48);

void VpContext::Destroy()
{
    if (native) Alloc::Delete(native);
    native = nullptr;
    for (unsigned i = 0; i < instructions.count; ++i) {
        if (instructions.data[i]) VDelete(instructions.data[i]);
        instructions.data[i] = nullptr;
    }
    while (BlockItem* const b = blocksHead) {
        blocksHead = b->next;
        if (!blocksHead) blocksTail = nullptr;
        --blocksCount;
        if (b->block) Alloc::Delete(b->block);
        Alloc::Delete(b);
    }
    if (vertexType && PString::Cache()) PString::Cache()->Destroy(vertexType);
    names.Clear();
    instructions.count = 0;
    if (instructions.data) Alloc::Delete(instructions.data);
    Symbols().~VertexSymbolTable();
}

struct VpTraits {
    using Minor = LemonMinor16;
    static constexpr int kNoCode = 0xf1;
    static constexpr int kStates = 0x1e0;
    static constexpr int kRules = 0x10c;
    static constexpr int kErrorSymbol = 0x76;
    static constexpr int kTerminals = 0x64;
    static const LemonAction pool[];
    static const LemonState states[];
    static const LemonRule rules[];
    static void Reduce(LemonStackT<Minor>* p, int rule, VpContext* ctx, Minor& gotoMinor);
    static void SyntaxError(VpContext* ctx) { ctx->error = true; }  // "Syntax error at %s." (0x100517d0)
    static void Failed(VpContext* ctx) { ctx->error = true; }       // "Giving up." (0x10051770)
};
const LemonAction VpTraits::pool[] = {GFXVP_POOL_DATA};
const LemonState VpTraits::states[] = {GFXVP_STATES_DATA};
const LemonRule VpTraits::rules[] = {GFXVP_RULES_DATA};
using VpLemon = LemonParser<VpTraits, VpContext>;

class VertexProgramParser : public FlexScanner {
public:
    explicit VertexProgramParser(MemoryFile* in) : FlexScanner(tables, in), file(in) {}

    // 0x10054f00: false on a syntax error or when the program refuses what it is handed
    bool Run(const PString& program, const PString& fileName, ExtendedGfxLanguage* language);
    int Lex();  // 0x10051bd0

    MemoryFile* file;
    void* value = nullptr;           // +0x78 the token's text (Jet::Alloc)
    PString fileName;                // +0x7c
    PString programName;             // +0x80
    int line = 1;                    // +0x84
    ExtendedGfxLanguage* program = nullptr;  // +0x88

private:
    void Push(int condition)
    {
        gfxLex.vpStack[gfxLex.vpDepth++] = gfxLex.vpCurrent;
        gfxLex.vpCurrent = condition;
        start = 2 * condition + 1;
    }
    int Pop(int token)
    {
        gfxLex.vpCurrent = gfxLex.vpStack[--gfxLex.vpDepth];
        start = 2 * gfxLex.vpCurrent + 1;
        return token;
    }
    int Text(int token)
    {
        value = CopyText(yytext);
        return token;
    }
    void LineMarker()  // "#LINE n"
    {
        char word[256];
        int n = 0;
        std::sscanf(yytext, "%s %d", word, &n);
        line = n;
    }
    void Copy()  // the text into the code block, the write pointer left on its terminator
    {
        const char* s = yytext;
        while ((*gfxLex.vpWrite++ = *s++) != 0) {
        }
        --gfxLex.vpWrite;
    }
    void SecondInclude()  // the second #include-like marker of a block opens condition 4
    {
        if (gfxLex.vpFlag && ++gfxLex.vpCount == 2) Push(4);
    }
};

int VertexProgramParser::Lex()
{
    static const unsigned char kPopTokens[0x44 - 0x10] = {
        0x32, 0x45, 0x63, 0x34, 0x10, 0x20, 0x5a, 0x2f, 0x08, 0x2d, 0x03, 0x13, 0x53, 0x17, 0x51, 0x28, 0x06,
        0x54, 0x22, 0x15, 0x2a, 0x4e, 0x2b, 0x5b, 0x1d, 0x35, 0x1f, 0x3f, 0x0e, 0x40, 0x42, 0x52, 0x4c, 0x2e,
        0x31, 0x48, 0x5d, 0x3b, 0x47, 0x24, 0x1b, 0x2c, 0x44, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static const unsigned char kTokens[0x69 - 0x47] = {
        0x38, 0x0f, 0x37, 0x0d, 0x41, 0x30, 0x1c, 0x39, 0x07, 0x3d, 0x59, 0x01, 0x3a, 0x02, 0x61, 0x47, 0x57,
        0x4b, 0x49, 0x26, 0x5e, 0x27, 0x21, 0x09, 0x46, 0x4f, 0x29, 0x3e, 0x14, 0x4a, 0x04, 0x3c, 0x43, 0x55};
    for (;;) {
        const int act = Scan();
        if (act >= 0x10 && act <= 0x3a) return Pop(kPopTokens[act - 0x10]);
        if (act >= 0x47 && act <= 0x68) return kTokens[act - 0x47];
        switch (act) {
        case 1: return 0x62;
        case 2: case 0x73: return Text(0x23);
        case 3:
            gfxLex.vpBuffer = gfxLex.vpWrite = static_cast<char*>(Alloc::New(static_cast<unsigned>(file->size)));
            Push(2);
            LineMarker();
            return 0x33;
        case 4: Push(1); LineMarker(); return 0xa;
        case 5: Push(1); LineMarker(); return 0x56;
        case 6:
            value = gfxLex.vpBuffer;
            return Pop(0x1a);
        case 7: Copy(); ++line; break;
        case 8: Copy(); break;
        case 9: case 0x3c: return Text(0x12);
        case 0xa: return 0x64;
        case 0xb: return 0x36;
        case 0xc: return 0x41;
        case 0xd: return 0x30;
        case 0xe: return 0x11;
        case 0xf: case 0x3e:
            Pop(0);
            gfxLex.vpFlag = 0;
            return 0x50;
        case 0x3b: return Text(0x5c);
        case 0x3d:  // pops through the entry below the top
            gfxLex.vpCurrent = (&gfxLex.vpStack[0])[gfxLex.vpDepth - 1];
            --gfxLex.vpDepth;
            start = 2 * gfxLex.vpCurrent + 1;
            SecondInclude();
            return 0x11;
        case 0x3f: Pop(0); gfxLex.vpFlag = 0; return 0xf;
        case 0x40: Pop(0); gfxLex.vpFlag = 0; return 0x41;
        case 0x41: Pop(0); gfxLex.vpFlag = 0; return 0x30;
        case 0x42: return Pop(0x18);
        case 0x43: return Pop(0x19);
        case 0x44: gfxLex.vpFlag = 0; return 0x50;
        case 0x45: SecondInclude(); return 0x11;
        case 0x46: Push(3); return 0x16;
        case 0x69: return Text(0x25);
        case 0x6a: return Text(0x1e);
        case 0x6b: {  // the program's register count less one, as text
            char* const s = static_cast<char*>(Alloc::New(10));
            std::sprintf(s, "%d", VCall<int>(program, 0x78) - 1);
            value = s;
            return 0x25;
        }
        case 0x6c: return 5;
        case 0x6d: return Text(0x60);
        case 0x6e: return Text(0x4d);
        case 0x6f: return Text(0xc);
        case 0x70: return Text(0xb);
        case 0x71: return Text(0x5f);
        case 0x72:
            gfxLex.vpFlag = 1;
            gfxLex.vpCount = 0;
            return 0x58;
        case 0x74: LineMarker(); break;
        case 0x75: ++line; break;
        case 0x76: break;
        case 0x77: Output(yytext, yyleng); break;
        case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: return 0;
        default: Error("fatal flex scanner internal error--no action found");
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the reduce actions

namespace {

struct NameValue {
    const char* name;
    int value;
};
// 0x100b8820.. (static PString tables filled at module start; looked up ignoring case, 0 when unknown)
const NameValue kUnary[] = {{"ABS", 1}, {"FLR", 10}, {"FRC", 11}, {"LIT", 13}, {"MOV", 19}, {"NRM", 21},
                            {"SGN", 26}, {nullptr, 0}};
const NameValue kScalar[] = {{"EX2", 8}, {"EXP", 9}, {"LG2", 12}, {"LOG", 14}, {"RCP", 23}, {"RSQ", 24},
                             {nullptr, 0}};
const NameValue kPower[] = {{"POW", 22}, {nullptr, 0}};
const NameValue kBinary[] = {{"ADD", 2},  {"DP3", 4},  {"DP4", 5},  {"DPH", 6},   {"DST", 7},   {"MAX", 17},
                             {"MIN", 18}, {"MUL", 20}, {"SGE", 25}, {"SLT", 27},  {"SUB", 28},  {"XPD", 30},
                             {"CRS", 30}, {"m3x2", 31}, {"m3x3", 32}, {"m3x4", 33}, {"m4x3", 34}, {"m4x4", 35},
                             {nullptr, 0}};
const NameValue kTrinary[] = {{"LRP", 15}, {"MAD", 16}, {nullptr, 0}};

int Opcode(const NameValue* table, const char* text)  // 0x10055260..0x10055660
{
    PString key;
    if (text) key.node = PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1);
    for (; table->value; ++table) {
        const PString name(table->name);
        if (key.EqualsIgnoreCase(name)) return table->value;
    }
    return 0;
}

using Minor = LemonMinor16;
using Stack = LemonStackT<Minor>;

Minor& M(Stack* p, int k) { return (p->top - k)->minor; }
uint32_t& W(Minor& m, int j) { return reinterpret_cast<uint32_t*>(&m)[j]; }
float& F(Minor& m, int j) { return reinterpret_cast<float*>(&m)[j]; }
template <class T>
T* P(Stack* p, int k)
{
    return static_cast<T*>(M(p, k).p);
}
const char* Text(Stack* p, int k) { return static_cast<const char*>(M(p, k).p); }
unsigned char Byte(Stack* p, int k) { return static_cast<unsigned char>(W(M(p, k), 0)); }
void Drop(Stack* p, int k)
{
    Minor& m = M(p, k);
    if (m.p) Alloc::Delete(m.p);
    m.p = nullptr;
}
void SetName(PString& s, const char* text)  // 0x1001d1e0
{
    if (s.node) PString::Cache()->Destroy(s.node);
    s.node = text ? PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1) : nullptr;
}
void FreeSymbol(SymbolEntry* s) { delete s; }  // 0x10021250

VertexSymbolEntry* NewSymbol(int cls, int type, bool referenced)  // 0x1000f390 (one body for both classes)
{
    void* const mem = Alloc::New(sizeof(VertexSymbolEntry));
    return mem ? ::new (mem) VertexSymbolEntry(static_cast<VertexSymbolClass>(cls), static_cast<SymbolType>(type),
                                               referenced)
               : nullptr;
}
RegisterList* NewList()
{
    void* const mem = Alloc::New(sizeof(RegisterList));
    return mem ? ::new (mem) RegisterList() : nullptr;
}
void AddOperand(RegisterList* list, Stack* p, int k) { reinterpret_cast<PtrArray*>(list)->Add(M(p, k).p); }
RegisterItem* NewOperand(SymbolEntry* s)
{
    void* const mem = Alloc::New(sizeof(RegisterItem));
    if (!mem) return nullptr;
    auto* const r = static_cast<RegisterItem*>(mem);
    r->symbol = s;
    r->offset = nullptr;
    r->swizzle = nullptr;
    r->flag = false;
    r->m1 = nullptr;
    r->m2 = nullptr;
    return r;
}
OffsetAddress* NewOffset(RegisterItem* item, int offset)
{
    auto* const o = static_cast<OffsetAddress*>(Alloc::New(sizeof(OffsetAddress)));
    if (o) {
        o->item = item;
        o->offset = offset;
    }
    return o;
}
// an instruction of `list`: the opcode found after the instruction is allocated
template <class Op>
VertexInstruction* NewInstruction(RegisterList* list, Op op)
{
    void* const mem = Alloc::New(sizeof(VertexInstruction));
    return mem ? ::new (mem) VertexInstruction(static_cast<VertexOpcode>(op()), list) : nullptr;
}
// a symbol already in the table stands for an equal new one (which is freed)
SymbolEntry* Shared(VpContext* ctx, Stack* p, int k)
{
    auto* const s = P<VertexSymbolEntry>(p, k);
    if (VertexSymbolEntry* const like = ctx->Symbols().FindLike(s)) {
        like->AddRef();
        if (s) FreeSymbol(s);
        M(p, k).p = like;
        return like;
    }
    s->AddRef();
    ctx->Symbols().AddSymbol(s);
    return s;
}
void SetFlag1(SymbolEntry* s, unsigned char b) { s->flags ^= (s->flags ^ static_cast<unsigned char>(b + b)) & 2; }
// "x/y/z/w" texts: their first characters
char* Chars(Stack* p, std::initializer_list<int> ks)
{
    char* const s = static_cast<char*>(Alloc::New(static_cast<unsigned>(ks.size() + 1)));
    char* d = s;
    for (int k : ks) *d++ = *Text(p, k);
    *d = 0;
    for (int k : ks) Drop(p, k);
    return s;
}
// the 16-byte value as {x, y, z, w}
void Vec(Minor& g, float x, float y, float z, float w)
{
    F(g, 0) = x;
    F(g, 1) = y;
    F(g, 2) = z;
    F(g, 3) = w;
}
void SetVector(SymbolEntry* s, const Minor& v)  // the symbol's value (+0x24), as Vec4::GetVec4 copies it
{
    std::memcpy(&s->field24, &v, 16);
}

}  // namespace

void VpTraits::Reduce(Stack* p, int rule, VpContext* ctx, Minor& g)
{
    uint32_t& g0 = W(g, 0);
    switch (rule) {
    case 8: case 17: case 25: Drop(p, 2); Drop(p, 0); break;
    case 9: case 13: Drop(p, 1); break;
    case 12:
        SetName(ctx->VertexTypeName(), Text(p, 0));
        Drop(p, 0);
        Drop(p, 1);
        break;
    case 14: {  // a declared symbol, its name, and what it stands for
        if (auto* const s = P<VertexSymbolEntry>(p, 1)) {
            if (M(p, 2).p) s->SetName(Text(p, 2));
            ctx->Symbols().AddSymbol(s);
            if (auto* const o = P<SymbolEntry>(p, 0)) {
                const int t = static_cast<int>(o->type);
                if (t >= 0xc && t <= 0x2b) {
                    s->alias = o;
                    s->flags |= 4;
                } else if (t >= 1 && t <= 2) {
                    s->field24 = o->field24;
                    s->field28 = o->field28;
                    s->field2c = o->field2c;
                    s->field30 = o->field30;
                    FreeSymbol(o);
                    M(p, 0).p = nullptr;
                }  // else "Undefined parameter yymsp[0].minor.yy225." (formatted, never shown)
            }
        }
        Drop(p, 2);
        break;
    }
    case 15: case 21: case 73: case 80: case 83: case 90: case 91: case 106: case 113: case 117: case 193:
    case 205: case 260:
        g0 = 0;
        break;
    case 16: g0 = W(M(p, 2), 0); Drop(p, 0); break;
    case 19: case 36: case 37: case 38: case 39: case 40: case 41: case 42: case 61: case 66: case 68: case 69:
    case 75: case 81: case 82: case 88: case 89: case 92: case 93: case 104: case 107: case 108: case 109: case 112:
    case 119: case 123: case 124: case 125: case 126: case 130: case 131: case 132: case 133: case 174: case 183:
    case 197: case 206: case 207: case 208: case 209: case 210: case 211: case 212: case 221: case 222: case 261:
    case 263: case 265:
        g0 = W(M(p, 0), 0);
        break;
    case 20: g0 = W(M(p, 1), 0); Drop(p, 2); Drop(p, 0); break;
    case 22: case 23: case 71: case 76: case 77: case 78: case 84: case 114: case 116:
        g0 = W(M(p, 0), 0);
        Drop(p, 1);
        break;
    case 26:
        ctx->native = static_cast<char*>(M(p, 0).p);
        Drop(p, 1);
        break;
    case 30: Drop(p, 0); break;
    case 31: {  // a name of the program
        PString name;
        SetName(name, Text(p, 0));
        ctx->names.Append(name);
        Drop(p, 0);
        name.~PString();
        name.node = nullptr;
        Drop(p, 1);
        break;
    }
    case 34:
        ctx->instructions.Add(M(p, 1).p);
        Drop(p, 0);
        break;
    case 35:
        if (M(p, 1).p) ctx->Symbols().AddSymbol(P<VertexSymbolEntry>(p, 1));
        Drop(p, 0);
        break;
    case 43: {  // ARL
        RegisterList* const list = NewList();
        AddOperand(list, p, 2);
        AddOperand(list, p, 0);
        g0 = reinterpret_cast<uint32_t>(NewInstruction(list, [] { return 3; }));
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 44: case 45: {  // a unary or scalar instruction
        RegisterList* const list = NewList();
        AddOperand(list, p, 2);
        AddOperand(list, p, 0);
        const char* const name = Text(p, 3);
        g0 = reinterpret_cast<uint32_t>(
            NewInstruction(list, [&] { return Opcode(rule == 44 ? kUnary : kScalar, name); }));
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 46: case 47: {  // POW, a binary instruction
        RegisterList* const list = NewList();
        AddOperand(list, p, 4);
        AddOperand(list, p, 2);
        AddOperand(list, p, 0);
        const char* const name = Text(p, 5);
        g0 = reinterpret_cast<uint32_t>(
            NewInstruction(list, [&] { return Opcode(rule == 46 ? kPower : kBinary, name); }));
        Drop(p, 5);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 48: {  // a trinary instruction
        RegisterList* const list = NewList();
        AddOperand(list, p, 6);
        AddOperand(list, p, 4);
        AddOperand(list, p, 2);
        AddOperand(list, p, 0);
        const char* const name = Text(p, 7);
        g0 = reinterpret_cast<uint32_t>(NewInstruction(list, [&] { return Opcode(kTrinary, name); }));
        Drop(p, 7);
        Drop(p, 5);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 49: {  // SWZ: the swizzle text goes to the source operand
        RegisterList* const list = NewList();
        AddOperand(list, p, 4);
        AddOperand(list, p, 2);
        P<RegisterItem>(p, 2)->swizzle = static_cast<char*>(M(p, 0).p);
        g0 = reinterpret_cast<uint32_t>(NewInstruction(list, [] { return 0x1d; }));
        Drop(p, 5);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 50: case 51: {
        auto* const r = P<RegisterItem>(p, 1);
        reinterpret_cast<unsigned char&>(r->flag) = Byte(p, 2);  // (the byte as it is)
        r->swizzle = static_cast<char*>(M(p, 0).p);
        g0 = W(M(p, 1), 0);
        break;
    }
    case 52: case 53:
        P<RegisterItem>(p, 1)->swizzle = static_cast<char*>(M(p, 0).p);
        g0 = W(M(p, 1), 0);
        break;
    case 54: {  // a swizzle "a,b,c,d" of four components (two characters each)
        char* const s = static_cast<char*>(Alloc::New(0x14));
        const int ks[4] = {6, 4, 2, 0};
        char* d = s;
        for (int i = 0; i < 4; ++i) {
            if (i) *d++ = ',';
            *d++ = static_cast<char>(W(M(p, ks[i]), 0));
            *d++ = static_cast<char>(W(M(p, ks[i]), 0) >> 8);
        }
        *d = 0;
        g0 = reinterpret_cast<uint32_t>(s);
        Drop(p, 5);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 55:
        reinterpret_cast<unsigned char*>(&g)[0] = Byte(p, 1) ? '-' : ' ';
        reinterpret_cast<unsigned char*>(&g)[1] = Byte(p, 0);
        break;
    case 56: reinterpret_cast<unsigned char*>(&g)[0] = '0'; Drop(p, 0); break;
    case 57: reinterpret_cast<unsigned char*>(&g)[0] = '1'; Drop(p, 0); break;
    case 58: reinterpret_cast<unsigned char*>(&g)[0] = *Text(p, 0); Drop(p, 0); break;
    case 59: case 62: case 65: case 74:
        g0 = reinterpret_cast<uint32_t>(NewOperand(P<SymbolEntry>(p, 0)));
        break;
    case 60: case 63: {
        SymbolEntry* const s = Shared(ctx, p, 0);
        g0 = reinterpret_cast<uint32_t>(NewOperand(s));
        break;
    }
    case 64: {  // an indexed operand
        RegisterItem* const r = NewOperand(P<SymbolEntry>(p, 3));
        g0 = reinterpret_cast<uint32_t>(r);
        r->offset = P<OffsetAddress>(p, 1);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 67: g0 = reinterpret_cast<uint32_t>(NewOffset(nullptr, static_cast<int>(W(M(p, 0), 0)))); break;
    case 70:
        P<RegisterItem>(p, 2)->swizzle = static_cast<char*>(M(p, 1).p);
        g0 = reinterpret_cast<uint32_t>(NewOffset(P<RegisterItem>(p, 2), static_cast<int>(W(M(p, 0), 0))));
        break;
    case 72: g0 = 0u - W(M(p, 0), 0); Drop(p, 1); break;
    case 79: case 87: g0 = reinterpret_cast<uint32_t>(Chars(p, {3, 2, 1, 0})); Drop(p, 4); break;
    case 85: g0 = reinterpret_cast<uint32_t>(Chars(p, {1, 0})); Drop(p, 2); break;
    case 86: g0 = reinterpret_cast<uint32_t>(Chars(p, {2, 1, 0})); Drop(p, 3); break;
    case 94:
        g0 = W(M(p, 0), 0);
        P<SymbolEntry>(p, 0)->SetName(Text(p, 2));
        Drop(p, 2);
        Drop(p, 3);
        Drop(p, 1);
        break;
    case 95: case 134: case 135: case 136: case 137: case 138: case 139: case 140: case 141: case 142: case 143:
    case 157:
        g0 = W(M(p, 0), 0);
        Drop(p, 2);
        Drop(p, 1);
        break;
    case 96: case 98: case 100: case 199: case 200: {  // a symbol of a fixed type
        static const int types[][2] = {{96, 3}, {98, 5}, {100, 8}, {199, 0xe}, {200, 0xc}};
        int type = 0;
        for (const auto& t : types)
            if (t[0] == rule) type = t[1];
        g0 = reinterpret_cast<uint32_t>(NewSymbol(rule >= 199 ? 4 : 6, type, false));
        Drop(p, 0);
        break;
    }
    case 97: case 101: case 102: case 198: case 201: {  // ... with a value
        static const int types[][3] = {{97, 6, 4}, {101, 6, 9}, {102, 6, 0xa}, {198, 4, 0xd}, {201, 4, 0xf}};
        int cls = 0, type = 0;
        for (const auto& t : types)
            if (t[0] == rule) cls = t[1], type = t[2];
        SymbolEntry* const s = NewSymbol(cls, type, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->field0c = static_cast<int>(W(M(p, 0), 0));
        Drop(p, 1);
        break;
    }
    case 99:
        g0 = reinterpret_cast<uint32_t>(NewSymbol(6, Byte(p, 0) ? 6 : 7, false));
        Drop(p, 1);
        break;
    case 103: {
        SymbolEntry* const s = NewSymbol(6, 0xb, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->field0c = static_cast<int>(W(M(p, 1), 0));
        Drop(p, 3);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 105: case 204: case 259: g0 = W(M(p, 1), 0); Drop(p, 2); Drop(p, 0); break;
    case 110: {
        auto* const s = P<SymbolEntry>(p, 1);
        g0 = W(M(p, 1), 0);
        s->SetName(Text(p, 2));
        s->hint = W(M(p, 0), 0);
        Drop(p, 2);
        Drop(p, 3);
        break;
    }
    case 111: {
        auto* const s = P<SymbolEntry>(p, 1);
        g0 = W(M(p, 1), 0);
        s->SetName(Text(p, 5));
        s->field08 = static_cast<int>(W(M(p, 3), 0));
        s->hint = W(M(p, 0), 0);
        Drop(p, 5);
        Drop(p, 6);
        Drop(p, 4);
        Drop(p, 2);
        break;
    }
    case 115: g0 = W(M(p, 1), 0); Drop(p, 3); Drop(p, 2); Drop(p, 0); break;
    case 118: g0 = W(M(p, 2), 0) | W(M(p, 0), 0); Drop(p, 1); break;
    case 120: case 162: case 194: g0 = 1; Drop(p, 0); break;
    case 121: case 163: case 195: g0 = 2; Drop(p, 0); break;
    case 122:
        g0 = W(M(p, 2), 0);
        P<SymbolEntry>(p, 2)->AppendSymbol(P<SymbolEntry>(p, 0));
        Drop(p, 1);
        break;
    case 127: case 128: g0 = reinterpret_cast<uint32_t>(Shared(ctx, p, 0)); break;
    case 129:
        g0 = W(M(p, 0), 0);
        ctx->Symbols().AddSymbol(P<VertexSymbolEntry>(p, 0));
        break;
    case 144: {
        SymbolEntry* const s = NewSymbol(4, static_cast<int>(W(M(p, 0), 0)), false);
        SetFlag1(s, Byte(p, 2));
        g0 = reinterpret_cast<uint32_t>(s);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 145: case 146: case 147: case 148: case 149: g0 = 0x12 + (rule - 145); Drop(p, 0); break;
    case 150: case 151: {
        SymbolEntry* const s = NewSymbol(4, static_cast<int>(W(M(p, rule == 150 ? 3 : 0), 0)), false);
        g0 = reinterpret_cast<uint32_t>(s);
        if (rule == 150) {
            s->field0c = static_cast<int>(W(M(p, 0), 0));
            s->field1c = static_cast<int>(W(M(p, 1), 0));
            Drop(p, 8);
            Drop(p, 7);
            Drop(p, 5);
            Drop(p, 4);
            Drop(p, 2);
        } else {
            s->field0c = static_cast<int>(W(M(p, 3), 0));
            s->field1c = 5;
            Drop(p, 5);
            Drop(p, 4);
            Drop(p, 2);
            Drop(p, 1);
        }
        break;
    }
    case 152: case 153: case 154: case 155: case 156: g0 = 0x17 + (rule - 152); Drop(p, 0); break;
    case 158: g0 = 0x1d; Drop(p, 0); break;
    case 159: g0 = 0x1c; Drop(p, 0); break;
    case 160: g0 = 0; Drop(p, 0); break;
    case 161: case 196: g0 = 3; Drop(p, 0); break;
    case 164: case 179: case 184: {
        g0 = reinterpret_cast<uint32_t>(NewSymbol(4, static_cast<int>(W(M(p, 0), 0)), false));
        if (rule != 164) Drop(p, 2);
        Drop(p, 1);
        break;
    }
    case 165: g0 = 0x1e; Drop(p, 1); Drop(p, 0); break;
    case 166: g0 = 0x1f; Drop(p, 2); Drop(p, 1); Drop(p, 0); break;
    case 167: g0 = 0x20; Drop(p, 2); Drop(p, 1); Drop(p, 0); break;
    case 168: g0 = 0x1f; Drop(p, 1); Drop(p, 0); break;
    case 169: {
        SymbolEntry* const s = NewSymbol(4, static_cast<int>(W(M(p, 3), 0)), false);
        s->field0c = static_cast<int>(W(M(p, 0), 0));
        s->field1c = static_cast<int>(W(M(p, 1), 0));
        SetFlag1(s, Byte(p, 5));
        g0 = reinterpret_cast<uint32_t>(s);
        Drop(p, 9);
        Drop(p, 8);
        Drop(p, 6);
        Drop(p, 4);
        Drop(p, 2);
        break;
    }
    case 170: {
        SymbolEntry* const s = NewSymbol(4, static_cast<int>(W(M(p, 0), 0)), false);
        s->field0c = static_cast<int>(W(M(p, 4), 0));
        SetFlag1(s, Byte(p, 2));
        g0 = reinterpret_cast<uint32_t>(s);
        s->field1c = 5;
        Drop(p, 6);
        Drop(p, 5);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 171: case 172: case 173: g0 = 0x21 + (rule - 171); Drop(p, 0); break;
    case 175: {
        SymbolEntry* const s = NewSymbol(4, static_cast<int>(W(M(p, 2), 0)), false);
        s->field0c = static_cast<int>(W(M(p, 4), 0));
        g0 = reinterpret_cast<uint32_t>(s);
        s->field20 = Byte(p, 0);
        Drop(p, 5);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 176: g0 = 0x24; Drop(p, 0); break;
    case 177: g0 = 0x25; Drop(p, 0); break;
    case 178:  // a texture coordinate letter
        switch (*Text(p, 0)) {
        case 'q': g0 = 3; break;
        case 'r': g0 = 2; break;
        case 's': g0 = 0; break;
        case 't': g0 = 1; break;
        default: break;
        }
        Drop(p, 0);
        break;
    case 180: g0 = 0x26; Drop(p, 0); break;
    case 181: g0 = 0x27; Drop(p, 0); break;
    case 182: {
        SymbolEntry* const s = NewSymbol(4, 0x28, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->field0c = static_cast<int>(W(M(p, 3), 0));
        Drop(p, 5);
        Drop(p, 4);
        Drop(p, 2);
        Drop(p, 1);
        Drop(p, 0);
        break;
    }
    case 185: g0 = 0x29; Drop(p, 0); break;
    case 186: g0 = 0x2a; Drop(p, 0); break;
    case 187: {
        auto* const s = P<SymbolEntry>(p, 5);
        s->field14 = static_cast<int>(W(M(p, 1), 0));
        g0 = W(M(p, 5), 0);
        s->field10 = static_cast<int>(W(M(p, 1), 0));
        Drop(p, 4);
        Drop(p, 3);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 188: {
        auto* const s = P<SymbolEntry>(p, 1);
        s->field10 = static_cast<int>(W(M(p, 0), 0));
        g0 = W(M(p, 1), 0);
        s->field14 = static_cast<int>(W(M(p, 0), 1));
        break;
    }
    case 189: {
        SymbolEntry* const s = NewSymbol(0xa, 0x2b, false);
        s->field10 = static_cast<int>(W(M(p, 3), 0));
        g0 = reinterpret_cast<uint32_t>(s);
        s->field14 = static_cast<int>(W(M(p, 1), 0));
        Drop(p, 7);
        Drop(p, 6);
        Drop(p, 5);
        Drop(p, 4);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 190:
        W(g, 1) = W(M(p, 1), 0);
        g0 = W(M(p, 3), 0);
        Drop(p, 6);
        Drop(p, 5);
        Drop(p, 4);
        Drop(p, 2);
        Drop(p, 0);
        break;
    case 191: g0 = 0; W(g, 1) = 3; break;
    case 192: {
        auto* const s = P<SymbolEntry>(p, 1);
        s->field34 = static_cast<int>(W(M(p, 0), 0));
        g0 = W(M(p, 1), 0);
        Drop(p, 3);
        Drop(p, 2);
        break;
    }
    case 202: case 203: {
        SymbolEntry* const s = NewSymbol(4, rule == 202 ? 0x10 : 0x11, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->field0c = static_cast<int>(W(M(p, 1), 0));
        Drop(p, 3);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 213: case 216: case 217: {
        SymbolEntry* const s = NewSymbol(rule == 217 ? 1 : 2, 2, false);
        if (rule == 216) {
            s->field14 = static_cast<int>(W(M(p, 1), 0));
            g0 = reinterpret_cast<uint32_t>(s);
            s->field10 = static_cast<int>(W(M(p, 1), 0));
        } else {
            s->field10 = static_cast<int>(W(M(p, 1), 0));
            g0 = reinterpret_cast<uint32_t>(s);
            s->field14 = static_cast<int>(W(M(p, 1), 1));
        }
        Drop(p, 5);
        Drop(p, 4);
        Drop(p, 3);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 214: case 218: g0 = W(M(p, 0), 0); W(g, 1) = W(M(p, 0), 0); break;
    case 215: case 219: g0 = W(M(p, 2), 0); W(g, 1) = W(M(p, 0), 0); Drop(p, 1); break;
    case 220: {
        SymbolEntry* const s = NewSymbol(1, 2, false);
        s->field14 = static_cast<int>(W(M(p, 2), 0));
        g0 = reinterpret_cast<uint32_t>(s);
        s->field10 = static_cast<int>(W(M(p, 2), 0));
        SetVector(s, M(p, 0));
        Drop(p, 6);
        Drop(p, 5);
        Drop(p, 4);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 224: {
        const float v = F(M(p, 0), 0);
        Vec(g, v, v, v, v);
        Drop(p, 1);
        break;
    }
    case 225: g = M(p, 0); Drop(p, 1); break;
    case 226: case 228: {  // a constant of one value in all four
        SymbolEntry* const s = NewSymbol(3, 1, rule == 228);
        const float v = F(M(p, 0), 0);
        g0 = reinterpret_cast<uint32_t>(s);
        Minor t;
        Vec(t, v, v, v, v);
        SetVector(s, t);
        break;
    }
    case 227: case 229: {
        SymbolEntry* const s = NewSymbol(3, 2, rule == 229);
        g0 = reinterpret_cast<uint32_t>(s);
        SetVector(s, M(p, 0));
        break;
    }
    case 230: case 231: case 237: F(g, 0) = F(M(p, 0), 0); break;
    case 232: Vec(g, F(M(p, 1), 0), 0, 0, 1); Drop(p, 2); Drop(p, 0); break;
    case 233: Vec(g, F(M(p, 3), 0), F(M(p, 1), 0), 0, 1); Drop(p, 4); Drop(p, 2); Drop(p, 0); break;
    case 234:
        Vec(g, F(M(p, 5), 0), F(M(p, 3), 0), F(M(p, 1), 0), 1);
        Drop(p, 6);
        Drop(p, 4);
        Drop(p, 2);
        Drop(p, 0);
        break;
    case 235:
        Vec(g, F(M(p, 7), 0), F(M(p, 5), 0), F(M(p, 3), 0), F(M(p, 1), 0));
        Drop(p, 8);
        Drop(p, 6);
        Drop(p, 4);
        Drop(p, 2);
        Drop(p, 0);
        break;
    case 236: F(g, 0) = W(M(p, 1), 0) ? -0.0f - F(M(p, 0), 0) : F(M(p, 0), 0); break;
    case 238: F(g, 0) = static_cast<float>(static_cast<int>(W(M(p, 0), 0))); break;
    case 239: case 253: case 256: reinterpret_cast<unsigned char*>(&g)[0] = 1; Drop(p, 0); break;
    case 240: case 254: case 257: reinterpret_cast<unsigned char*>(&g)[0] = 0; Drop(p, 0); break;
    case 241: reinterpret_cast<unsigned char*>(&g)[0] = 0; break;
    case 255: case 258: reinterpret_cast<unsigned char*>(&g)[0] = 1; break;
    case 242: case 243: {  // every symbol of the list gets a class and goes into the table
        auto* const list = P<PtrArray>(p, 0);
        g0 = 0;
        W(g, 1) = 0;
        if (list->count)
            for (unsigned i = 0; i < list->count; ++i) {
                static_cast<VertexSymbolEntry*>(list->data[i])->symbolClass =
                    static_cast<VertexSymbolClass>(rule == 242 ? 9 : 5);
                ctx->Symbols().AddSymbol(static_cast<VertexSymbolEntry*>(list->data[i]));
            }
        if (list) {  // 0x10051740
            list->count = 0;
            if (list->data) Alloc::Delete(list->data);
            Alloc::Delete(list);
        }
        M(p, 0).p = nullptr;
        Drop(p, 1);
        break;
    }
    case 244: case 245: {  // a named symbol added to a list (a new one for 245)
        PtrArray* list;
        int name;
        if (rule == 244) {
            list = P<PtrArray>(p, 0);
            name = 2;
        } else {
            list = static_cast<PtrArray*>(Alloc::New(sizeof(PtrArray)));
            if (list) *list = PtrArray{0, 0, nullptr};
            name = 0;
        }
        g0 = reinterpret_cast<uint32_t>(list);
        SymbolEntry* const s = NewSymbol(0, 0, false);
        s->SetName(Text(p, name));
        list->Add(s);
        Drop(p, name);
        if (rule == 244) Drop(p, 1);
        break;
    }
    case 246:
        g0 = W(M(p, 0), 0);
        P<SymbolEntry>(p, 0)->SetName(Text(p, 2));
        Drop(p, 2);
        Drop(p, 3);
        Drop(p, 1);
        break;
    case 247: case 249: case 250: {
        g0 = reinterpret_cast<uint32_t>(NewSymbol(7, rule == 247 ? 0x2c : rule == 249 ? 0x2f : 0x30, false));
        Drop(p, 2);
        Drop(p, 1);
        Drop(p, 0);
        break;
    }
    case 248: {
        SymbolEntry* const s = NewSymbol(7, Byte(p, 0) ? 0x2d : 0x2e, false);
        SetFlag1(s, Byte(p, 1));
        g0 = reinterpret_cast<uint32_t>(s);
        Drop(p, 4);
        Drop(p, 3);
        Drop(p, 2);
        break;
    }
    case 251: {
        SymbolEntry* const s = NewSymbol(7, 0x32, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->field0c = static_cast<int>(W(M(p, 0), 0));
        Drop(p, 3);
        Drop(p, 2);
        Drop(p, 1);
        break;
    }
    case 252: {
        SymbolEntry* const s = NewSymbol(7, 0x31, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->field0c = static_cast<int>(W(M(p, 1), 0));
        Drop(p, 5);
        Drop(p, 4);
        Drop(p, 3);
        Drop(p, 2);
        Drop(p, 0);
        break;
    }
    case 262: {  // an alias of another symbol
        SymbolEntry* const s = NewSymbol(8, 0, false);
        g0 = reinterpret_cast<uint32_t>(s);
        s->SetName(Text(p, 2));
        if (auto* const o = P<SymbolEntry>(p, 0)) {
            s->alias = o;
            o->DecRef();
        }
        Drop(p, 2);
        Drop(p, 3);
        Drop(p, 1);
        break;
    }
    case 264: {  // a variable by name
        PString name;
        SetName(name, Text(p, 0));
        VertexSymbolEntry* const s = ctx->Symbols().Find(name);
        g0 = reinterpret_cast<uint32_t>(s);
        if (s)
            s->AddRef();
        else
            ctx->error = true;  // "Undefined variable name %s."
        Drop(p, 0);
        break;
    }
    case 266: g0 = static_cast<uint32_t>(std::atoi(Text(p, 0))); Drop(p, 0); break;
    case 267: F(g, 0) = static_cast<float>(std::atof(Text(p, 0))); Drop(p, 0); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the run

bool VertexProgramParser::Run(const PString& program_, const PString& file_, ExtendedGfxLanguage* language)
{
    if (fileName.node) PString::Cache()->Destroy(fileName.node);
    ::new (&fileName) PString(file_);
    if (programName.node) PString::Cache()->Destroy(programName.node);
    ::new (&programName) PString(program_);
    program = language;
    VpContext ctx{};
    ctx.program = language;
    ctx.fileName = file_.node ? file_.node->text : nullptr;
    ctx.programName = program_.node ? program_.node->text : nullptr;
    ctx.parser = this;
    auto* const p = VpLemon::New();
    value = nullptr;
    int token = Lex();
    while (token > 0) {
        VpLemon::Parse(p, token, Minor{value, {}}, &ctx);
        value = nullptr;
        if (ctx.error) break;
        token = Lex();
    }
    if (!ctx.error) VpLemon::Parse(p, 0, Minor{value, {}}, &ctx);
    VpLemon::Free(p);
    bool ok = false;
    if (!ctx.error) {
        language->SetProgramName(program_);
        VCall<void>(language, 0x30, static_cast<const void*>(&ctx.names));
        if (VCall<bool>(language, 0x34, static_cast<const void*>(&ctx.Symbols()), ctx.native != nullptr)) {
            if (VCall<int>(language, 0x74) == 0 && ctx.vertexType && ctx.vertexType->length) {
                VertexType* const shared = VertexType::GetVertexType(ctx.VertexTypeName());
                void* const mem = Alloc::New(0x24);
                VertexType* const copy = mem ? ::new (mem) VertexType(*shared) : nullptr;
                reinterpret_cast<ReferenceCount*>(shared)->ReleaseReference();
                VCall<void>(language, 0x70, static_cast<const void*>(copy));
                reinterpret_cast<ReferenceCount*>(copy)->ReleaseReference();
            }
            if (ctx.native)
                ok = VCall<bool>(language, 0xc, static_cast<const void*>(&program_),
                                 static_cast<const char*>(ctx.native));
            else
                ok = VCall<bool>(language, 0x38, static_cast<const void*>(&ctx.instructions)) &&
                     VCall<bool>(language, 0x10);
        }
    }
    ctx.Destroy();
    return ok;
}

// ---- VertexProgram::Load (0x10049540)
bool VertexProgramParse(MemoryFile* in, const PString& program, const PString& file, ExtendedGfxLanguage* lang)
{
    VertexProgramParser parser(in);
    return parser.Run(program, file, lang);
}

// ---- entry points for the differential test (found through the linker map; nothing else calls them)

extern "C" void __cdecl RsTestVpCtor(void* mem, MemoryFile* in) { ::new (mem) VertexProgramParser(in); }
extern "C" bool __cdecl RsTestVpRun(void* p, const PString* program, const PString* file, ExtendedGfxLanguage* lang)
{
    return static_cast<VertexProgramParser*>(p)->Run(*program, *file, lang);
}
extern "C" void __cdecl RsTestVpDtor(void* p) { static_cast<VertexProgramParser*>(p)->~VertexProgramParser(); }
extern "C" int __cdecl RsTestVpLine(void* p) { return static_cast<VertexProgramParser*>(p)->line; }
// the Lemon driver alone, on token streams the test makes up (a context laid out as the original's)
extern "C" void __cdecl RsTestVpParse(LemonStackT<LemonMinor16>* p, int major, void* minor, void* ctx)
{
    VpLemon::Parse(p, major, LemonMinor16{minor, {}}, static_cast<VpContext*>(ctx));
}
extern "C" void __cdecl RsTestVpFree(LemonStackT<LemonMinor16>* p) { VpLemon::Free(p); }
extern "C" void __cdecl RsTestVpDestroy(void* ctx) { static_cast<VpContext*>(ctx)->Destroy(); }
#pragma comment(linker, "/include:_RsTestVpCtor")
#pragma comment(linker, "/include:_RsTestVpRun")
#pragma comment(linker, "/include:_RsTestVpDtor")
#pragma comment(linker, "/include:_RsTestVpLine")
#pragma comment(linker, "/include:_RsTestVpParse")
#pragma comment(linker, "/include:_RsTestVpFree")
#pragma comment(linker, "/include:_RsTestVpDestroy")

}  // namespace Jet
