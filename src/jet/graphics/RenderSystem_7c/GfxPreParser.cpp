// Unit U2b2 of RenderSystem_7c: GfxLanguagePreParser, the pass VertexProgram::Load runs over a program file before
// the vertex program parser: it keeps the one program asked for, evaluates the #if/#ifdef blocks against the API
// (ExtendedGfxLanguage::Defines and the Display's capabilities) and writes the kept text with #CODE / #BINDING /
// #NATIVE / #LINE markers (object 0x1002b9d0, scanner gfxpp 0x1002cf30, Lemon driver 0x1002cad0 with reduce
// 0x1002c340, run 0x1002bbd0). The DFA and parser tables are the original's (gfxpp_*.h, read at build time).
#include "GfxParse.h"

#include <jet/GfxLanguage.h>

#include <cstdlib>
#include <cstring>
#include <new>

#include "gfxpp_accept.h"
#include "gfxpp_acclist.h"
#include "gfxpp_base.h"
#include "gfxpp_chk.h"
#include "gfxpp_def.h"
#include "gfxpp_ec.h"
#include "gfxpp_lemon.h"
#include "gfxpp_meta.h"
#include "gfxpp_nxt.h"

namespace Jet {

namespace {

const unsigned short accList[] = {GFXPP_ACCLIST_DATA};
const unsigned short accept[] = {GFXPP_ACCEPT_DATA};
const unsigned char ec[] = {GFXPP_EC_DATA};
const unsigned int meta[] = {GFXPP_META_DATA};
const unsigned short base[] = {GFXPP_BASE_DATA};
const unsigned short def[] = {GFXPP_DEF_DATA};
const unsigned short nxt[] = {GFXPP_NXT_DATA};
const unsigned short chk[] = {GFXPP_CHK_DATA};
const FlexTables tables = {accList, accept, ec, meta, base, def, nxt, chk, 0x11d, 80, true};

// the capabilities #if can name, in the order Display's slot 7 numbers them (0x100b6c9c)
const char* const kCapabilities[33] = {
    "D_MULTI_TEXTURE", "D_MULTI_TEX_COMBINE", "D_MULTI_TEX_COMBINE4", "D_MULTI_TEX_MULTIPLY_ADD",
    "D_MULTI_TEX_FULL_SCALE", "D_AUTOMIP", "D_ALPHA_TO_COVERAGE", "D_ALPHA_TO_COVERAGE_ATI", "D_RENDER_TO_TEXTURE",
    "D_DEPTH_TEXTURE", "D_STENCIL_BUFFER", "D_DEPTH_STENCIL_BUFFER", "D_STENCIL_TWO_SIDE", "D_STENCIL_WRAP",
    "D_TEXGEN_OBJECT_SPACE", "D_BUFFER_SAVES", "D_DEPTH_CLAMP", "D_VERTEX_PROGRAM", "D_VERTEX_PROGRAM_1",
    "D_VERTEX_PROGRAM_2", "D_VERTEX_PROGRAM_3", "D_PIXEL_SHADER_1_1", "D_PIXEL_SHADER_1_3", "D_PIXEL_SHADER_1_4",
    "D_PIXEL_SHADER_2_0", "D_FRAGMENT_PROGRAM_1", "D_SPHERE_MAPPING", "D_CUBE_MAPPING", "D_DOT3_BUMPMAPPING",
    "D_HW_VERTEX_PROCESSING", "D_VERTEX_BUFFERS", "D_CONCURRENT_CONTEXTS", "D_BLEND_EQUATION"};

const char kLineFormat[] = "%s %d\n";

bool DisplayCapability(const ExtendedGfxLanguage* lang, int index)  // the Display's slot 7
{
    Display* const d = lang->GetDisplay();
    using Fn = bool(__cdecl*)(Display*, int);
    return reinterpret_cast<Fn>((*reinterpret_cast<void* const* const*>(d))[7])(d, index);
}

PString TextName(const char* text)  // an interned name, nothing for no text
{
    PString n;
    if (text) n.node = PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1);
    return n;
}

}  // namespace

