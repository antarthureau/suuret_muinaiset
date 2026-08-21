/**
 * FaultHandler.h - Cortex-M7 fault capture for Teensy 4.0, written for the
 * "linker glue" fault hunt on Suuret Muinaiset (LONG/leader unit).
 *
 * WHY THIS EXISTS
 *   CrashReport (built into Teensyduino) already tells us the PC lands in
 *   linker glue next to _reboot_Teensyduino_: the CPU branched to a garbage-
 *   but-flash-shaped address. Checked against PaulStoffregen/cores
 *   teensy4/startup.c: every fault vector (Hard/Mem/Bus/Usage) is routed to
 *   one shared handler, unused_interrupt_vector(), which captures CFSR,
 *   HFSR, MMFAR, BFAR and the stacked PC (stack[6]) into a fixed struct at
 *   0x2027FF80 (the top 128 bytes of OCRAM) - and that struct is exactly what
 *   CrashReport prints after reboot. It does NOT capture LR (stack[5]): the
 *   return address live in the LR register at the moment of the fault, which
 *   in the common case of a bad BL/BLX/BX names the function that made the
 *   jump. That is the one piece of information this hunt needs and
 *   CrashReport cannot give us.
 *
 * WHAT THIS ADDS
 *   1. begin() - call as the very first line of setup(), before anything
 *      else runs. It:
 *        - installs a second handler into just the four fault-vector slots
 *          of _VectorsRam (HardFault/MemManage/BusFault/UsageFault), ahead
 *          of PJRC's generic one. Every other vector (peripheral IRQs, USB,
 *          etc.) is untouched.
 *        - enables the Bus/Usage/MemManage sub-handlers (SCB_SHCSR) so a
 *          fault reports its real type via IPSR instead of only ever
 *          escalating to HardFault.
 *        - paints an 8 KB window of stack memory, just below the current
 *          stack pointer, with a sentinel pattern. This is a crude
 *          high-water-mark check: if the audio ISR (or any deep call chain)
 *          is overflowing the stack, the sentinel nearest the painted floor
 *          gets overwritten, and the next fault report says so.
 *   2. reportIfPending() - call once Serial is up (right after the existing
 *      CrashReport print in Controller::setup()). Prints and clears a
 *      pending record from the previous run; a no-op on a clean boot.
 *
 *   The record is written to a fixed OCRAM address, 0x2027FC00, chosen to
 *   sit well clear of (896 bytes below) PJRC's own 0x2027FF80 reservation -
 *   same "OCRAM survives the SCB_AIRCR software reset" property CrashReport
 *   itself already relies on, and that this installation is already
 *   observing works (that is how the existing CrashReport output survives
 *   to the next boot).
 *
 * USING THE OUTPUT
 *   pc/lr are raw addresses. Against the exact .elf that was flashed:
 *     arm-none-eabi-addr2line -e teensy_code_v4.ino.elf -f -C <pc> <lr>
 *   pc is where execution died (expected: still glue, as before). lr is new:
 *   in the common "branched to a bad address" case it is the return address
 *   of the call/branch that jumped there, which should resolve into real
 *   application code instead of glue.
 *
 *   stackUsedWords close to or at kPaintWords (2048) means the stack reached,
 *   or overran, the painted floor before the fault - direct evidence for a
 *   stack-overflow origin rather than a one-off corrupted pointer. See the
 *   ranked suspect list for why the audio ISR / SD read path is the leading
 *   candidate for that.
 *
 * CAVEATS, READ BEFORE FLASHING
 *   - The fixed OCRAM address and the _VectorsRam layout were verified
 *     against the current PaulStoffregen/cores teensy4/startup.c (fetched
 *     2026-08-19). If the installed Teensyduino core differs meaningfully
 *     from that, double-check before relying on the persisted record; the
 *     live best-effort Serial print in the handler is a second, independent
 *     channel for exactly that reason.
 *   - The painted stack window is real, live stack memory (8 KB directly
 *     below the stack pointer at boot), not a separate buffer. That is
 *     deliberate - it is the only way to observe real stack depth - but it
 *     means kPaintWords should stay well inside whatever headroom exists
 *     between the top of the stack and the top of .bss/.data. If in doubt,
 *     check the linker .map file before increasing it from the current 8 KB.
 */
#ifndef SM_FAULTHANDLER_H
#define SM_FAULTHANDLER_H

#include <Arduino.h>

namespace FaultHandler {

  // Call as the first line of setup(), before anything else runs.
  void begin();

  // Call early in setup(), once Serial is up. Prints and clears any pending
  // fault captured during the previous run; a no-op on a clean boot.
  void reportIfPending(Print &out);

}  // namespace FaultHandler

#endif  // SM_FAULTHANDLER_H
