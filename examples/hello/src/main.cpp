// openkal over SBI, with no C library and no board package beneath it.
//
// ⚠️ The entry point is `_start` and not `main`: nothing here supplies a C
// runtime, so there is no crt0 to call one. OpenSBI hands control to the image
// at its load address in supervisor mode with a stack that the linker script
// below establishes.
import openkal.stream;
import openkal.abort;
import openkal.memory;
import openkal.time;
import openkal.env;

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

    // ⚠️ THE ASSERTION IS THAT IT MOVES, NOT THAT IT READS.
    //
    // A clock that returns a constant reads perfectly well and is worthless,
    // and it is the exact failure the comment this interface replaced was
    // afraid of. So the sleep is between two readings and the second must
    // exceed the first: a stuck counter fails here, and only here.
    //
    // The figure is a millisecond because it has to be long enough to exceed
    // one tick of a granularity this package does not fix, and short enough
    // that a spin of it does not matter to anyone.
    const kal_duration t0 = kal_time_monotonic();
    kal_time_sleep(1000000);
    const kal_duration t1 = kal_time_monotonic();
    say(t1 > t0 ? "clock ok\n" : "clock stuck\n");

    // Empty, and that is the answer rather than a refusal — src/env.cpp records
    // why the two are different. Asserted because "returns zero" and "was never
    // linked" are indistinguishable without asking.
    say(kal_env_arg_count() == 0 && kal_env_var_count() == 0
            ? "env empty\n" : "env unexpected\n");

    // Reaches the host as QEMU's exit status, because SRST is a real shutdown
    // rather than a spin.
    kal_exit(p && t1 > t0 ? 0 : 1);
}

asm(".section .text.entry\n.globl _start\n_start:\n"
    "  la sp, __stack_top\n  call kmain\n1: j 1b\n");
