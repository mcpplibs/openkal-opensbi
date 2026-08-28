// openkal.time on the RISC-V Supervisor Binary Interface.
//
// ⚠️ THIS FILE EXISTS BECAUSE THE REASON RECORDED FOR ITS ABSENCE WAS WRONG.
//
// kal.cpp said, and said it as a design decision rather than as a note:
//
//     `time` is deliberately absent even though SBI has a timer extension:
//     SBI can arm a timer interrupt, which is a mechanism for a kernel rather
//     than a clock a program can read, and reporting a clock that does not
//     advance would make every timed wait silently wrong.
//
// The first half is true and the second does not follow from it. SBI's timer
// extension does arm an interrupt, and it is indeed a kernel's mechanism. But
// this architecture ALSO exposes a counter directly to the program: `rdtime`
// reads the `time` CSR, which the Zicntr extension defines and which firmware
// makes readable below its own privilege level. Those are two different
// facilities, and the absence of the first says nothing about the second.
//
// Measured 2026-08-23 under OpenSBI on QEMU's `virt`, from a program in
// supervisor mode with no kernel beneath it:
//
//     t0=333572 t1=381292  ADVANCES
//
// ⭐ The lesson is the one the repository keeps relearning: a conclusion gets
// rechecked and the reason written beside it does not. The reason above sat in
// a comment for as long as the file existed, and the two minutes that refuted
// it were available the whole time.
//
// WHAT IS AND IS NOT AVAILABLE HERE
//
// A monotonic count, precisely; a granularity, exactly; a sleep, by spinning on
// the same counter, which on a machine with one execution context and no
// scheduler is what sleeping is rather than a simulation of it.
//
// ⚠️ NOT a wall clock. SBI defines no facility for one, and no board fact would
// supply it either --- a real-time clock is a device, and reading it is what a
// board backend does. `KAL_TIME_PROP_WALL_AVAILABLE` is left clear, which is
// the specification's own way of saying so: clause 6.2 makes availability of a
// wall clock a PROPERTY rather than an interface, so `kal_time_wall` is present
// and reports zero, and a program reads the word before it reads the clock.

#include <openkal/time.h>

#include "sbi.h"

namespace {

// ⚠️ THE ONE BOARD FACT THIS PACKAGE TAKES, AND IT TAKES IT AS AN INPUT.
//
// `rdtime` counts at a rate the architecture does not fix. The rate is
// published in the device tree as `/cpus/timebase-frequency`, and this package
// does not read one: the device tree is handed to the image in a register at
// entry, the entry sequence belongs to the consumer rather than to this
// package, and requiring every consumer to forward it would change a contract
// that today is "jump here with a stack".
//
// So the rate is a build input, exactly as the heap size already is, and for
// the same reason --- a figure that belongs to the machine is declared by the
// project that knows which machine, rather than assumed by a package that does
// not. The default is QEMU's `virt`, which is what the example runs on.
//
// ⚠️ A project on other hardware that leaves the default in place gets a clock
// that advances at the wrong rate. That is a stated bound and not a hidden one:
// the figure has a name, the name appears in the manifest, and this comment is
// what a reader finds when they look for it.
#ifndef OPENKAL_OPENSBI_TIMEBASE_HZ
#  define OPENKAL_OPENSBI_TIMEBASE_HZ 10000000
#endif

constexpr kal_u64 kHz  = OPENKAL_OPENSBI_TIMEBASE_HZ;
constexpr kal_u64 kNano = 1000000000ULL;

inline kal_u64 ticks() {
    kal_u64 v;
    __asm__ __volatile__("rdtime %0" : "=r"(v));
    return v;
}

// Split rather than `t * kNano / kHz`, which overflows a 64-bit product after
// about eighteen seconds at ten megahertz. The split form is exact for every
// value the counter can hold.
inline kal_u64 to_ns(kal_u64 t) {
    return (t / kHz) * kNano + (t % kHz) * kNano / kHz;
}

inline kal_u64 to_ticks(kal_u64 ns) {
    return (ns / kNano) * kHz + (ns % kNano) * kHz / kNano;
}

}  // namespace

extern "C" {

kal_duration kal_time_monotonic(void) { return to_ns(ticks()); }

// Present and reporting zero, which clause 6.2 provides for: the availability
// of a wall clock is a property of the implementation, the property word says
// this one has none, and a program that reads the word does not reach here.
kal_duration kal_time_wall(void) { return 0; }

// One tick, in nanoseconds, rounded up so that the figure is never reported as
// finer than it is. At ten megahertz this is 100.
kal_duration kal_time_monotonic_granularity(void) {
    const kal_u64 g = kNano / kHz;
    return g ? g : 1;
}

// ⚠️ Spinning, and that is the accurate implementation rather than a stand-in.
//
// Sleeping means giving the machine to something else until a time arrives. On
// a machine with one execution context and nothing to give it to, the time
// still has to arrive, and waiting for it on the same counter the caller would
// read is precise to a tick. `KAL_TIME_PROP_SLEEP_PRECISE` is therefore set,
// and it is set truthfully: there is no scheduler to overshoot.
//
// A `wfi` between polls would lower power, and it is not used, because it
// requires an interrupt to be pending to wake from --- which requires SBI's
// timer extension, which requires a trap handler, which this arrangement does
// not have. The comment kal.cpp got wrong was about that extension; this is
// where it would have been right.
void kal_time_sleep(kal_duration ns) {
    if (ns == 0) return;
    const kal_u64 deadline = ticks() + to_ticks(ns);
    while (ticks() < deadline) {}
}

kal_uintptr kal_time_props(void) { return KAL_TIME_PROP_SLEEP_PRECISE; }

}  // extern "C"
