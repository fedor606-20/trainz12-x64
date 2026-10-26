// Unit U2b2 of RenderSystem_7c: the machinery shared by the four shader-language front ends (notes/RenderSystem.md):
// a flex 2.5.4 C++ scanner skeleton with REJECT (FlexScanner, 0x1002e680.. and 0x10045f70..0x10046240 shared by all
// four), an old-style Lemon parser engine (LemonParser<Traits>, one copy per language in the original) and the
// chunked list of names the parsers fill (0x1002cd80 / 0x1002ce20). All of it is internal to the module.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/PString.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jet {

// a circular list of chunks {next, prev, count, 13 names} (0x40 bytes each); the head is the first chunk
struct GfxNameList {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        int count;
        PStringCache::Node* names[13];
    };
    Chunk* head = nullptr;

    void Append(const PString& name);  // 0x1002cd80: a reference to the name at the end
    void Clear();                      // 0x1002ce20: the second chunk first, the head last
};

// the same chunks holding ints, used as a stack (0x1000d530 push, 0x1002c1a0 pop)
struct GfxIntStack {
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        int count;
        int items[13];
    };
    Chunk* head = nullptr;

    void Push(int v);
    int Pop();     // from the last chunk, which goes when it empties (an empty stack would read address 0)
    void Clear();  // the second chunk first, the head last
};

// The rule actions' state: module statics in the original (0x100b7dc8..0x100b8127), shared by every scanner
// instance and kept between runs. Laid out the same way, so an overrun lands in the same neighbours. Names follow
// the addresses; what each flag does is in the actions.
struct GfxLexStatics {
    unsigned char pp7dc8[100];  // 0x100b7dc8 per #if level
    unsigned char pp7e2c;       // 0x100b7e2c
    unsigned char pad7e2d[3];
    int ppStack[20];            // 0x100b7e30 the pre-parser's start conditions
    unsigned char pp7e80[104];  // 0x100b7e80 per #if level
    unsigned char pp7ee8[100];  // 0x100b7ee8 per #if level
    int unknown7f4c;            // 0x100b7f4c
    int qkStack[20];            // 0x100b7f50 the quick parser's start conditions
    int tpStack[20];            // 0x100b7fa0 the texture program scanner's (one rule reads the entry below the top,
                                // so an empty stack reads qkStack[19])
    int vpStack[20];            // 0x100b7ff0 the vertex program scanner's (the same, reading tpStack[19])
    unsigned char unknown8040[3];
    unsigned char pp8043;       // 0x100b8043
    unsigned char unknown8044[12];
    int ppDepth;                // 0x100b8050
    int ppCurrent;              // 0x100b8054
    int pp8058;                 // 0x100b8058 code/native block depth
    int pp805c;                 // 0x100b805c brace depth
    unsigned char pp8060, pp8061, pp8062, pp8063, pp8064;
    unsigned char pad8065[2];
    unsigned char tpNative;     // 0x100b8067 the API section is native (its text is collected whole)
    int ppIfDepth;              // 0x100b8068
    int qkDepth;                // 0x100b806c
    int qkCurrent;              // 0x100b8070
    int qkBraces;               // 0x100b8074
    unsigned char unknown8078[0x18];
    int tpDepth;                // 0x100b8090
    int tpCurrent;              // 0x100b8094
    int tpBraces;               // 0x100b8098 brace depth inside an API section
    int tpBlock;                // 0x100b809c brace depth of a code or native block
    unsigned char unknown80a0[3];
    unsigned char vpFlag;       // 0x100b80a3 inside a block that counts its markers
    unsigned char unknown80a4[0x70];
    int vpDepth;                // 0x100b8114
    int vpCurrent;              // 0x100b8118
    int vpCount;                // 0x100b811c markers seen in the block
    char* vpBuffer;             // 0x100b8120 the code block's text (Jet::Alloc, the file's size)
    char* vpWrite;              // 0x100b8124 where the next text goes
};
JET_LAYOUT32(offsetof(GfxLexStatics, ppStack) == 0x68 && offsetof(GfxLexStatics, qkStack) == 0x188 &&
                  offsetof(GfxLexStatics, tpStack) == 0x1d8 && offsetof(GfxLexStatics, pp8043) == 0x27b &&
                  offsetof(GfxLexStatics, ppDepth) == 0x288 && offsetof(GfxLexStatics, ppIfDepth) == 0x2a0 &&
                  offsetof(GfxLexStatics, tpNative) == 0x29f && offsetof(GfxLexStatics, tpDepth) == 0x2c8 &&
                  offsetof(GfxLexStatics, vpStack) == 0x228 && offsetof(GfxLexStatics, vpFlag) == 0x2db &&
                  offsetof(GfxLexStatics, vpDepth) == 0x34c && sizeof(GfxLexStatics) == 0x360);
