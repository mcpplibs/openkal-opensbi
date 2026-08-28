// openkal's core interfaces, implemented on the RISC-V Supervisor Binary
// Interface.
//
// WHY THIS BACKEND EXISTS ALONGSIDE A BOARD'S OWN
//
// A board-supplied backend writes to a device address, and that address is a
// board fact: the same binary on a second RISC-V machine writes to something
// that is not a UART, compiles, links, runs, and prints nothing.
//
// SBI has no such property. The console here is a call into firmware that
// already knows the machine, so one binary runs under OpenSBI on QEMU's `virt`
// and on a real board without being rebuilt. ⭐ That makes this the portable
// RISC-V backend and the board's the specific one — and a project picks by
// which property it needs, not by which is better.
//
// WHAT IS IMPLEMENTED
//
// This file: abort, stream and memory — openkal's core set. Beside it,
// time.cpp and env.cpp. An implementation provides an interface in whole or not
// at all, so the absence of fs, process and task is not a deviation: this
// machine has no storage, no second image to start, and no scheduler, and
// clause 6.2 says the remedy for an operation that cannot be provided is that
// its absence be expressed by its absence rather than by a run-time refusal.
//
// ⚠️ `time` USED TO BE ON THAT LIST, WITH A REASON, AND THE REASON WAS WRONG.
//
// It read: SBI can arm a timer interrupt, which is a mechanism for a kernel
// rather than a clock a program can read. The first half is true; the second
// does not follow, because this architecture exposes `rdtime` to the program
// independently of SBI. Measured under OpenSBI on QEMU's `virt` and it
// advances. time.cpp records the measurement and what it cost to not take it.

#include <openkal/abort.h>
#include <openkal/memory.h>
#include <openkal/stream.h>

#include "sbi.h"

namespace {

constexpr kal_uintptr kStdin  = 0;
constexpr kal_uintptr kStdout = 1;
constexpr kal_uintptr kStderr = 2;

// ⚠️ Probed once rather than assumed. DBCN arrived in SBI v2.0, and firmware
// older than that answers `SBI_ERR_NOT_SUPPORTED` — on which every write would
// silently transfer nothing. The legacy extension is one character per trap and
// is deprecated, which is exactly why it is the fallback and not the default.
//
// ⚠️ A tri-state file-scope variable, and NOT a function-local `static`.
//
// A guarded local static compiles to `__cxa_guard_acquire`/`__cxa_guard_release`
// — thread-safe initialisation supplied by the C++ runtime, which a
// freestanding target does not have. Measured: the link fails naming both,
// pointing at this function. `-fno-threadsafe-statics` would also silence it,
// but a flag that has to be remembered is weaker than a construct that cannot
// require the runtime in the first place.
signed char g_dbcn = -1;   // -1 unknown, 0 absent, 1 present

bool dbcn_available() {
    if (g_dbcn < 0) {
        auto r = sbi_call(SBI_EXT_BASE, SBI_BASE_PROBE_EXT, SBI_EXT_DBCN, 0, 0);
        g_dbcn = (r.error == SBI_SUCCESS && r.value != 0) ? 1 : 0;
    }
    return g_dbcn == 1;
}

kal_intptr write_all(const unsigned char* p, kal_uintptr n) {
    if (n == 0) return 0;

    if (dbcn_available()) {
        // The buffer is passed by physical address split into low and high
        // halves. On rv64 with an identity mapping — which is what a program
        // in this arrangement has — the high half is zero.
        kal_uintptr done = 0;
        while (done < n) {
            auto r = sbi_call(SBI_EXT_DBCN, SBI_DBCN_WRITE,
                              n - done, reinterpret_cast<kal_uintptr>(p + done), 0);
            if (r.error != SBI_SUCCESS)
                return done ? static_cast<kal_intptr>(done) : -kal_err_io;
            // A short write is legal here and is why the loop exists; openkal
            // requires the whole buffer or a report, so the retry is the
            // implementation's job rather than every caller's.
            if (r.value <= 0)
                return done ? static_cast<kal_intptr>(done) : -kal_err_io;
            done += static_cast<kal_uintptr>(r.value);
        }
        return static_cast<kal_intptr>(done);
    }

    for (kal_uintptr i = 0; i < n; ++i) {
        auto r = sbi_call(SBI_EXT_LEGACY_PUTCHAR, 0, p[i], 0, 0);
        if (r.error != SBI_SUCCESS)
            return i ? static_cast<kal_intptr>(i) : -kal_err_io;
    }
    return static_cast<kal_intptr>(n);
}

// ── The heap ────────────────────────────────────────────────────────────────
//
// SBI provides no allocator, so this one is a bump allocator over a static
// region. openkal's own criterion admits it: an implementation that can be
// exhausted BOUNDS what is available, whereas one that makes its callers
// silently wrong is a simulation. Exhaustion is a defined outcome — `kal_alloc`
// returns null — and every caller already has to handle it.
//
// ⚠️ `kal_free` therefore does nothing, and that is stated rather than hidden.
// A program whose allocation pattern needs reuse should place a real allocator
// above this one; that is a policy decision, and openkal carries mechanism.
alignas(16) unsigned char g_heap[OPENKAL_OPENSBI_HEAP_BYTES];
kal_uintptr g_used = 0;

}  // namespace

