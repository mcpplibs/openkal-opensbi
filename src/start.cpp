// Program startup, for a program that carries no runtime of its own.
//
// The same object openkal-linux supplies, for an environment that supplies far
// less. It belongs to the implementation for the same reason: every step is a
// fact about what hands control over, and a consumer that contained these steps
// would contain a copy of them per environment.
//
// ⚠️ WHAT FIRMWARE HANDS OVER, AND WHAT IT DOES NOT.
//
// A kernel starts a program with arguments on the stack, a thread pointer to
// establish, program headers to report, and a stack already there. Firmware
// starts an IMAGE: it jumps to the load address in supervisor mode with a hart
// identifier in a0 and a device tree in a1, and nothing else. There is no
// argument vector to find, because there is no one to have supplied one; and
// there is no stack, because a stack is a region of the image's own memory and
// only the image knows where it put one.
//
// So this file does two things the other does not have to: it makes the stack
// exist, and it stops. And it does not do the thing the other spends most of
// its length on, because there is nothing to read.
//
// ⚠️ THE STACK COMES FROM THE PROGRAM'S LINKER SCRIPT AND CANNOT COME FROM HERE.
//
// `__stack_top' is defined by the linker script the program supplies, because
// where the stack goes is a statement about the image's layout and the image is
// the program's. This package names the symbol and does not define it; a
// program that links this object without a script that defines it is told so by
// the linker, which is the right report.
//
// Control is handed on through `__libc_start_main', weakly, exactly as the
// other does: a program written directly against openkal has no such symbol,
// and then this file runs the initialisers and calls `main' itself.
#ifdef OPENKAL_OPENSBI_STANDALONE

#include <openkal/abort.h>
#include <openkal/memory.h>

extern "C" {

int main(int, char**, char**);

[[gnu::weak]] int __libc_start_main(int (*)(int, char**, char**), int, char**,
                                    void (*)(), void (*)(), void (*)());

[[noreturn]] void __okb_start_c(void);

}

using initialiser = void (*)(int, char**, char**);

// ⚠️ NOT in an anonymous namespace, and that is the compiler's rule rather than
// a preference: internal linkage and a weak declaration are contradictory, and
// clang says so in as many words. The linker script defines these, and a
// program built without one gets the null range the weak declaration is for.
[[gnu::weak]] extern initialiser __preinit_array_start[];
[[gnu::weak]] extern initialiser __preinit_array_end[];
[[gnu::weak]] extern initialiser __init_array_start[];
[[gnu::weak]] extern initialiser __init_array_end[];

namespace {

void run_initialisers() {
    // ⚠️ Only when no C library took the hand-over. One that did runs these
    // itself, and running them twice constructs every static object twice.
    static char* nothing = nullptr;
    for (initialiser* p = __preinit_array_start; p != __preinit_array_end; ++p)
        (*p)(0, &nothing, &nothing);
    for (initialiser* p = __init_array_start; p != __init_array_end; ++p)
        (*p)(0, &nothing, &nothing);
}

}  // namespace

// ⭐⭐ THE THREAD POINTER, WHICH IS THE OTHER THING A KERNEL WOULD HAVE DONE.
//
// openkal-linux's start object establishes it too, and for the same reason: the
// register that names the current context's thread-local storage is set by
// whoever creates the context, and where a program carries no loader, that is
// the implementation.
//
// ⚠️ WHAT IT LOOKS LIKE WHEN IT IS MISSING IS NOT "NO THREAD-LOCAL STORAGE".
//
// Measured 2026-08-23. A bare-metal `import std;` program started, printed, and
// faulted at the first `throw`:
//
//     fault_load  epc = __cxa_throw  tval = 0x80048008
//     cxa_exception.cpp:284  globals->uncaughtExceptions += 1
//
// `__cxa_get_globals` reads a thread_local, `tp` held whatever it held at
// reset, and the load went somewhere in the firmware's own memory. Nothing
// about the message says "thread pointer": it names an exception function and
// an address, and both look like memory corruption.
//
// ⚠️ AND THE C LIBRARY DOES NOT COVER THIS. openkal-musl keeps ITS OWN thread
// pointer in a variable rather than in the register --- that is what lets it
// run where the register means nothing --- so musl's startup succeeding says
// nothing about whether the TOOLCHAIN's thread-locals work. Two mechanisms,
// and only one of them was established.
// ⚠️ FROM THE LINKER SCRIPT, NOT FROM THE PROGRAM HEADERS.
//
// The obvious source is PT_TLS, reached through `__ehdr_start`. That requires
// the ELF header to lie inside a loaded segment, which requires it to be at the
// lowest loaded address --- and firmware jumps to the lowest loaded address, so
// the header would be executed. Measured: the machine hangs on `\x7fELF`.
//
// So the script states the three measurements, exactly as it already states
// where the stack and the heap are, and for the same reason: they are facts
// about the image's layout and the image is the program's. A program whose
// script does not define them gets no thread-local storage established, which
// is correct for one whose toolchain put its thread-locals somewhere else.
extern "C" {
[[gnu::weak]] extern unsigned char __tls_start[];
[[gnu::weak]] extern unsigned char __tls_filesz[];
[[gnu::weak]] extern unsigned char __tls_memsz[];
}

