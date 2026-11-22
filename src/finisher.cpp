// THE STATUS LEAVES THE MACHINE THROUGH THE DEVICE THE MACHINE STATES FOR IT (0.10.1).
//
// SBI's system reset carries a reason, none or failure, and not a number: a program that ended with
// 7 and one that ended with 0 both shut the machine down the same way, and an emulator running a
// test reported 0 for both (measured: QEMU's `virt' under OpenSBI 1.5, status 0 for a program that
// returned 7). A machine that has a device to end with a status describes it in the device tree the
// firmware hands the program in a1: a node compatible with "sifive,test1" or "sifive,test0", the
// test finisher, whose register ends the machine with (status << 16) | 0x3333, or passes with
// 0x5555. Where the tree states one, the status is written there; where it states none, the
// reset's reason is what remains, and a machine without the device is not written to.
#include "finisher.h"

namespace {

using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;

volatile u32* g_finisher = nullptr;

u32 be32(const u8* p) {
    return static_cast<u32>(p[0]) << 24 | static_cast<u32>(p[1]) << 16 | static_cast<u32>(p[2]) << 8 | p[3];
}

bool same(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

// Whether a `compatible' value, a list of terminated strings, names the test finisher.
bool finisher_compatible(const u8* v, u32 len) {
    for (u32 at = 0; at < len;) {
        const char* s = reinterpret_cast<const char*>(v + at);
        if (same(s, "sifive,test1") || same(s, "sifive,test0")) return true;
        while (at < len && v[at]) ++at;
        ++at;
    }
    return false;
}

constexpr u32 kBeginNode = 1, kEndNode = 2, kProp = 3, kNop = 4;
constexpr u32 kMaxDepth = 16;

}  // namespace

void okbsbi_find_finisher(const void* fdt) {
    const auto* h = static_cast<const u8*>(fdt);
    if (!h || be32(h) != 0xd00dfeedu) return;
    const u8* structure = h + be32(h + 8);
    const char* strings = reinterpret_cast<const char*>(h + be32(h + 12));
    const u32 size = be32(h + 36);
    // The address cells of each node's parent, and what the node at each depth has stated.
    u32 cells[kMaxDepth] = {2};
    bool compatible[kMaxDepth] = {};
    u64 reg[kMaxDepth] = {};
    u32 depth = 0;
    for (u32 at = 0; at + 4 <= size;) {
        const u32 token = be32(structure + at);
        at += 4;
        if (token == kBeginNode) {
            while (structure[at]) ++at;
            at = (at + 4) & ~3u;
            if (++depth >= kMaxDepth) return;
            cells[depth] = 2;
            compatible[depth] = false;
            reg[depth] = 0;
        } else if (token == kProp) {
            const u32 len = be32(structure + at), name = be32(structure + at + 4);
            const u8* value = structure + at + 8;
            at = (at + 8 + len + 3) & ~3u;
            const char* n = strings + name;
            if (same(n, "#address-cells") && len == 4) cells[depth] = be32(value);
            else if (same(n, "compatible")) compatible[depth] = finisher_compatible(value, len);
            else if (same(n, "reg") && depth > 0 && len >= 4 * cells[depth - 1])
                reg[depth] = cells[depth - 1] == 2 ? (static_cast<u64>(be32(value)) << 32 | be32(value + 4))
                                                   : be32(value);
        } else if (token == kEndNode) {
            if (depth > 0 && compatible[depth] && reg[depth]) {
                g_finisher = reinterpret_cast<volatile u32*>(static_cast<unsigned long>(reg[depth]));
                return;
            }
            if (depth == 0) return;
            --depth;
        } else if (token != kNop) {
            return;   // the end of the structure, or a token this reader does not know
        }
    }
}

void okbsbi_finish(int status) {
    if (!g_finisher) return;
    const u32 code = static_cast<u32>(status) & 0xff;
    *g_finisher = code == 0 ? 0x5555u : (code << 16) | 0x3333u;
}
