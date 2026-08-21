/**
 * FaultHandler.cpp - see FaultHandler.h for the design, verification against
 * PaulStoffregen/cores teensy4/startup.c, and how to read the output.
 */
#include "FaultHandler.h"
#include <imxrt.h>

namespace {

  constexpr uint32_t kMagic      = 0x534D4655u;  // "SMFU"
  constexpr uint32_t kPaintWords = 2048;          // 8 KB stack window
  constexpr uint32_t kPaintValue = 0xEEEEEEEEu;
  constexpr uint32_t kGuardBytes = 256;           // headroom below SP left unpainted

  // 896 bytes below PJRC's own 0x2027FF80 (top 128 bytes of OCRAM, used by
  // CrashReport) - see the header comment. i.MX RT1062 OCRAM on Teensy 4.0
  // runs 0x20200000-0x2027FFFF.
  constexpr uint32_t kRecordAddr = 0x2027FC00u;

  struct FaultRecord {
    uint32_t magic;
    uint32_t ipsr;    // exception number at fault time: 3=Hard 4=MemManage 5=Bus 6=Usage
    uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr;
    uint32_t cfsr, hfsr, mmfar, bfar;
    uint32_t stackUsedWords;   // see begin(): how far into the painted window
                               // (measured from its lowest/deepest address)
                               // the sentinel was overwritten by fault time.
  };
  static_assert(sizeof(FaultRecord) <= 0x80,
                "keep well clear of PJRC's 0x2027FF80 CrashReport block");

  FaultRecord * const g_rec = reinterpret_cast<FaultRecord *>(kRecordAddr);

  uint32_t g_paintBase = 0;   // set by begin(); 0 means "not painted this boot"

  const char *faultName(uint32_t ipsr) {
    switch (ipsr) {
      case 3:  return "HARD";
      case 4:  return "MEMMANAGE";
      case 5:  return "BUS";
      case 6:  return "USAGE";
      default: return "?";
    }
  }

  void printHex32(Print &out, const char *label, uint32_t v) {
    char buf[9];
    for (int i = 7; i >= 0; i--) {
      uint8_t nib = (v >> (i * 4)) & 0xF;
      buf[7 - i] = nib < 10 ? char('0' + nib) : char('A' + nib - 10);
    }
    buf[8] = 0;
    out.print(label);
    out.print("0x");
    out.println(buf);
  }

}  // namespace

extern "C" {

  // Teensyduino's core (cores/teensy4/startup.c) defines this as the RAM-
  // resident, runtime-patchable vector table; every entry defaults to its
  // own unused_interrupt_vector(). We only ever write indices 3-6 here.
  extern void (* volatile _VectorsRam[])(void);

  // Called by smfuFaultTrampoline with r0 = pointer to the 8-word exception
  // stack frame: r0,r1,r2,r3,r12,lr,pc,xpsr (AAPCS: frame arrives in r0).
  void smfuFaultCommon(uint32_t *frame) {
    uint32_t ipsr;
    asm volatile ("mrs %0, ipsr" : "=r" (ipsr));

    g_rec->ipsr = ipsr;
    g_rec->r0   = frame[0];
    g_rec->r1   = frame[1];
    g_rec->r2   = frame[2];
    g_rec->r3   = frame[3];
    g_rec->r12  = frame[4];
    g_rec->lr   = frame[5];   // <-- the piece CrashReport doesn't give us
    g_rec->pc   = frame[6];
    g_rec->xpsr = frame[7];

    g_rec->cfsr  = SCB_CFSR;
    g_rec->hfsr  = SCB_HFSR;
    g_rec->mmfar = SCB_MMFAR;
    g_rec->bfar  = SCB_BFAR;

    uint32_t used = 0;
    if (g_paintBase) {
      volatile uint32_t *p = (volatile uint32_t *)g_paintBase;
      for (uint32_t i = 0; i < kPaintWords; i++) {
        if (p[i] != kPaintValue) used++;
        else break;
      }
    }
    g_rec->stackUsedWords = used;

    // Best-effort live print. USB interrupts are not guaranteed to run while
    // we're inside a fault handler, so this may not flush - the persisted
    // record above (read back by reportIfPending() on the next boot) is the
    // reliable channel and is why this whole mechanism exists.
    Serial.println();
    Serial.print("*** FAULT "); Serial.println(faultName(ipsr));
    printHex32(Serial, "PC  ", g_rec->pc);
    printHex32(Serial, "LR  ", g_rec->lr);
    Serial.flush();

    g_rec->magic = kMagic;   // written last: marks the record valid
    arm_dcache_flush_delete(g_rec, sizeof(*g_rec));

    SCB_AIRCR = 0x05FA0004;  // ARM system reset, same as Controller::reboot()
    while (1) {}
  }

  __attribute__((naked)) void smfuFaultTrampoline(void) {
    // Standard Cortex-M hard-fault trampoline (Joseph Yiu's pattern): bit 2
    // of the EXC_RETURN value left in LR on exception entry tells us whether
    // MSP or PSP was the active stack, i.e. which one holds the exception
    // frame. This firmware never uses PSP (no RTOS), so this always resolves
    // to MSP in practice, but the check costs nothing and keeps the handler
    // correct if that ever changes.
    __asm volatile (
      "tst lr, #4            \n"
      "ite eq                \n"
      "mrseq r0, msp         \n"
      "mrsne r0, psp         \n"
      "b.w   smfuFaultCommon \n"
    );
  }

}  // extern "C"

