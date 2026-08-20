/* The SBI calling convention, and the two extensions this implementation uses.
 *
 * SBI is the interface a RISC-V supervisor has to the firmware beneath it. It
 * is invoked with `ecall`: the extension identifier in a7, the function
 * identifier in a6, arguments in a0-a5, and a two-word result in a0 (an error
 * code) and a1 (a value). ⚠️ That shape is the same two-word return openkal
 * specifies for its own results, and for the same reason — it is what crosses a
 * privilege boundary in registers on this architecture.
 *
 * WHY THIS BACKEND IS NOT BOARD-SPECIFIC
 *
 * The console here is not a store to an address. It is a call into firmware
 * that already knows which device the board has, which is why one binary runs
 * on QEMU's `virt` under OpenSBI and on a real board under the same firmware
 * without being rebuilt. That is the difference between this backend and a UART
 * backend, and it is the reason both exist.
 */
#ifndef OPENKAL_OPENSBI_SBI_H
#define OPENKAL_OPENSBI_SBI_H

typedef unsigned long sbi_word;

struct sbi_ret { long error; long value; };

static inline struct sbi_ret sbi_call(sbi_word eid, sbi_word fid,
                                      sbi_word a0, sbi_word a1, sbi_word a2) {
    register sbi_word r_a0 __asm__("a0") = a0;
    register sbi_word r_a1 __asm__("a1") = a1;
    register sbi_word r_a2 __asm__("a2") = a2;
    register sbi_word r_a6 __asm__("a6") = fid;
    register sbi_word r_a7 __asm__("a7") = eid;
    __asm__ __volatile__("ecall"
                         : "+r"(r_a0), "+r"(r_a1)
                         : "r"(r_a2), "r"(r_a6), "r"(r_a7)
                         : "memory");
    struct sbi_ret ret;
    ret.error = (long)r_a0;
    ret.value = (long)r_a1;
    return ret;
}

/* Debug Console extension (SBI v2.0). Takes a whole buffer, so a line costs one
 * trap rather than one per character. */
#define SBI_EXT_DBCN            0x4442434EUL
#define SBI_DBCN_WRITE          0UL
#define SBI_DBCN_READ           1UL
#define SBI_DBCN_WRITE_BYTE     2UL

/* Legacy console (SBI v0.1). One character per call, and deprecated — kept
 * because firmware that predates v2.0 has no DBCN, and falling back is what
 * lets one binary run on both. */
#define SBI_EXT_LEGACY_PUTCHAR  0x01UL
#define SBI_EXT_LEGACY_GETCHAR  0x02UL

/* System Reset extension (SBI v0.3). */
#define SBI_EXT_SRST            0x53525354UL
#define SBI_SRST_RESET          0UL
#define SBI_SRST_TYPE_SHUTDOWN  0UL
#define SBI_SRST_REASON_NONE    0UL
#define SBI_SRST_REASON_FAILURE 1UL

/* Base extension, used to ask whether DBCN is present before relying on it. */
#define SBI_EXT_BASE            0x10UL
#define SBI_BASE_PROBE_EXT      3UL

#define SBI_SUCCESS             0L
#define SBI_ERR_NOT_SUPPORTED  -2L

#endif /* OPENKAL_OPENSBI_SBI_H */