class GfxLanguagePreParser;

struct PreContext {  // the run's state (0x1002bbd0 locals, 0x24 bytes)
    float version;            // +0 "version x" of the file
    float generation;         // +4 the program's generation (1 unless given)
    int param;                // +8
    ExtendedGfxLanguage* lang;  // +0xc
    GfxNameList* names;       // +0x10 the kept program's names (Jet::Alloc)
    const char* fileName;     // +0x14
    const char* programName;  // +0x18 the program to keep
    bool error;               // +0x1c
    bool found;               // +0x1d
    bool unused1e;            // +0x1e
    GfxLanguagePreParser* parser;  // +0x20
};

struct PreTraits {
    static constexpr int kNoCode = 0x3d;
    static constexpr int kStates = 0x5f;
    static constexpr int kRules = 0x36;
    static constexpr int kErrorSymbol = 0x24;
    static constexpr int kTerminals = 0x1d;
    static const LemonAction pool[];
    static const LemonState states[];
    static const LemonRule rules[];
    static void Reduce(LemonStack* p, int rule, PreContext* ctx, void*& gotoMinor);
    static void SyntaxError(PreContext* ctx) { ctx->error = true; }
    static void Failed(PreContext* ctx) { ctx->error = true; }
};
const LemonAction PreTraits::pool[] = {GFXPP_POOL_DATA};
const LemonState PreTraits::states[] = {GFXPP_STATES_DATA};
const LemonRule PreTraits::rules[] = {GFXPP_RULES_DATA};
using PreLemon = LemonParser<PreTraits, PreContext>;

class GfxLanguagePreParser : public FlexScanner {
public:
    GfxLanguagePreParser(int p, MemoryFile* in) : FlexScanner(tables, in), param(p)
    {
        output.SetLength(in->size);  // room for the whole text
    }
    ~GfxLanguagePreParser() override { conditions.Clear(); }

    // 0x1002bbd0: keep `program` of the text; false on a syntax error, an unsupported generation or no such
    // program. The language gets the program's generation.
    bool Run(const PString& program, const PString& file, ExtendedGfxLanguage* language);
    int Lex();  // 0x1002cf30

    void* value = nullptr;               // +0x78 the token's text (Jet::Alloc)
    PString fileName;                    // +0x7c
    PString programName;                 // +0x80
    int line = 1;                        // +0x84
    AnsiString output;                   // +0x88
    ExtendedGfxLanguage* lang = nullptr;  // +0x8c
    PreContext* context = nullptr;       // +0x90
    int param;                           // +0x94
    GfxIntStack conditions;              // +0x98 the #if expression being evaluated

private:
    void Echo() { output.Ins(yytext, output.Length()); }
    void Mark(const char* marker) { output.Addf(kLineFormat, marker, line); }
    void PushCondition(int c)  // BEGIN on the static stack
    {
        gfxLex.ppStack[gfxLex.ppDepth++] = gfxLex.ppCurrent;
        gfxLex.ppCurrent = c;
        start = 2 * c + 1;
    }
    void PopCondition()
    {
        gfxLex.ppCurrent = gfxLex.ppStack[--gfxLex.ppDepth];
        start = 2 * gfxLex.ppCurrent + 1;
    }
    void Replace(int c)  // the top of the stack replaced (the original stores the old top back into place)
    {
        gfxLex.ppCurrent = c;
        start = 2 * c + 1;
    }
    void Evaluate(bool v);           // 0x1002bee0: a value into the expression
    void CloseParenthesis();         // 0x1002be30
    void IfProgramName(const char* text);  // 0x1002bfc0: is it one of the kept program's names?
};

// 0x1002bee0: fold the value into the operators waiting on the stack: 3 and, 4 or, 5 not, 2 an open parenthesis
// (kept, the value pushed above it); a value under it ends the fold and the new one is lost
void GfxLanguagePreParser::Evaluate(bool v)
{
    while (conditions.head) {
        const int op = conditions.Pop();
        if (op == 3) {
            const int w = conditions.Pop();
            v = w == 1 && v;
        } else if (op == 4) {
            const int w = conditions.Pop();
            v = w == 1 || v;
        } else if (op == 5) {
            v = !v;
        } else {
            if (op == 2) {
                conditions.Push(2);
                conditions.Push(v ? 1 : 0);
            }
            return;
        }
    }
    conditions.Push(v ? 1 : 0);
}

