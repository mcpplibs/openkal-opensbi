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

`abort`, `stream` and `memory` — openkal's core set. An implementation provides
an interface in whole or not at all, so the absence of `fs`, `process`, `task`,
`env` and `time` is not a deviation; `import openkal.task;` simply does not
resolve.

⚠️ **`time` is absent deliberately, and SBI does have a timer.** SBI can arm a
timer interrupt, which is a mechanism for a kernel rather than a clock a program
can read. Reporting a clock that does not advance would make every timed wait
silently wrong — the specification's own example of a simulation that
disqualifies an interface from being provided at all.

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
package. It is loaded at `0x80200000`, because OpenSBI itself occupies the start
of RAM.

[kal]: https://github.com/mcpplibs/openkal