namespace FaultHandler {

  void begin() {
    _VectorsRam[3] = &smfuFaultTrampoline;  // HardFault
    _VectorsRam[4] = &smfuFaultTrampoline;  // MemManage
    _VectorsRam[5] = &smfuFaultTrampoline;  // BusFault
    _VectorsRam[6] = &smfuFaultTrampoline;  // UsageFault

    // Architectural SCB_SHCSR bit positions (ARMv7-M, unchanged across every
    // Cortex-M3/4/7 part): MEMFAULTENA=16, BUSFAULTENA=17, USGFAULTENA=18.
    SCB_SHCSR |= (1u << 16) | (1u << 17) | (1u << 18);

    uint32_t sp;
    asm volatile ("mov %0, sp" : "=r" (sp));
    uint32_t top = sp - kGuardBytes;
    g_paintBase = top - (kPaintWords * 4);

    volatile uint32_t *p = (volatile uint32_t *)g_paintBase;
    for (uint32_t i = 0; i < kPaintWords; i++) p[i] = kPaintValue;
  }

  void reportIfPending(Print &out) {
    if (g_rec->magic != kMagic) return;

    out.println();
    out.println(F("===== FAULT RECORD (previous run) ====="));
    out.print(F("Type: ")); out.println(faultName(g_rec->ipsr));
    printHex32(out, "PC     ", g_rec->pc);
    printHex32(out, "LR     ", g_rec->lr);
    printHex32(out, "R0     ", g_rec->r0);
    printHex32(out, "R1     ", g_rec->r1);
    printHex32(out, "R2     ", g_rec->r2);
    printHex32(out, "R3     ", g_rec->r3);
    printHex32(out, "R12    ", g_rec->r12);
    printHex32(out, "XPSR   ", g_rec->xpsr);
    printHex32(out, "CFSR   ", g_rec->cfsr);
    printHex32(out, "HFSR   ", g_rec->hfsr);
    printHex32(out, "MMFAR  ", g_rec->mmfar);
    printHex32(out, "BFAR   ", g_rec->bfar);
    out.print(F("MMFAR valid: ")); out.println((g_rec->cfsr & (1u << 7))  ? "YES" : "no");
    out.print(F("BFAR  valid: ")); out.println((g_rec->cfsr & (1u << 15)) ? "YES" : "no");
    out.print(F("Stack window used (words, from the deep end): "));
    out.print(g_rec->stackUsedWords);
    out.print(F(" / "));
    out.println(kPaintWords);
    out.println(F("Run against the exact flashed .elf:"));
    out.println(F("  arm-none-eabi-addr2line -e teensy_code_v4.ino.elf -f -C <PC> <LR>"));
    out.println(F("========================================"));

    g_rec->magic = 0;   // consumed; don't re-print on the next clean boot
  }

}  // namespace FaultHandler