namespace {

kal_uintptr round_up(kal_uintptr n, kal_uintptr to) { return (n + to - 1) & ~(to - 1); }

// Variant I with no gap: on this architecture `tp` addresses the first byte of
// the block and every offset the linker computed is positive from there. That
// is the psABI's statement, and musl's own header for this architecture repeats
// it as `TLS_ABOVE_TP` with `GAP_ABOVE_TP 0` --- which is where this was taken
// from rather than from memory.
void establish_thread_pointer() {
    // ⚠️ These symbols carry their VALUE in their ADDRESS. A linker script
    // assignment defines an absolute symbol; there is no object to load from,
    // and reading one as if there were gives whatever lies at that address.
    const auto filesz = reinterpret_cast<kal_uintptr>(__tls_filesz);
    const auto memsz  = reinterpret_cast<kal_uintptr>(__tls_memsz);
    if (memsz == 0) return;

    // 16 rather than the segment's own alignment, which the script does not
    // state: over-aligning a block is always safe, and every offset the linker
    // computed stays valid because they are measured from the block's start.
    const kal_uintptr align = 16;
    // The extra room is for what a C library places beside the block. musl puts
    // its own descriptor below `tp`; this allocation is never returned, so being
    // generous costs nothing that is later wanted.
    const kal_uintptr bytes = round_up(memsz, align) + 128;
    auto* p = static_cast<unsigned char*>(kal_alloc(bytes, align));
    if (p == nullptr) {
        static const char m[] =
            "openkal-opensbi: no memory for this context's thread-local storage";
        kal_abort(m, sizeof m - 1);
    }
    for (kal_uintptr i = 0; i < bytes; ++i) p[i] = 0;
    for (kal_uintptr i = 0; i < filesz; ++i) p[i] = __tls_start[i];

    __asm__ __volatile__("mv tp, %0" :: "r"(p));
}

}  // namespace

// ⭐ WHAT crtbegin WOULD HAVE SUPPLIED, AND WHY ITS ABSENCE IS A LINK ERROR
// ABOUT A RELOCATION RANGE RATHER THAN ABOUT A MISSING NAME.
//
// Every static object with a destructor registers it through `__cxa_atexit`,
// and the third argument identifies the image the object belongs to. In an
// ordinary link that identifier comes from crtbegin. There is no crtbegin here,
// so the linker synthesises one — at address zero, because it has no better
// idea — and the image is at 0x80200000. The distance between them is 2.05 GiB,
// which is outside what `auipc` can reach even under `-mcmodel=medany`:
//
//     relocation R_RISCV_PCREL_HI20 out of range: -525086
//     references '__dso_handle'
//
// ⚠️ A reader who has not seen this before will read that as a code-model
// problem, and the code model is already the widest one. The problem is that
// the symbol is not in the image. Defining it here puts it in the image, and
// the value is its own address because that is what identifies an image
// uniquely and is what an ordinary link produces.
extern "C" { void* __dso_handle = &__dso_handle; }

// The stack, then C. `la` of a linker-defined symbol resolves at link time, so
// the sequence needs nothing to have been set up before it runs --- which is
// the situation it is in.
//
// ⚠️ In `.text.entry` rather than `.text`, because the linker script places
// that section first. Firmware jumps to the load address, not to `_start`: the
// entry recorded in the image header is not read by firmware that loads a raw
// image, so the first instruction at the load address has to BE this one.
asm(".section .text.entry\n"
    ".globl _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "  la sp, __stack_top\n"
    "  call __okb_start_c\n"
    "1: j 1b\n"
    ".size _start,.-_start\n");

extern "C" [[noreturn]] void __okb_start_c(void) {
    // Zero and null rather than a vector: openkal.env is what a program above
    // this reads its arguments through, and this environment's answer there is
    // that there are none. Passing the same answer here keeps the two from
    // disagreeing.
    static char* nothing = nullptr;

    // Before anything with thread-local state runs, which includes the C
    // library's own initialisation.
    establish_thread_pointer();

    if (__libc_start_main != nullptr) {
        __libc_start_main(main, 0, &nothing, nullptr, nullptr, nullptr);
        // A C library's hand-over does not return. Reaching here means one did,
        // and continuing would run the program a second time.
        kal_exit(127);
    }

    run_initialisers();
    kal_exit(main(0, &nothing, &nothing));
}

#endif  // OPENKAL_OPENSBI_STANDALONE