extern GfxLexStatics gfxLex;

// ---------------------------------------------------------------------------------------------------------------
// flex

struct FlexTables {
    const unsigned short* accList;
    const unsigned short* accept;
    const unsigned char* ec;
    const unsigned int* meta;
    const unsigned short* base;
    const unsigned short* def;
    const unsigned short* nxt;
    const unsigned short* chk;
    int jamState;
    int endOfBufferRule;   // YY_END_OF_BUFFER; the end-of-file rules follow, one per start condition
    bool highFromEc;       // yy_get_previous_state reads chars >= 0x7f as ec[0x7e] (else as class 1)
};

// The scanner state of yyFlexLexer (0x78 bytes in the original). Scan() runs the DFA and hands back the action
// of the next match (yytext/yyleng set, line counting done); the end-of-buffer action is handled inside.
class FlexScanner {
public:
    FlexScanner(const FlexTables& tables, MemoryFile* in);
    virtual ~FlexScanner();

protected:
    struct Buffer {  // yy_buffer_state (0x28)
        MemoryFile* input;
        char* chars;
        char* pos;
        int size;
        int count;
        int ours;
        int interactive;
        int atBol;
        int fill;
        int status;  // 0 new, 1 normal, 2 EOF pending
    };

    int Scan();
    void Unput(int c, char* bp);
    void Output(const char* buf, int size);  // ECHO (nobody sets yyout)
    [[noreturn]] static void Error(const char* message);
    char* CopyText(const char* s) const;  // strdup through Jet::Alloc (null stays null)

    char* yytext = nullptr;  // +4
    int yyleng = 0;          // +8
    int yylineno = 1;        // +0xc
    int start = 0;           // +0x3c

private:
    int Next(int state, int cls) const;
    int Input(char* buf, int max);
    Buffer* CreateBuffer(MemoryFile* in, int size);
    void InitBuffer(Buffer* b, MemoryFile* in);
    void LoadBufferState();
    void Restart(MemoryFile* in);
    int GetNextBuffer();
    int PreviousState();
    int TryNulTransition(int state);

    const FlexTables& t;
    MemoryFile* yyin;               // +0x20
    MemoryFile* yyout = nullptr;    // +0x24
    Buffer* current = nullptr;      // +0x28
    char holdChar = 0;              // +0x2c
    int nChars = 0;                 // +0x30
    char* cBufP = nullptr;          // +0x34
    int init = 1;                   // +0x38
    int switchOnEof = 0;            // +0x40
    int* stateBuf;                  // +0x4c (Jet::Alloc, 0x10008)
    int* statePtr = nullptr;        // +0x50
    int lp = 0;                     // +0x60
};

// ---------------------------------------------------------------------------------------------------------------
// Lemon (two generations: the older keeps one pointer per value and byte actions; the vertex program parser's
// keeps 16-byte values and int actions)

struct LemonAction {
    unsigned char lookahead;
    unsigned short action;
    short next;  // index in the pool, -1 at the end of the chain
};
struct LemonState {
    short block;  // first slot in the pool, -1 none
    unsigned mask;
    unsigned short defaultAction;
};
struct LemonRule {
    unsigned char lhs, nrhs;
};

// the newer generation's value: a pointer (a token's text) and three more words
struct LemonMinor16 {
    void* p;
    int w[3];
};

