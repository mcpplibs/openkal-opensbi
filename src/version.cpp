#include <openkal/version.h>

// What this implementation says about itself before it is used.
//
// NOT AN INTERFACE, SO NOT CONDITIONAL ON ONE, AND NOT A BURDEN ON A MACHINE
// WITH NO OPERATING SYSTEM EITHER: both answers are constants. Clause 3.2 closes
// the set of core INTERFACES and these provide no resource; what they let a
// consumer do is ask which interfaces are here before it calls into one, which a
// consumer that is linked learns from the linker and one bound otherwise cannot.
extern "C" {

kal_u64 kal_version(void) { return KAL_VERSION; }

// ⚠️ THE WORD AGREES WITH WHAT IS EXPORTED, WHICH IS THE WHOLE OF ITS VALUE.
// This machine has no storage, no second image and no scheduler, so
// `openkal.fs', `openkal.process' and `openkal.task' are absent as definitions
// --- and the word says so rather than leaving a consumer to discover it by
// failing to link, which is the only report the earlier arrangement had.
kal_u64 kal_interfaces(void) {
    return KAL_IFACE_ABORT | KAL_IFACE_STREAM | KAL_IFACE_MEMORY
         | KAL_IFACE_ENV   | KAL_IFACE_TIME;
}

}
