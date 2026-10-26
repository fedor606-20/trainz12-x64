// Unit U2b2 of RenderSystem_7c: TextureProgramParser, the front end TextureCombiner::Load runs over a texture
// program (scanner tp 0x10044d30, Lemon driver 0x10044830 with reduce 0x10043760, run 0x100462d0). The reduce actions
// build parse records (TexHeaderItem, TexRegisterDefinition, TexApiSection, TexApiCodeLine, TexApiDataItem); the run
// then hands the constants, native sections and instructions to the combiner through its virtuals. The DFA and parser
// tables are the original's (gfxtp_*.h, read from the installed module at build time).
#include "GfxParse.h"

#include <jet/GfxLanguage.h>
#include <jet/VCall.h>

#include <cstdlib>
#include <cstring>
#include <new>

#include "gfxtp_accept.h"
#include "gfxtp_acclist.h"
#include "gfxtp_base.h"
#include "gfxtp_chk.h"
#include "gfxtp_def.h"
#include "gfxtp_ec.h"
#include "gfxtp_lemon.h"
#include "gfxtp_meta.h"
#include "gfxtp_nxt.h"

namespace Jet {

namespace {

const unsigned short accList[] = {GFXTP_ACCLIST_DATA};
const unsigned short accept[] = {GFXTP_ACCEPT_DATA};
const unsigned char ec[] = {GFXTP_EC_DATA};
const unsigned int meta[] = {GFXTP_META_DATA};
const unsigned short base[] = {GFXTP_BASE_DATA};
const unsigned short def[] = {GFXTP_DEF_DATA};
const unsigned short nxt[] = {GFXTP_NXT_DATA};
const unsigned short chk[] = {GFXTP_CHK_DATA};
const FlexTables tables = {accList, accept, ec, meta, base, def, nxt, chk, 0x146, 0x52, true};

}  // namespace

// 0x100b8800: the text of the code or native block being read (a module static like the scanners' state, kept
// between runs)
AnsiString gfxTpCode;

// ---------------------------------------------------------------------------------------------------------------
// the parse records (internal classes with one-slot vftables at 0x100727cc..0x100727fc)

struct TexItem {  // Jet::ParseItem (vftable 0x100727d4)
    virtual ~TexItem() {}
    TexItem* next = nullptr;  // +4
    JET_HEAP_OBJECT
};

struct TexHeaderItem : TexItem {  // 0xc (vftable 0x100727dc)
    int kind = 0;                 // +8: 1 for the register definitions
};

struct TexApiDataItem : TexItem {  // 0xc (vftable 0x100727ec)
    int kind = 0;                  // +8: 1 for the code lines
};

// a constant's value (0x18): up to four floats
struct TexConstant {
    int head = 0;  // +0 always 0 (it stands where a list head would be)
    float v[4];    // +4
    int count;     // +0x14
    JET_HEAP_OBJECT
};

// "c<n> = name (x, y, z, w)" (0x18, vftable 0x100727e4, ctor 0x100431d0)
struct TexRegisterDefinition : TexHeaderItem {
    TexRegisterDefinition(int n, const AnsiString& s, TexConstant* c) : index(n), name(s), value(c) { kind = 1; }
    ~TexRegisterDefinition() override
    {
        if (value) Alloc::Delete(value);
        value = nullptr;
    }
    int index;           // +0xc
    AnsiString name;     // +0x10
    TexConstant* value;  // +0x14 (Jet::Alloc)
};

// an API block (0x10, vftable 0x100727cc): its lines, or the text of a native block
struct TexApiSection : TexItem {
    explicit TexApiSection(ParseList<TexItem>* l) : lines(l) {}  // 0x100433a0
    explicit TexApiSection(const AnsiString& s) : lines(nullptr), text(s) {}  // 0x10043400
    ~TexApiSection() override;
    ParseList<TexItem>* lines;  // +8
    AnsiString text;            // +0xc
};

// an instruction line (0x38, vftables 0x100727f4 / 0x100727fc): the instruction sits at +0xc
struct TexApiCodeLine : TexApiDataItem, TextureCombiner::Instruction {
    TexApiCodeLine(TextureCombiner::TextureOpcode op, ParseList<TextureCombiner::RegisterItem>* r,
                   ParseList<TextureCombiner::ModifierItem>* m, TextureCombiner::TextureAddressType a, float v, bool f)
        : Instruction(op, r, m, a, v, f)
    {
        kind = 1;
    }
    JET_HEAP_OBJECT
};
JET_LAYOUT32(sizeof(TexApiCodeLine) == 0x38 && sizeof(TexRegisterDefinition) == 0x18 && sizeof(TexApiSection) == 0x10);

namespace {

// the items of a record list, each deleted through its vftable, then the list (0x100432e0)
void FreeItems(ParseList<TexItem>* list)
{
    while (TexItem* const item = list->head) {
        list->head = item->next;
        if (!list->head) list->tail = nullptr;
        --list->count;
        delete item;
    }
    Alloc::Delete(list);
}

template <class T>
void Append(ParseList<T>* list, T* item)  // 0x10044ae0 / 0x1001d190
{
    if (!item) return;
    if (list->tail) {
        list->tail->next = item;
        ++list->count;
        list->tail = item;
    } else {
        ++list->count;
        list->head = item;
        list->tail = item;
    }
}

}  // namespace

TexApiSection::~TexApiSection()  // 0x10043460
{
    if (lines) FreeItems(lines);
    lines = nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// the scanner

class TextureProgramParser;

// a "c<n>" name of the constants block (0xc), looked up by the instruction operands
struct TexConstantName {
    int index;
    PString name;
    TexConstantName* next;
    JET_HEAP_OBJECT
};

struct TexContext {  // the run's state (0x100462d0 locals, 0x30 bytes)
    float version;                     // +0
    PString programName;               // +4
    int api;                           // +8 the parser's +0x84
    ParseList<TexItem>* header;        // +0xc
    ExtendedGfxLanguage* lang;         // +0x10
    const char* fileName;              // +0x14
    TextureProgramParser* parser;      // +0x18
    ParseList<TexItem>* sections;      // +0x1c
    TexConstantName* namesHead;        // +0x20
    TexConstantName* namesTail;        // +0x24
    int namesCount;                    // +0x28
    bool error;                        // +0x2c

    void Destroy();  // 0x10044bf0
};
JET_LAYOUT32(sizeof(TexContext) == 0x30);

void TexContext::Destroy()
{
    if (header) FreeItems(header);
    header = nullptr;
    if (sections) FreeItems(sections);
    sections = nullptr;
    while (TexConstantName* const n = namesHead) {  // 0x10044ce0
        namesHead = n->next;
        if (!namesHead) namesTail = nullptr;
        --namesCount;
        delete n;
    }
    if (programName.node) PString::Cache()->Destroy(programName.node);
    programName.node = nullptr;
}

struct TexTraits {
    static constexpr int kNoCode = 0x46;
    static constexpr int kStates = 0x77;
    static constexpr int kRules = 0x45;
    static constexpr int kErrorSymbol = 0x2b;
    static constexpr int kTerminals = 0x20;
    static const LemonAction pool[];
    static const LemonState states[];
    static const LemonRule rules[];
    static void Reduce(LemonStack* p, int rule, TexContext* ctx, void*& gotoMinor);
    static void SyntaxError(TexContext* ctx) { ctx->error = true; }  // "Syntax error at %s." (0x10046580)
    static void Failed(TexContext* ctx) { ctx->error = true; }       // 0x10044780
};
const LemonAction TexTraits::pool[] = {GFXTP_POOL_DATA};
const LemonState TexTraits::states[] = {GFXTP_STATES_DATA};
const LemonRule TexTraits::rules[] = {GFXTP_RULES_DATA};
using TexLemon = LemonParser<TexTraits, TexContext>;

class TextureProgramParser : public FlexScanner {
public:
    TextureProgramParser(MemoryFile* in, int a) : FlexScanner(tables, in), api(a) {}

    bool Run(const PString& file, ExtendedGfxLanguage* language);  // 0x100462d0
    int Lex();                                                      // 0x10044d30

    void* value = nullptr;  // +0x78 the token's text (Jet::Alloc)
    PString fileName;       // +0x7c
    int line = 1;           // +0x80
    int api;                // +0x84 the combiner's vftable slot 17 value (which API this build runs)

private:
    void Push(int condition)
    {
        gfxLex.tpStack[gfxLex.tpDepth++] = gfxLex.tpCurrent;
        gfxLex.tpCurrent = condition;
        start = 2 * condition + 1;
    }
    void Pop()
    {
        gfxLex.tpCurrent = gfxLex.tpStack[--gfxLex.tpDepth];
        start = 2 * gfxLex.tpCurrent + 1;
    }
    void AppendText() { gfxTpCode.Ins(yytext, gfxTpCode.Length()); }
    int Text(int token)
    {
        value = CopyText(yytext);
        return token;
    }
    int OpenApi()  // an API block of another API: skipped as a code block
    {
        gfxLex.tpBlock = 0;
        Push(5);
        return 3;
    }
    int OpenOwnApi()
    {
        gfxLex.tpBraces = 0;
        if (gfxLex.tpNative) {
            gfxTpCode = "";
            Push(6);
        } else {
            Push(2);
        }
        return 3;
    }
};

int TextureProgramParser::Lex()
{
    for (;;) {
        switch (const int act = Scan()) {
        case 1: case 0x4f: ++line; break;
        case 2: Pop(); break;
        case 3: case 4: case 0x13: case 0x1e: case 0x21: case 0x29: case 0x2e: case 0x40: case 0x43: case 0x4d:
        case 0x50:
            break;
        case 5:
            if (gfxLex.tpBlock == 0) return 6;
            break;
        case 6:
            if (gfxLex.tpBlock++ == 0) return 0x1a;
            break;
        case 7:
            if (--gfxLex.tpBlock == 0) {
                Pop();
                return 10;
            }
            break;
        case 8:
            if (gfxLex.tpBlock == 0) return 6;
            AppendText();
            break;
        case 9:
            if (gfxLex.tpBlock++ == 0) return 0x1a;
            AppendText();
            break;
        case 10:
            if (--gfxLex.tpBlock == 0) {
                Pop();
                value = CopyText(gfxTpCode);
                return 0x1b;
            }
            AppendText();
            break;
        case 0xb: AppendText(); ++line; break;
        case 0xc: case 0xd: AppendText(); break;
        case 0xe: case 0x22: case 0x2d: return Text(0x1b);
        case 0xf: case 0x11: return 0x14;
        case 0x10: Pop(); return 6;
        case 0x12: return 6;
        case 0x14: return 0x1e;
        case 0x15: Push(3); return 0xd;
        case 0x16: case 0x26: case 0x48: return Text(0xc);
        case 0x17: gfxLex.tpNative = 0; return 2;
        case 0x18: gfxLex.tpNative = 1; return 4;
        case 0x19: return api != 0 ? OpenApi() : OpenOwnApi();
        case 0x1a: return api != 2 ? OpenApi() : OpenOwnApi();
        case 0x1b:
            Push(2);
            gfxLex.tpBraces = 0;
            return 3;
        case 0x1c: case 0x39: return Text(0x11);
        case 0x1d: case 0x23: return 0x1a;
        case 0x1f: return 10;
        case 0x20: case 0x28: case 0x44: case 0x4e: Push(1); break;
        case 0x24: return 0x16;
        case 0x25: return 0x1c;
        case 0x27: case 0x49: return Text(0xf);
        case 0x2a: case 0x3a: case 0x4a: return 7;
        case 0x2b: return 0x19;
        case 0x2c: Pop(); return 10;
        case 0x2f: return 2;
        case 0x30: return api == 0 ? 3 : OpenApi();
        case 0x31: return api == 2 ? 3 : OpenApi();
        case 0x32: return 0x1f;
        case 0x33: return Text(0x20);
        case 0x34:
            value = CopyText(yytext);
            Push(7);
            return 9;
        case 0x35: return Text(0x12);
        case 0x36: return Text(0x17);
        case 0x37: return 0x13;
        case 0x38: return 5;
        case 0x3b: return 0x15;
        case 0x3c: return 0x10;
        case 0x3d: return Text(0xe);
        case 0x3e: return Text(0x18);
        case 0x3f: return Text(0x1d);
        case 0x41: return 0xb;
        case 0x42: ++line; return 0xb;
        case 0x45: ++gfxLex.tpBraces; return 0x1a;
        case 0x46:
            if (--gfxLex.tpBraces == 0) {  // (the entry below the top: an empty stack reads the quick parser's)
                gfxLex.tpCurrent = (&gfxLex.tpStack[0])[gfxLex.tpDepth - 1];
                --gfxLex.tpDepth;
            }
            start = 2 * gfxLex.tpCurrent + 1;
            return 10;
        case 0x47: return Text(1);
        case 0x4b: return Text(8);
        case 0x4c:
            ++line;
            Pop();
            return 0xb;
        case 0x51: Output(yytext, yyleng); break;
        case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x58: case 0x59: case 0x5a: return 0;
        default: (void)act; Error("fatal flex scanner internal error--no action found");
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
// 0x1006ae70: the instructions (looked up in lower case; unknown ones are 3, "add")
const NameValue kOpcodes[] = {
    {"def", 0}, {"ps", 1}, {"phase", 2}, {"add", 3}, {"bem", 4}, {"cmp", 5}, {"cnd", 6}, {"dp3", 7}, {"dp4", 8},
    {"lrp", 9}, {"mad", 10}, {"mov", 11}, {"mul", 12}, {"sub", 13}, {"tex", 14}, {"texbem", 15}, {"texbeml", 16},
    {"texcoord", 17}, {"texcrd", 18}, {"texdepth", 19}, {"texdp3", 20}, {"texdp3tex", 21}, {"texkill", 22},
    {"texld", 23}, {"texreg2ar", 24}, {"texreg2gb", 25}, {"texreg2rgb", 26}, {"texm3x2depth", 27},
    {"texm3x2pad", 28}, {"texm3x2tex", 29}, {"texm3x3", 30}, {"texm3x3pad", 31}, {"texm3x3tex", 32},
    {"texm3x3spec", 33}, {"texm3x3vspec", 34}, {nullptr, 0}};
// 0x1006b0e0: texture address types (lower case; unknown ones are 0)
const NameValue kAddressTypes[] = {{"1d", 1}, {"2d", 2}, {"3d", 3}, {"rect", 4}, {"cube", 5}, {nullptr, 0}};
// 0x1006ae28: register files (unknown ones are 3)
const NameValue kRegisterTypes[] = {{"c", 0}, {"C", 0}, {"r", 1}, {"R", 1}, {"v", 2},
                                    {"V", 2}, {"t", 3}, {"T", 3}, {nullptr, 0}};
// 0x1006af90: instruction and register modifiers
const NameValue kModifiers[] = {
    {"_x2", 1},   {"_x4", 2},   {"_x8", 3},   {"_d2", 4},   {"_d4", 5},   {"_d8", 6},   {"_dz", 7},
    {"_dw", 8},   {"_db", 7},   {"_da", 8},   {"_sat", 9},  {"_bias", 12}, {"_bx2", 14}, {".r", 15},
    {".g", 16},   {".b", 17},   {".a", 18},   {".rg", 19},  {".rb", 20},  {".ra", 21},  {".gb", 22},
    {".ga", 23},  {".ba", 24},  {".rgb", 25}, {".rga", 26}, {".gba", 27}, {".rba", 28}, {".x", 15},
    {".y", 16},   {".z", 17},   {".w", 18},   {".xy", 19},  {".xz", 20},  {".xw", 21},  {".yz", 22},
    {".yw", 23},  {".zw", 24},  {".xyz", 25}, {".xyw", 26}, {".yzw", 27}, {".xzw", 28}, {nullptr, 0}};

int FindExact(const NameValue* table, const char* name, int otherwise)
{
    for (; table->name; ++table)
        if (!std::strcmp(table->name, name)) return table->value;
    return otherwise;
}
int FindLower(const NameValue* table, const char* name, int otherwise)
{
    AnsiString s(name);
    s.ToLower();
    for (; table->name; ++table)
        if (s.Compare(table->name) == 0) return table->value;
    return otherwise;
}
// 0x100435c0: "_x2" is 1 for an instruction, 13 for a register; 0 when unknown
int Modifier(const char* name, bool instruction)
{
    const int v = FindExact(kModifiers, name, 0);
    return v != 1 ? v : instruction ? 1 : 13;
}

LemonEntry* At(LemonStack* p, int k) { return p->top - k; }
template <class T = char>
T* M(LemonStack* p, int k)
{
    return static_cast<T*>(At(p, k)->minor);
}
template <class T>
T Bits(LemonStack* p, int k)  // a value carried in the minor itself
{
    T v;
    std::memcpy(&v, &At(p, k)->minor, sizeof v);
    return v;
}
template <class T>
void* AsMinor(T v)
{
    void* m = nullptr;
    std::memcpy(&m, &v, sizeof v);
    return m;
}
void Free(LemonStack* p, int k)  // a token's text (small values are not pointers)
{
    LemonEntry* const e = At(p, k);
    if (reinterpret_cast<uintptr_t>(e->minor) > 0x100) {
        if (e->minor) Alloc::Delete(e->minor);
        e->minor = nullptr;
    }
}
void Drop(LemonStack* p, int k)  // a text known to be one
{
    LemonEntry* const e = At(p, k);
    if (e->minor) Alloc::Delete(e->minor);
    e->minor = nullptr;
}
void SetName(PString& s, const char* text)  // 0x1001d1e0
{
    if (s.node) PString::Cache()->Destroy(s.node);
    s.node = text ? PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1) : nullptr;
}

using Regs = ParseList<TextureCombiner::RegisterItem>;
using Mods = ParseList<TextureCombiner::ModifierItem>;
using Items = ParseList<TexItem>;
using TexOp = TextureCombiner::TextureOpcode;
using TexAddr = TextureCombiner::TextureAddressType;
using RegType = TextureCombiner::RegType;
using ModType = TextureCombiner::ModifierType;

}  // namespace

void TexTraits::Reduce(LemonStack* p, int rule, TexContext* ctx, void*& gotoMinor)
{
    switch (rule) {
    case 1:
        ctx->version = static_cast<float>(std::atof(M(p, 0)));
        Drop(p, 0);
        Free(p, 1);
        break;
    case 2:
        ctx->header = M<Items>(p, 1);
        SetName(ctx->programName, M(p, 4));
        Drop(p, 4);
        Free(p, 6);
        Free(p, 5);
        Free(p, 3);
        Free(p, 2);
        Free(p, 0);
        break;
    case 3: case 7: case 12: case 28: case 35: case 63: gotoMinor = M<void>(p, 0); break;
    case 4: case 11: case 49: case 52: case 57: gotoMinor = nullptr; break;
    case 5: case 25:
        Append(M<Items>(p, 1), M<TexItem>(p, 0));
        gotoMinor = M<void>(p, 1);
        break;
    case 6: case 26: {
        auto* const list = new Items{nullptr, nullptr, 0};
        gotoMinor = list;
        Append(list, M<TexItem>(p, 0));
        break;
    }
    case 8: gotoMinor = new TexHeaderItem; break;
    case 9: {  // c<n> = name (value)
        const int n = std::atoi(M(p, 4));
        if (M<void>(p, 1)) {
            void* const mem = TexRegisterDefinition::operator new(sizeof(TexRegisterDefinition));
            const char* const name = M(p, 3);
            gotoMinor = mem ? ::new (mem) TexRegisterDefinition(n, AnsiString(name ? name : ""), M<TexConstant>(p, 1))
                            : nullptr;
        } else {
            gotoMinor = new TexHeaderItem;
            Drop(p, 1);
        }
        if (const char* const name = M(p, 3)) {  // 0x10043530
            auto* const entry = new TexConstantName;
            entry->index = n;
            entry->name.node = nullptr;
            entry->next = nullptr;
            SetName(entry->name, name);
            if (ctx->namesTail)
                ctx->namesTail->next = entry;
            else
                ctx->namesHead = entry;
            ++ctx->namesCount;
            ctx->namesTail = entry;
        }
        Drop(p, 3);
        Drop(p, 4);
        Free(p, 5);
        Free(p, 2);
        Free(p, 0);
        break;
    }
    case 10: case 61:
        gotoMinor = M<void>(p, 1);
        Free(p, 2);
        Free(p, 0);
        break;
    case 13: {
        auto* const c = M<TexConstant>(p, 2);
        if (c->count < 4) c->v[c->count++] = Bits<float>(p, 0);
        gotoMinor = c;
        Free(p, 1);
        break;
    }
    case 14: {
        auto* const c = new TexConstant;
        c->v[0] = Bits<float>(p, 0);
        c->v[1] = c->v[2] = c->v[3] = 0;
        c->count = 1;
        gotoMinor = c;
        break;
    }
    case 15:
        gotoMinor = AsMinor(static_cast<float>(std::atof(M(p, 0))));
        Drop(p, 0);
        break;
    case 16:
        gotoMinor = AsMinor(static_cast<float>(std::atoi(M(p, 0))));
        Drop(p, 0);
        break;
    case 17: ctx->sections = M<Items>(p, 0); break;
    case 19:
        if (M<void>(p, 0)) Append(M<Items>(p, 1), M<TexItem>(p, 0));
        gotoMinor = M<void>(p, 1);
        break;
    case 20: {
        auto* const list = new Items{nullptr, nullptr, 0};
        gotoMinor = list;
        if (M<void>(p, 0)) Append(list, M<TexItem>(p, 0));
        break;
    }
    case 21:
        gotoMinor = new TexApiSection(M<Items>(p, 1));
        Free(p, 7);
        Free(p, 6);
        Free(p, 5);
        Free(p, 4);
        Free(p, 2);
        Free(p, 0);
        break;
    case 22: case 24:
        gotoMinor = nullptr;
        Free(p, 6);
        Free(p, 5);
        Free(p, 4);
        Free(p, 3);
        Free(p, 1);
        Free(p, 0);
        break;
    case 23: {
        void* const mem = TexApiSection::operator new(sizeof(TexApiSection));
        gotoMinor = mem ? ::new (mem) TexApiSection(AnsiString(M(p, 0))) : nullptr;
        Drop(p, 0);
        Free(p, 6);
        Free(p, 5);
        Free(p, 4);
        Free(p, 3);
        Free(p, 1);
        break;
    }
    case 27: {  // the second list's items moved to the end of the first
        Items* const a = M<Items>(p, 1);
        Items* const b = M<Items>(p, 0);
        if (b->count) {
            a->count += b->count;
            a->tail->next = b->head;
            a->tail = b->tail;
            if (a->count == 0) a->head = b->head;
        }
        gotoMinor = a;
        b->head = b->tail = nullptr;
        b->count = 0;
        if (b) FreeItems(b);
        At(p, 0)->minor = nullptr;
        break;
    }
    case 29:
        gotoMinor = M<void>(p, 1);
        Free(p, 7);
        Free(p, 6);
        Free(p, 5);
        Free(p, 4);
        Free(p, 2);
        Free(p, 0);
        break;
    case 30:
        gotoMinor = new Items{nullptr, nullptr, 0};
        Free(p, 6);
        Free(p, 5);
        Free(p, 4);
        Free(p, 3);
        Free(p, 1);
        Free(p, 0);
        break;
    case 33: case 34: Free(p, 0); break;
    case 36:
        gotoMinor = new TexApiCodeLine(Bits<TexOp>(p, 3), M<Regs>(p, 1), nullptr, Bits<TexAddr>(p, 2), 0,
                                       Bits<unsigned char>(p, 4) != 0);
        Free(p, 0);
        break;
    case 37:
        gotoMinor = new TexApiCodeLine(Bits<TexOp>(p, 4), M<Regs>(p, 1), M<Mods>(p, 2), Bits<TexAddr>(p, 3), 0,
                                       Bits<unsigned char>(p, 5) != 0);
        Free(p, 0);
        break;
    case 38: gotoMinor = new TexApiDataItem; Free(p, 0); break;
    case 39: gotoMinor = new TexApiDataItem; break;
    case 40: {  // a line with a value only
        void* const mem = TexApiCodeLine::operator new(sizeof(TexApiCodeLine));
        gotoMinor = mem ? ::new (mem) TexApiCodeLine(static_cast<TexOp>(1), nullptr, nullptr, static_cast<TexAddr>(0),
                                                     static_cast<float>(std::atof(M(p, 1))), false)
                        : nullptr;
        Drop(p, 1);
        Free(p, 2);
        Free(p, 0);
        break;
    }
    case 41: {  // "c<n> (x, y, z, w)": an instruction of opcode 0 (def) on constant n
        auto* const regs = new Regs{nullptr, nullptr, 0};
        const int n = std::atoi(M(p, 3) + 1);
        Append(regs, new TextureCombiner::RegisterItem(static_cast<RegType>(0), n));
        auto* const line = new TexApiCodeLine(static_cast<TexOp>(0), regs, nullptr, static_cast<TexAddr>(0), 0, false);
        const TexConstant* const c = M<TexConstant>(p, 1);
        std::memcpy(&line->field18, c->v, sizeof c->v);
        gotoMinor = static_cast<TexItem*>(line);
        Drop(p, 1);
        Drop(p, 3);
        Free(p, 4);
        Free(p, 2);
        Free(p, 0);
        break;
    }
    case 42: gotoMinor = reinterpret_cast<void*>(1); Free(p, 0); break;
    case 43: gotoMinor = nullptr; break;
    case 44: case 65:
        gotoMinor = M<void>(p, 1);
        Append(M<Mods>(p, 1), M<TextureCombiner::ModifierItem>(p, 0));
        break;
    case 45: case 66: {
        auto* const list = new Mods{nullptr, nullptr, 0};
        gotoMinor = list;
        Append(list, M<TextureCombiner::ModifierItem>(p, 0));
        break;
    }
    case 46: case 67: {
        void* const mem = TextureCombiner::ModifierItem::operator new(sizeof(TextureCombiner::ModifierItem));
        gotoMinor = mem ? ::new (mem) TextureCombiner::ModifierItem(static_cast<ModType>(Modifier(M(p, 0), rule == 46)))
                        : nullptr;
        Drop(p, 0);
        break;
    }
    case 47:
        gotoMinor = AsMinor(FindLower(kOpcodes, M(p, 0), 3));
        Drop(p, 0);
        break;
    case 48:
        gotoMinor = AsMinor(FindLower(kAddressTypes, M(p, 1), 0));
        Drop(p, 1);
        Free(p, 2);
        Free(p, 0);
        break;
    case 50:
        Append(M<Regs>(p, 2), M<TextureCombiner::RegisterItem>(p, 0));
        gotoMinor = M<void>(p, 2);
        Free(p, 1);
        break;
    case 51: {
        auto* const list = new Regs{nullptr, nullptr, 0};
        gotoMinor = list;
        Append(list, M<TextureCombiner::RegisterItem>(p, 0));
        break;
    }
    case 53:
        gotoMinor = M<void>(p, 0);
        M<TextureCombiner::RegisterItem>(p, 0)->mods1 = M<Mods>(p, 1);
        break;
    case 54: {
        auto* const r = M<TextureCombiner::RegisterItem>(p, 1);
        r->mods1 = M<Mods>(p, 2);
        gotoMinor = r;
        r->mods2 = M<Mods>(p, 0);
        break;
    }
    case 55: case 56: {
        if (rule == 55) (void)std::atoi(M(p, 1));
        auto* const list = new Mods{nullptr, nullptr, 0};
        gotoMinor = list;
        Append(list, new TextureCombiner::ModifierItem(static_cast<ModType>(rule == 55 ? 10 : 11)));
        if (rule == 55) Drop(p, 1);
        Free(p, 0);
        break;
    }
    case 58:
        gotoMinor = new TextureCombiner::RegisterItem(Bits<RegType>(p, 1), Bits<int>(p, 0));
        break;
    case 59:
        gotoMinor = AsMinor(FindExact(kRegisterTypes, M(p, 0), 3));
        Drop(p, 0);
        break;
    case 60: case 62:
        gotoMinor = AsMinor(std::atoi(M(p, 0)));
        Drop(p, 0);
        break;
    case 64: {  // a constant by name: its number
        PString name;
        SetName(name, M(p, 0) + 1);
        const TexConstantName* found = nullptr;
        {
            const PString key(name);  // (passed by value to 0x10044b00)
            for (const TexConstantName* e = ctx->namesHead; e; e = e->next)
                if (e->name.CompareIgnoreCase(key) == 0) {
                    found = e;
                    break;
                }
        }
        gotoMinor = found ? AsMinor(found->index) : nullptr;
        Drop(p, 0);
        break;
    }
    case 68: {
        const int v = Modifier(M(p, 0), true);
        if (v != 1) ctx->error = true;
        gotoMinor = new TextureCombiner::ModifierItem(static_cast<ModType>(v));
        Drop(p, 0);
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the run

namespace {

// 0x10046480: the constants, then each API section (a native text, or its instructions and a compile); false as
// soon as the combiner refuses one
bool Emit(const TexContext& ctx, ExtendedGfxLanguage* lang)
{
    if (ctx.header)
        for (const TexItem* item = ctx.header->head; item; item = item->next) {
            const auto* const def = static_cast<const TexRegisterDefinition*>(item);
            if (def->kind == 1 && def->value->head == 0) {
                float v[4] = {def->value->v[0], def->value->v[1], def->value->v[2], def->value->v[3]};
                VCall<bool>(lang, 0x34, def->index, &def->name, static_cast<const void*>(v));  // TrackConstant
            }
        }
    if (ctx.sections)
        for (const TexItem* item = ctx.sections->head; item; item = item->next) {
            const auto* const section = static_cast<const TexApiSection*>(item);
            bool ok;
            if (!section->lines) {
                if (!section->text.Length()) continue;
                ok = lang->LoadNativeFormat(ctx.programName, section->text);
            } else {
                for (const TexItem* l = section->lines->head; l; l = l->next) {
                    const auto* const line = static_cast<const TexApiDataItem*>(l);
                    if (line->kind == 1)
                        VCall<void>(lang, 0x2c, static_cast<const TextureCombiner::Instruction*>(
                                                    static_cast<const TexApiCodeLine*>(line)));
                }
                ok = lang->CompileProgram();
            }
            if (!ok) return false;
        }
    return true;
}

}  // namespace

bool TextureProgramParser::Run(const PString& file, ExtendedGfxLanguage* language)
{
    if (fileName.node) PString::Cache()->Destroy(fileName.node);
    ::new (&fileName) PString(file);
    TexContext ctx{};
    ctx.api = api;
    ctx.lang = language;
    ctx.fileName = file.node ? file.node->text : nullptr;
    ctx.parser = this;
    LemonStack* const p = TexLemon::New();
    value = nullptr;
    int token = Lex();
    while (token > 0) {
        TexLemon::Parse(p, token, value, &ctx);
        value = nullptr;
        if (ctx.error) break;
        token = Lex();
    }
    if (ctx.error) {
        TexLemon::Free(p);
        ctx.Destroy();
        return false;
    }
    TexLemon::Parse(p, 0, value, &ctx);
    TexLemon::Free(p);
    language->SetProgramName(ctx.programName);
    const bool ok = Emit(ctx, language);
    ctx.Destroy();
    return ok;
}

// ---- TextureCombiner::Load (0x10040550)
bool TextureProgramParse(MemoryFile* in, int api, const PString& file, ExtendedGfxLanguage* lang)
{
    TextureProgramParser parser(in, api);
    return parser.Run(file, lang);
}

// ---- entry points for the differential test (found through the linker map; nothing else calls them)

extern "C" void __cdecl RsTestTpCtor(void* mem, MemoryFile* in, int api) { ::new (mem) TextureProgramParser(in, api); }
extern "C" bool __cdecl RsTestTpRun(void* p, const PString* file, ExtendedGfxLanguage* lang)
{
    return static_cast<TextureProgramParser*>(p)->Run(*file, lang);
}
extern "C" void __cdecl RsTestTpDtor(void* p) { static_cast<TextureProgramParser*>(p)->~TextureProgramParser(); }
extern "C" int __cdecl RsTestTpLine(void* p) { return static_cast<TextureProgramParser*>(p)->line; }
// the Lemon driver alone, on token streams the test makes up (a context laid out as the original's)
extern "C" void __cdecl RsTestTpParse(LemonStack* p, int major, void* minor, void* ctx)
{
    TexLemon::Parse(p, major, minor, static_cast<TexContext*>(ctx));
}
extern "C" void __cdecl RsTestTpFree(LemonStack* p) { TexLemon::Free(p); }
extern "C" void __cdecl RsTestTpDestroy(void* ctx) { static_cast<TexContext*>(ctx)->Destroy(); }
#pragma comment(linker, "/include:_RsTestTpCtor")
#pragma comment(linker, "/include:_RsTestTpRun")
#pragma comment(linker, "/include:_RsTestTpDtor")
#pragma comment(linker, "/include:_RsTestTpLine")
#pragma comment(linker, "/include:_RsTestTpParse")
#pragma comment(linker, "/include:_RsTestTpFree")
#pragma comment(linker, "/include:_RsTestTpDestroy")

}  // namespace Jet