template <class M>
struct LemonEntryT {
    int state;
    int major;
    M minor;
};
// the parser's stack (Jet::Alloc): index of the top, error countdown, top, 100 entries (0x4bc / 0x96c)
template <class M>
struct LemonStackT {
    int index;
    int errorCount;
    LemonEntryT<M>* top;
    LemonEntryT<M> entries[100];
};
using LemonEntry = LemonEntryT<void*>;
using LemonStack = LemonStackT<void*>;
JET_LAYOUT32(sizeof(LemonStack) == 0x4bc && sizeof(LemonStackT<LemonMinor16>) == 0x96c);

template <class T, class = void>
struct LemonMinorOf {
    using type = void*;
};
template <class T>
struct LemonMinorOf<T, std::void_t<typename T::Minor>> {
    using type = typename T::Minor;
};

// Traits: kNoCode, kStates, kRules, kErrorSymbol, kTerminals, pool[], states[], rules[], Reduce(Stack*, int rule,
// Ctx*, Minor& gotoMinor), Failed(Ctx*) and SyntaxError(Ctx*); Minor (void* when not given). The text a terminal of
// majors 1..kTerminals carries is freed: a void* above 0x100, a LemonMinor16's pointer whenever set. Actions: below
// kStates shift, then the rules, then error, accept, and (lookups only) none.
template <class Traits, class Ctx>
struct LemonParser {
    using Minor = typename LemonMinorOf<Traits>::type;
    using Entry = LemonEntryT<Minor>;
    using Stack = LemonStackT<Minor>;
    static constexpr int kError = Traits::kStates + Traits::kRules;
    static constexpr int kAccept = kError + 1;
    static constexpr int kNone = kError + 2;

    static void* Text(const Minor& m)  // the token text a value owns, or null
    {
        if constexpr (std::is_same_v<Minor, void*>)
            return reinterpret_cast<uintptr_t>(m) > 0x100 ? m : nullptr;
        else
            return m.p;
    }
    static void ClearText(Minor& m)
    {
        if constexpr (std::is_same_v<Minor, void*>)
            m = nullptr;
        else
            m.p = nullptr;
    }

