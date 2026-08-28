// openkal.env on the RISC-V Supervisor Binary Interface.
//
// ⚠️ EVERY ANSWER HERE IS EMPTY, AND THAT IS AN IMPLEMENTATION RATHER THAN A
// STUB. THE DIFFERENCE IS THE ONE CLAUSE 6.2 TURNS ON.
//
// Clause 6.2 forbids the arrangement this file could be mistaken for: "an
// operation that is present and always fails is a defect; the remedy is that
// its absence be expressed by its absence". So the question that has to be
// answered before writing any of it is whether an image started by firmware
// HAS an environment, and the answer is that it has one and it is empty.
//
// Those are not the same situation, and the distinction is visible in what a
// caller sees. An implementation that refused would have to report a failure,
// and a caller would have to distinguish "this machine cannot tell me" from
// "there are none". Here there is nothing to distinguish: the count is zero,
// and every enumeration of zero things ends immediately. A hosted
// implementation asked for the fourth of three arguments returns exactly what
// this one returns for the first of none, by the same rule, and no caller needs
// a special case for either.
//
// ⚠️ WHY IT IS EMPTY, WHICH IS A FACT ABOUT THE ENTRY CONTRACT AND NOT ABOUT SBI
//
// Firmware enters the image at its load address with a hart identifier and a
// device tree in registers. Neither is a command line. A device tree CAN carry
// one --- `/chosen/bootargs` --- and this package does not read the device
// tree, for the reason time.cpp records: the entry sequence belongs to the
// consumer, and the pointer is gone by the time anything here runs.
//
// So "no arguments" describes this arrangement accurately today. If the entry
// contract later forwards the device tree, this file is where `bootargs` would
// be split, and the interface does not change --- which is the point of
// answering the question rather than declining it.

#include <openkal/env.h>

extern "C" {

// A machine started by firmware receives no arguments and no named values, and
// the interface is provided rather than withheld: the answer "there are none"
// is an answer, and a program that asks is answered rather than failing to link.
//
// Each of these copies into the caller's buffer and reports the length the value
// has. Here there is no value, so each reports the condition --- which is what
// distinguishes "there is no such thing" from "there is one and it is empty",
// and is the distinction the interface exists to preserve.
kal_uintptr kal_env_arg_count(void) { return 0; }

kal_intptr kal_env_arg(kal_uintptr, char*, kal_uintptr) {
    return -kal_err_not_found;
}

kal_uintptr kal_env_var_count(void) { return 0; }

kal_intptr kal_env_var(const char*, kal_uintptr, char*, kal_uintptr) {
    return -kal_err_not_found;
}

kal_intptr kal_env_var_at(kal_uintptr, char*, kal_uintptr) {
    return -kal_err_not_found;
}

}  // extern "C"