// ⭐ AND THE PROGRAM MAY SAY WHERE INSTEAD, FOR THE SAME REASON IT SAYS WHERE
// THE STACK IS.
//
// The region above is a static array, which means its size is in the image and
// is paid for by every program whether or not it allocates. 64 KiB is a
// reasonable figure for a program that allocates a little; a program that
// carries a C library and a C++ standard library allocates during its own
// initialisation, before `main`, and 64 KiB does not survive it.
//
// ⚠️ AND THE WAY THAT SHOWS IS NOT A DIAGNOSTIC. Measured 2026-08-23: a
// bare-metal `import std;` program linked, started, and printed NOTHING — the
// allocator ran out inside the standard library's static initialisation, before
// any stream existed to report it on. A message would have needed the very
// facility that had not come up yet.
//
// Raising the default would put the cost on every program including the ones
// that do not allocate. So the size is where the stack's size already is: the
// program's linker script. `__heap_start` and `__heap_end` are weak, so a
// script that does not define them leaves the static region in use and nothing
// changes for a program that was working.
extern "C" {
[[gnu::weak]] extern unsigned char __heap_start[];
[[gnu::weak]] extern unsigned char __heap_end[];
}

namespace {

// Which region is in use, decided once. A script that defines the pair and gets
// the order wrong is ignored rather than trusted: an end below a start would
// otherwise make every allocation appear to succeed into memory that is not
// the heap, which is the failure this whole package is written to avoid.
struct region { unsigned char* base; kal_uintptr size; };

region heap_region() {
    // ⚠️ `+` on each: these are arrays, and comparing two arrays directly is
    // deprecated in this dialect because it compares addresses while reading
    // like a comparison of contents. Decaying them says which was meant.
    if (+__heap_start != nullptr && +__heap_end > +__heap_start)
        return { __heap_start, (kal_uintptr)(__heap_end - __heap_start) };
    return { g_heap, sizeof g_heap };
}

}  // namespace

