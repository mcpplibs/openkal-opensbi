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