    static Stack* New()
    {
        auto* p = static_cast<Stack*>(Alloc::New(sizeof(Stack)));
        if (p) p->index = -1;
        return p;
    }
    static void Destroy(Entry* e)
    {
        if (static_cast<unsigned char>(e->major - 1) < Traits::kTerminals && Text(e->minor)) {
            Alloc::Delete(Text(e->minor));
            ClearText(e->minor);
        } else if (static_cast<unsigned char>(e->major - 1) < Traits::kTerminals && !std::is_same_v<Minor, void*>) {
            ClearText(e->minor);
        }
    }
    static void PopAll(Stack* p)
    {
        while (p->index >= 0) {
            Destroy(p->top);
            --p->index;
            --p->top;
        }
    }
    static void Free(Stack* p)  // the stack and what is left on it
    {
        if (!p) return;
        PopAll(p);
        Alloc::Delete(p);
    }
    static void Push(Stack* p, int state, int major, const Minor* minor)
    {
        ++p->index;
        ++p->top;
        if (p->index >= 100) {  // overflow: everything is dropped
            --p->index;
            --p->top;
            PopAll(p);
            return;
        }
        p->top->state = state;
        p->top->major = major;
        p->top->minor = *minor;
    }
    // the action of `state` on `la`; false for the "no code" lookahead on a state that does not end with it
    static bool Find(int state, int la, int& action)
    {
        const LemonState& s = Traits::states[state];
        if (la == Traits::kNoCode) {
            // (a state without actions would make the original read address 0)
            if (s.mask != 0 || s.block < 0 || Traits::pool[s.block].lookahead != Traits::kNoCode) return false;
        } else if (s.block >= 0) {
            for (int e = s.block + static_cast<int>(s.mask & static_cast<unsigned>(la)); e >= 0;
                 e = Traits::pool[e].next)
                if (Traits::pool[e].lookahead == la) {
                    action = Traits::pool[e].action;
                    return true;
                }
        }
        action = s.defaultAction;
        return true;
    }
    static int FindGoto(Stack* p, int la)
    {
        int action;
        return Find(p->top->state, la, action) ? action : kNone;
    }
    static void Reduce(Stack* p, int rule, Ctx* ctx)
    {
        Minor gotoMinor;
        if constexpr (std::is_same_v<Minor, void*>) {
            gotoMinor = p;  // left as it is by rules without a value (the original's stack slot)
            Traits::Reduce(p, rule, ctx, gotoMinor);
        } else {
            // the original's is a stack slot nothing initialises: rules that set part of it or nothing leave the
            // previous reduction's value of the same token there; the scanner's frame overwrites it between
            // tokens (with what cannot be reproduced: zero here)
            gotoMinor = Carry();
            Traits::Reduce(p, rule, ctx, gotoMinor);
            Carry() = gotoMinor;
        }
        const int lhs = Traits::rules[rule].lhs;
        const int n = Traits::rules[rule].nrhs;
        p->index -= n;
        p->top -= n;
        const int action = FindGoto(p, lhs);
        if (action < Traits::kStates)
            Push(p, action, lhs, &gotoMinor);
        else if (action == kAccept)
            PopAll(p);
    }
    static Minor& Carry()
    {
        static Minor value{};
        return value;
    }
    static void Parse(Stack* p, int major, Minor minor, Ctx* ctx)
    {
        if constexpr (!std::is_same_v<Minor, void*>) Carry() = Minor{};
        int errorHit = 0;
        if (p->index < 0) {
            if (major == 0) return;
            p->top = p->entries;
            p->index = 0;
            p->errorCount = -1;
            p->top->state = 0;
            p->top->major = 0;
        }
        const bool endOfInput = major == 0;
        Minor value = minor;
        do {
            int action;
            if (!Find(p->top->state, major, action)) {
                PopAll(p);
                return;
            }
            if (action < Traits::kStates) {
                Push(p, action, major, &value);
                --p->errorCount;
                if (!endOfInput || p->index < 0) return;
                major = 0;
            } else if (action < kError) {
                Reduce(p, action - Traits::kStates, ctx);
                if (major == Traits::kNoCode) return;
            } else if (action == kError) {
                if (p->errorCount < 0) Traits::SyntaxError(ctx);  // "Syntax error at %s." (formatted, never shown)
                if (p->top->major == Traits::kErrorSymbol || errorHit) {
                    DropToken(major, value);
                    major = Traits::kNoCode;
                } else {
                    int recover = kError;
                    while (p->index >= 0) {
                        Entry* e = p->top;
                        if (e->major == Traits::kErrorSymbol) break;
                        recover = FindGoto(p, Traits::kErrorSymbol);
                        if (recover < Traits::kStates) break;
                        Destroy(e);
                        --p->index;
                        --p->top;
                    }
                    if (p->index < 0 || major == 0) {
                        DropToken(major, value);
                        PopAll(p);
                        Traits::Failed(ctx);
                        major = Traits::kNoCode;
                    } else if (p->top->major != Traits::kErrorSymbol) {
                        Minor none{};
                        Push(p, recover, Traits::kErrorSymbol, &none);
                    }
                }
                p->errorCount = 3;
                errorHit = 1;
                if (major == Traits::kNoCode) return;
            } else {
                PopAll(p);
                return;
            }
        } while (p->index >= 0);
    }

private:
    static void DropToken(int major, Minor& value)
    {
        const unsigned char m = static_cast<unsigned char>(major);
        if (m != 0 && m <= Traits::kTerminals) {
            if (void* const t = Text(value)) Alloc::Delete(t);
            if (Text(value) || !std::is_same_v<Minor, void*>) ClearText(value);
        }
    }
};

class ExtendedGfxLanguage;

// the parsers' entry points for unit U3 (their classes are local to their files)
bool GfxPreParse(int api, MemoryFile* in, const PString& program, const PString& file, ExtendedGfxLanguage* lang,
                 bool (*then)(const AnsiString& output, void* ctx), void* ctx);
bool VertexProgramParse(MemoryFile* in, const PString& program, const PString& file, ExtendedGfxLanguage* lang);
bool TextureProgramParse(MemoryFile* in, int api, const PString& file, ExtendedGfxLanguage* lang);

}  // namespace Jet