extern "C" {

// ── openkal.abort ───────────────────────────────────────────────────────────
KAL_NORETURN void kal_abort(const char* msg, kal_uintptr len) {
    if (msg && len) write_all(reinterpret_cast<const unsigned char*>(msg), len);
    const unsigned char nl = '\n';
    write_all(&nl, 1);
    sbi_call(SBI_EXT_SRST, SBI_SRST_RESET,
             SBI_SRST_TYPE_SHUTDOWN, SBI_SRST_REASON_FAILURE, 0);
    // Reached only if the firmware declined to reset, which no conforming
    // implementation does. Spinning is the only remaining honest behaviour.
    for (;;) {}
}

KAL_NORETURN void kal_exit(int code) {
    sbi_call(SBI_EXT_SRST, SBI_SRST_RESET, SBI_SRST_TYPE_SHUTDOWN,
             code == 0 ? SBI_SRST_REASON_NONE : SBI_SRST_REASON_FAILURE, 0);
    for (;;) {}
}

// ── openkal.stream ──────────────────────────────────────────────────────────
kal_stream kal_stdin (void) { return kal_stream{kStdin};  }
kal_stream kal_stdout(void) { return kal_stream{kStdout}; }
kal_stream kal_stderr(void) { return kal_stream{kStderr}; }

kal_intptr kal_stream_write(kal_stream s, const void* buf, kal_uintptr n) {
    if (s.h != kStdout && s.h != kStderr) return -kal_err_invalid;
    // ⚠️ Both streams reach the same console. SBI has one, and reporting two
    // that are secretly one would be a claim the firmware cannot honour.
    return write_all(static_cast<const unsigned char*>(buf), n);
}

kal_intptr kal_stream_read(kal_stream s, void* buf, kal_uintptr n) {
    if (s.h != kStdin) return -kal_err_invalid;
    if (n == 0) return 0;
    auto* out = static_cast<unsigned char*>(buf);
    if (dbcn_available()) {
        auto r = sbi_call(SBI_EXT_DBCN, SBI_DBCN_READ,
                          n, reinterpret_cast<kal_uintptr>(out), 0);
        if (r.error != SBI_SUCCESS) return -kal_err_io;
        return static_cast<kal_intptr>(r.value);
    }
    auto r = sbi_call(SBI_EXT_LEGACY_GETCHAR, 0, 0, 0, 0);
    // The legacy extension reports "nothing available" as a negative value,
    // which is end of input as far as a reader is concerned.
    if (r.error < 0) return 0;
    out[0] = static_cast<unsigned char>(r.error);
    return 1;
}

// The firmware console is not buffered by this implementation, so there is
// nothing to commit. Reporting success is accurate rather than a stub: the
// bytes were already handed to the firmware when the write returned.
int kal_stream_flush(kal_stream s) {
    if (s.h != kStdout && s.h != kStderr && s.h != kStdin) return kal_err_invalid;
    return kal_ok;
}

kal_uintptr kal_stream_props(kal_stream s) {
    if (s.h != kStdin && s.h != kStdout && s.h != kStderr) return 0;
    // The SBI console is a debug console. A C library above this backend must
    // not defer a prompt until a buffer fills.
    return KAL_STREAM_PROP_INTERACTIVE;
}

// ── openkal.memory ──────────────────────────────────────────────────────────
// The quantum this environment allocates and protects memory in.
//
// ONE, AND THAT IS AN ANSWER RATHER THAN AN ABSENCE. This machine has no
// memory management unit in this arrangement: memory is a fixed region and the
// allocator hands out any alignment a caller asks for, so every address and
// every length is acceptable. A caller that rounds to one is correct, which is
// what the operation promises. Reporting a page size the firmware does not
// enforce would be reporting a fact about some other machine.
kal_uintptr kal_memory_granularity(void) { return 1; }

void* kal_alloc(kal_uintptr size, kal_uintptr align) {
    if (size == 0) size = 1;
    if (align == 0) align = 1;
    const region r = heap_region();
    const kal_uintptr base = reinterpret_cast<kal_uintptr>(r.base);
    kal_uintptr p = (base + g_used + align - 1) & ~(align - 1);
    const kal_uintptr end = p + size;
    if (end > base + r.size) return nullptr;          // exhaustion, reported
    g_used = end - base;
    return reinterpret_cast<void*>(p);
}

// Nothing is reclaimed. See the note on the heap above: this is a stated bound
// rather than an omission.
void kal_free(void*, kal_uintptr, kal_uintptr) {}

// ── The interfaces this machine does not have are absent, and stay absent ───
//
// ⚠️ NO `kal_fs_props`, NO `kal_task_props`, AND THAT IS THE MECHANISM RATHER
// THAN AN OMISSION.
//
// 0.1.3 defined both as zero, because a capability-querying program failed to
// link against this backend:
//
//     ld.lld: error: undefined symbol: kal_fs_props
//     >>> referenced by fs.cppm:136
//     >>>               obj/main.o:(kal::fs::properties@openkal.fs())
//
// That error is clause 6.1 working. "An interface that an implementation does
// not provide is absent as a link-time definition, and a consumer that uses it
// fails to link." Clause 6.2's table is explicit about which question each
// mechanism answers: the LINKER answers "was an interface used that the
// implementation does not provide", and a capability word answers "how does
// this implementation behave WITHIN AN INTERFACE IT PROVIDES".
//
// Defining the word for an interface with no operations answers the second
// question about something that cannot be asked the first. The program then
// proceeds past the point the linker existed to stop it at, and reaches
// `kal_fs_open` — undefined — or worse, believes it has a filesystem with no
// capabilities. SURFACE.txt says the same thing in the other direction: an
// implementation "exports the names of the interfaces it provides and exports
// no other name beginning with kal_".
//
// 0.1.3 is retracted. A program that asks this backend about a filesystem is a
// program that should not build against this backend, and the graph is where
// that is decided.
}  // extern "C"