void GfxLanguagePreParser::CloseParenthesis()
{
    const int v = conditions.Pop();
    conditions.Pop();  // the parenthesis
    Evaluate(v != 0);
}

void GfxLanguagePreParser::IfProgramName(const char* text)
{
    GfxNameList* const names = context->names;
    if (!names) {
        Evaluate(true);
        return;
    }
    PString n = TextName(text);
    bool found = false;
    if (GfxNameList::Chunk* const first = names->head) {
        GfxNameList::Chunk* c = first;
        int i = 0;
        for (;;) {  // (the first slot of a chunk is read even when the chunk is empty)
            if (c->names[i] == n.node) found = true;
            if (++i >= c->count) {
                c = c->next;
                if (c == first) break;
                i = 0;
            }
        }
    }
    Evaluate(found);
}

int GfxLanguagePreParser::Lex()
{
    GfxLexStatics& s = gfxLex;
    for (;;) {
        switch (const int act = Scan()) {
        case 1: case 77: ++line; break;
        case 2: PopCondition(); break;
        case 3: case 73: case 76: case 78: break;
        case 4: s.ppIfDepth = 0; s.pp8062 = 0; return 0x1d;
        case 5: PushCondition(3); s.pp805c = 0; return 0x1a;
        case 6: return 0x10;
        case 7: case 18: value = CopyText(yytext); return 0x11;
        case 8: PushCondition(4); s.pp8060 = 0; return 2;
        case 9: PushCondition(4); s.pp8060 = 1; return 4;
        case 10: return 0x18;
        case 11: return 7;
        case 12: ++s.pp805c; return 0x17;
        case 13:
            if (--s.pp805c == 0) PopCondition();
            return 6;
        case 14: return 0xf;
        case 15: return 0x1c;
        case 16: return 9;
        case 17: return 0x1a;
        case 19: return 0x12;
        case 20: value = CopyText(yytext); return 0x13;
        case 21: s.pp8043 = s.pp8060 == 0; return 3;
        case 22: s.pp8043 = 0; return 0x18;
        case 23:
            if (s.pp8043 && !s.pp8062) {
                s.pp8058 = 0;
                s.pp8061 = 0;
                s.pp8062 = 1;
                if (s.pp8060) {
                    Replace(7);
                    s.pp8063 = 0;
                } else {
                    Replace(2);
                }
            } else {
                s.pp8064 = 0;
                Replace(5);
                s.pp8063 = 0;
            }
            return 7;
        case 24: return 0x19;
        case 25:
            if (!s.pp8043) {
                PString n = TextName(yytext);
                s.pp8043 = lang->Defines(n);
            }
            return 3;
        case 26:
            if (s.pp8058++ == 0)
                Mark("\n#CODE");
            else
                Echo();
            break;
        case 27:
            if (--s.pp8058 == 0) {
                PopCondition();
                Mark("\n#ENDCODE");
                return 0x1b;
            }
            Echo();
            break;
        case 28: case 38: case 41: ++line; Echo(); break;
        case 29: case 44: case 30: case 45:
            ++s.ppIfDepth;
            s.pp7e80[s.ppIfDepth] = 0;
            s.pp7dc8[s.ppIfDepth] = 0;
            PushCondition(8);
            s.pp7e2c = 1;
            if (act == 30 || act == 45) conditions.Push(5);
            return 0x14;
        case 31: case 46: PushCondition(9); break;
        case 32: case 47: PushCondition(9); return 0xb;
        case 33: case 48:
            s.pp7e80[s.ppIfDepth] = 1;
            PushCondition(8);
            return 0xd;
        case 34: case 39: case 49: case 50: case 51: Echo(); break;
        case 35:
            s.pp8063 = 0;
            Mark("\n#BINDING");
            s.pp8061 = 1;
            return 8;
        case 36:
            if (s.pp8063) {
                Echo();
                break;
            }
            ++s.pp8058;
            Replace(6);
            if (s.pp8061) {
                Mark("\n#ENDBINDING\n#NATIVE");
                return 5;
            }
            Mark("\n#NATIVE");
            break;
        case 37: s.pp8063 = 1; Echo(); break;
        case 40: s.pp8063 = 0; Echo(); break;
        case 42:
            if (s.pp8058++ != 0) Echo();
            break;
        case 43:
            if (--s.pp8058 != 0) {
                Echo();
                break;
            }
            Mark("\n#ENDNATIVE");
            PopCondition();
            return 0x1b;
        case 52:
            ++line;
            if (s.pp7e2c) {  // the end of an #if line: its value decides
                const int v = conditions.Pop();
                s.pp7e2c = 0;
                if (v == 1) {
                    s.pp7ee8[s.ppIfDepth] = 0;
                    PopCondition();
                    Mark("\n#LINE");
                } else {
                    s.pp7ee8[s.ppIfDepth] = 1;
                    Replace(9);
                }
                return 0xe;
            }
            if (s.pp7e80[s.ppIfDepth]) {
                --s.ppIfDepth;
                PopCondition();
                Mark("\n#LINE");
            }
            break;
        case 53: conditions.Push(2); return 0x18;
        case 54: CloseParenthesis(); return 7;
        case 55: conditions.Push(4); return 0x19;
        case 56: conditions.Push(3); return 1;
        case 57: conditions.Push(5); return 0x15;
        case 58: IfProgramName(yytext); return 0x16;
        case 59: PushCondition(0xa); break;
        case 60: PushCondition(0xb); break;
        case 61: case 63: PopCondition(); return 0x16;
        case 62: {
            PString n = TextName(yytext);
            Evaluate(lang->Defines(n));
            break;
        }
        case 64: {
            PString n = TextName(yytext);
            bool v = false;
            for (int i = 0; i < 33; ++i)
                if (n.Compare(kCapabilities[i]) == 0) {
                    v = DisplayCapability(lang, i);
                    break;
                }
            Evaluate(v);
            break;
        }
        case 65: case 66:
            ++s.ppIfDepth;
            s.pp7e80[s.ppIfDepth] = 0;
            s.pp7dc8[s.ppIfDepth] = 1;
            break;
        case 67:
            if (!s.pp7dc8[s.ppIfDepth] && s.pp7ee8[s.ppIfDepth]) {
                s.pp7e2c = 1;
                Replace(8);
                return 0xc;
            }
            break;
        case 68:
            if (!s.pp7dc8[s.ppIfDepth] && s.pp7ee8[s.ppIfDepth]) {
                PopCondition();
                Mark("\n#LINE");
                return 0xb;
            }
            break;
        case 69:
            if (s.pp7dc8[s.ppIfDepth]) {
                --s.ppIfDepth;
                break;
            }
            s.pp7e80[s.ppIfDepth] = 1;
            Replace(8);
            return 0xd;
        case 70:
            s.pp8064 = s.pp8063;
            ++s.pp8058;
            s.pp8063 = 0;
            break;
        case 71:
            --s.pp8058;
            s.pp8063 = 0;
            if (s.pp8058 == 0 && !s.pp8064) {
                PopCondition();
                return 0xa;
            }
            s.pp8064 = 0;
            break;
        case 72: s.pp8063 = 1; break;
        case 74: s.pp8063 = 0; break;
        case 75: PushCondition(1); break;
        case 79: Output(yytext, yyleng); break;
        case 81: case 82: case 83: case 84: case 85: case 86: case 87: case 88: case 89: case 90: case 91: case 92:
            return 0;
        default: Error("fatal flex scanner internal error--no action found");
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the grammar's actions (0x1002c340)

namespace {

LemonEntry* At(LemonStack* p, int k) { return p->top - k; }
void FreeToken(LemonStack* p, int k)  // a token's text (small values are not pointers)
{
    LemonEntry* const e = At(p, k);
    if (reinterpret_cast<uintptr_t>(e->minor) > 0x100) {
        if (e->minor) Alloc::Delete(e->minor);
        e->minor = nullptr;
    }
}
void FreeText(LemonStack* p, int k)  // a text taken over: freed whatever it holds
{
    LemonEntry* const e = At(p, k);
    if (e->minor) Alloc::Delete(e->minor);
    e->minor = nullptr;
}
float AsFloat(const void* minor)
{
    float f;
    std::memcpy(&f, &minor, sizeof f);
    return f;
}
void* NewNameList()
{
    auto* list = static_cast<GfxNameList*>(Alloc::New(sizeof(GfxNameList)));
    if (list) ::new (list) GfxNameList();
    return list;
}
void DeleteNameList(GfxNameList* list)  // 0x1002ca00
{
    list->Clear();
    if (list) Alloc::Delete(list);
}
void AppendName(void* list, LemonStack* p, int k)  // the name in token k into the list, the token freed
{
    PString n = TextName(static_cast<const char*>(At(p, k)->minor));
    static_cast<GfxNameList*>(list)->Append(n);
    FreeText(p, k);
}

}  // namespace

void PreTraits::Reduce(LemonStack* p, int rule, PreContext* ctx, void*& gotoMinor)
{
    switch (rule) {
    case 1:  // version
        ctx->version = AsFloat(At(p, 0)->minor);
        if (2.0f > ctx->version) ctx->error = true;  // "source version %.2f is older than parser can read (2.0)"
        FreeToken(p, 1);
        break;
    case 2: case 36: FreeToken(p, 4); FreeToken(p, 2); FreeToken(p, 0); break;
    case 3: case 30: case 41: case 43: case 48: FreeToken(p, 0); break;
    case 5: {  // a program name: is it the one asked for?
        String name;
        name.SetANSI(static_cast<const char*>(At(p, 1)->minor), -1);
        if (name == ctx->programName) ctx->found = true;
        FreeText(p, 1);
        FreeToken(p, 2);
        FreeToken(p, 0);
        break;
    }
    case 13: {  // a program block: kept when it is the one asked for
        String name;
        name.SetANSI(static_cast<const char*>(At(p, 3)->minor), -1);
        if (name == ctx->programName) {
            if (!ctx->found) {
                AnsiString marker;
                ctx->found = true;
                ctx->names = static_cast<GfxNameList*>(At(p, 1)->minor);
                if (const char* type = static_cast<const char*>(At(p, 2)->minor)) {
                    marker.Setf("\n#VERTEX %s\n", type);
                    ctx->parser->output.Ins(marker, ctx->parser->output.Length());
                }
            }
            // (a second one: "Found duplicate program specification: %hs"; its names are left to leak)
        } else {
            if (At(p, 1)->minor) DeleteNameList(static_cast<GfxNameList*>(At(p, 1)->minor));
            At(p, 1)->minor = nullptr;
        }
        FreeText(p, 3);
        FreeText(p, 2);
        FreeToken(p, 4);
        FreeToken(p, 0);
        break;
    }
    case 14: gotoMinor = At(p, 1)->minor; FreeToken(p, 2); FreeToken(p, 0); break;
    case 15: gotoMinor = nullptr; break;
    case 16:
        ctx->generation = AsFloat(At(p, 1)->minor);
        FreeToken(p, 3);
        FreeToken(p, 2);
        FreeToken(p, 0);
        break;
    case 17: gotoMinor = At(p, 0)->minor; FreeToken(p, 1); break;
    case 18: gotoMinor = NewNameList(); break;
    case 19: gotoMinor = At(p, 2)->minor; AppendName(gotoMinor, p, 0); FreeToken(p, 1); break;
    case 20: gotoMinor = NewNameList(); AppendName(gotoMinor, p, 0); break;
    case 21: gotoMinor = At(p, 0)->minor; break;
    case 24: FreeToken(p, 5); FreeToken(p, 4); FreeToken(p, 2); FreeToken(p, 0); break;
    case 25: case 27: FreeToken(p, 4); FreeToken(p, 3); FreeToken(p, 1); FreeToken(p, 0); break;
    case 26: FreeToken(p, 6); FreeToken(p, 5); FreeToken(p, 3); FreeToken(p, 0); break;
    case 28: case 38: case 39: case 49: FreeToken(p, 1); break;
    case 31: FreeToken(p, 1); FreeToken(p, 0); break;
    case 35: FreeToken(p, 5); FreeToken(p, 3); FreeToken(p, 0); break;
    case 42: FreeToken(p, 2); FreeToken(p, 0); break;
    case 46: FreeToken(p, 3); FreeToken(p, 1); break;
    case 47: FreeToken(p, 4); FreeToken(p, 2); break;
    case 53: {  // a number
        const float f = static_cast<float>(std::atof(static_cast<const char*>(At(p, 0)->minor)));
        std::memcpy(&gotoMinor, &f, sizeof f);
        FreeText(p, 0);
        break;
    }
    default: break;
    }
}

bool GfxLanguagePreParser::Run(const PString& program, const PString& file, ExtendedGfxLanguage* language)
{
    fileName = file;
    programName = program;
    lang = language;
    PreContext ctx{0.0f, 1.0f, param, language, nullptr, file.Text(), program.Text(), false, false, false, this};
    context = &ctx;
    LemonStack* const p = PreLemon::New();
    value = nullptr;
    int token = Lex();
    while (token > 0) {
        PreLemon::Parse(p, token, value, &ctx);
        value = nullptr;
        if (ctx.error) break;
        token = Lex();
    }
    if (!ctx.error) PreLemon::Parse(p, 0, value, &ctx);
    PreLemon::Free(p);
    bool ok = false;
    if (!ctx.error && language->SupportedByPlatform(ctx.generation) && ctx.found) {
        language->SetGeneration(ctx.generation);
        ok = true;
    }
    if (ctx.names) DeleteNameList(ctx.names);
    return ok;
}

// ---- VertexProgram::Load (0x10049540): the pre-parser stays alive while `then` reads its output
bool GfxPreParse(int api, MemoryFile* in, const PString& program, const PString& file, ExtendedGfxLanguage* lang,
                 bool (*then)(const AnsiString& output, void* ctx), void* ctx)
{
    GfxLanguagePreParser pre(api, in);
    if (!pre.Run(program, file, lang)) return false;
    return then(pre.output, ctx);
}

// ---- entry points for the differential test (found through the linker map; nothing else calls them)

extern "C" void __cdecl RsTestPreCtor(void* mem, int param, MemoryFile* in) { ::new (mem) GfxLanguagePreParser(param, in); }
extern "C" bool __cdecl RsTestPreRun(void* p, const PString* program, const PString* file, ExtendedGfxLanguage* lang)
{
    return static_cast<GfxLanguagePreParser*>(p)->Run(*program, *file, lang);
}
extern "C" const AnsiString* __cdecl RsTestPreOutput(void* p) { return &static_cast<GfxLanguagePreParser*>(p)->output; }
extern "C" void __cdecl RsTestPreDtor(void* p) { static_cast<GfxLanguagePreParser*>(p)->~GfxLanguagePreParser(); }
extern "C" void __cdecl RsTestPreParse(LemonStack* p, int major, void* minor, void* ctx)
{
    PreLemon::Parse(p, major, minor, static_cast<PreContext*>(ctx));
}
extern "C" void __cdecl RsTestPreFree(LemonStack* p) { PreLemon::Free(p); }
#pragma comment(linker, "/include:_RsTestPreCtor")
#pragma comment(linker, "/include:_RsTestPreRun")
#pragma comment(linker, "/include:_RsTestPreOutput")
#pragma comment(linker, "/include:_RsTestPreDtor")
#pragma comment(linker, "/include:_RsTestPreParse")
#pragma comment(linker, "/include:_RsTestPreFree")

}  // namespace Jet
