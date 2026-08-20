// openkal over SBI, with no C library and no board package beneath it.
//
// ⚠️ The entry point is `_start` and not `main`: nothing here supplies a C
// runtime, so there is no crt0 to call one. OpenSBI hands control to the image
// at its load address in supervisor mode with a stack that the linker script
// below establishes.
import openkal.stream;
import openkal.abort;
import openkal.memory;

namespace {

void say(const char* s) {
    kal_uintptr n = 0;
    while (s[n]) ++n;
    kal_stream_write(kal_stdout(), s, n);
}

}  // namespace

extern "C" void kmain() {
    say("hello from openkal over SBI\n");

    void* p = kal_alloc(64, 16);
    say(p ? "heap ok\n" : "heap exhausted\n");
    kal_free(p, 64, 16);

    // Reaches the host as QEMU's exit status, because SRST is a real shutdown
    // rather than a spin.
    kal_exit(p ? 0 : 1);
}

asm(".section .text.entry\n.globl _start\n_start:\n"
    "  la sp, __stack_top\n  call kmain\n1: j 1b\n");
