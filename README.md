# openkal-opensbi

An implementation of [openkal][kal] on the RISC-V Supervisor Binary Interface.

```toml
[dependencies]
openkal         = "0.5.1"
openkal-opensbi = "0.1.0"
```

## ⭐ The portable RISC-V backend, as distinct from a board's own

A board-supplied backend writes to a device address, and that address is a board
fact. The same binary on a second RISC-V machine writes to something that is not
a UART: it compiles, links, runs, and prints nothing.

SBI has no such property. The console here is a call into firmware that already
knows the machine, so one image runs under OpenSBI on QEMU's `virt` and on a real
board without being rebuilt.

Both kinds are legitimate, and a project chooses by which property it needs: SBI
requires firmware beneath it, a board backend does not.

## What is implemented

`abort`, `stream` and `memory` — openkal's core set — plus `time` and `env`. An
implementation provides an interface in whole or not at all, so the absence of
`fs`, `process` and `task` is not a deviation; `import openkal.task;` simply
does not resolve. This machine has no storage, no second image to start and no
scheduler, and clause 6.2 says the remedy for an operation that cannot be
provided is that its absence be expressed by its absence rather than by a
run-time refusal.

The five interfaces version 0.8 adds are absent for the same reason and by the
same rule. This machine has no network, so `net` and `datagram` are not provided.
It has no memory management unit to copy an address space with, so `space` is
not. It has no second execution context to bound a wait against, so `timeout` is
not: an operation here either completes or does not, and a bound upon it would
report an expiry that could never occur. `terminal` is absent because the console
is a serial line whose settings this firmware does not expose; a stream is
reported as interactive and nothing acts upon that fact.

None of the five is a deviation. Clause 6.1 makes an interface an implementation
does not provide absent at the link, so a program requiring one is refused when
it is built rather than when it runs.

⚠️ **`time` used to be on that list, with a reason, and the reason was wrong.**

It read: SBI can arm a timer interrupt, which is a mechanism for a kernel rather
than a clock a program can read. The first half is true. The second does not
follow from it — this architecture also exposes `rdtime` directly to the
program, through the `time` CSR, independently of SBI's timer extension. Two
different facilities, and the absence of the first says nothing about the
second.

Measured 2026-08-23 under OpenSBI on QEMU's `virt`, from supervisor mode with no
kernel beneath:

```
t0=333572 t1=381292  ADVANCES
```

⭐ The conclusion got rechecked and the reason beside it did not. The two
minutes that refuted it had been available for as long as the file existed.

`time` is therefore provided: a monotonic count, an exact granularity, and a
sleep that spins on the same counter — which on a machine with one execution
context and nothing to yield to is what sleeping *is*, so
`KAL_TIME_PROP_SLEEP_PRECISE` is set truthfully. **Not** a wall clock: SBI
defines no facility for one, `KAL_TIME_PROP_WALL_AVAILABLE` is clear, and clause
6.2 makes that a property rather than a missing interface.

`env` is provided and every answer is empty. That is an implementation, not a
stub: firmware enters the image with a hart identifier and a device tree, and
neither is a command line, so this environment *has* an environment and it has
nothing in it. A caller enumerating zero things needs no special case, which is
exactly what distinguishes this from the run-time refusal clause 6.2 forbids.

## The heap is a bump allocator, and that is a bound rather than an omission

SBI provides no allocator, so this one hands out from a static region and
`kal_free` does nothing. openkal's criterion admits it: an implementation that
can be exhausted **bounds** what is available, whereas one that makes its callers
silently wrong is a simulation. Exhaustion is a defined outcome — `kal_alloc`
returns null — and every caller already handles it.

The region is 64 KiB by default. A project that needs another figure overrides
`OPENKAL_OPENSBI_HEAP_BYTES` rather than editing this package. A program whose
allocation pattern needs reuse should place a real allocator above this one;
that is policy, and openkal carries mechanism.

## The memory map is this package's, because it is a fact about this machine

`board.ld` states where the firmware hands control over (`0x80200000`, since
OpenSBI itself occupies the start of RAM), what the layout has to contain for a
C++ program to work — the initialiser arrays, the unwind tables and their bounds,
the thread-local segment — and how much stack and heap the image reserves. It
reaches a consumer's link line through this package's build program, in the same
way its `kal_*` definitions reach the consumer's objects.

A program therefore states nothing. Until 2026-08-23 every program that wanted to
run here carried a copy: `examples/hello` had one, the C++ runtime's
`same-source` example had a second, and the specification's conformance suite
would have needed a third. None of those is a property of a program.

⚠️ **A program that states one as well states it twice.** The fact reaches
consumers transitively, and two linker scripts are both applied — which fails as
overlapping output sections and says nothing about there being two:

```
ld.lld: error: section .eh_frame file range overlaps with .debug_str_offsets
```

The sizes — 256 KiB of stack, 16 MiB of heap — are generous rather than minimal,
and that is the one judgement in the file. A C program printing a string needs
neither; a program carrying a C library and a C++ standard library does, and it
exhausts smaller figures during its own initialisation, before `main`, so what it
looks like is a program that starts and prints nothing. Both regions lie past the
end of the image and cost nothing in it. A board with far less memory states its
own map instead of taking this one.

## Two details that were measured rather than assumed

**DBCN is probed, not assumed.** The Debug Console extension arrived in SBI
v2.0, and older firmware answers `SBI_ERR_NOT_SUPPORTED` — on which every write
would silently transfer nothing. The legacy one-character extension is the
fallback.

⚠️ **No function-local `static`.** A guarded local static compiles to
`__cxa_guard_acquire`/`__cxa_guard_release`, which a freestanding target has no
runtime to supply. Measured: the link fails naming both. `-fno-threadsafe-statics`
would also silence it, but a flag that has to be remembered is weaker than a
construct that cannot require the runtime in the first place.

## Verified

`examples/hello` runs under QEMU with `-bios default`, which is real OpenSBI:

```
hello from openkal over SBI
heap ok
```

The image carries no C library (`[target.<triple>].sysroot = ""`) and no board
package, and it states no memory map of its own: `board.ld` above is this
package's.

The specification's conformance suite also runs here, under the same firmware,
selected to the `core` set — twelve observations held, none failed. Clause 9
makes the behavioural half a property of every implementation, and an
implementation of a machine with no operating system is not exempt from it.

[kal]: https://github.com/mcpplibs/openkal
