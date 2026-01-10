//Aisosa Okunbor
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "shell.h"
#include "asm.h"
#include "faults.h"

//volatile uint32_t pid = 0;

void enableFaultsAndTraps(void)
{
    // Enable Usage Fault, Bus Fault, and Memory Management Fault
    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE |
                           NVIC_SYS_HND_CTRL_BUS |
                           NVIC_SYS_HND_CTRL_MEM;

    //Enable divide-by-zero trap
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;

    //Enable unaligned access trap
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_UNALIGNED;
}

// ------------------ Small print helpers ------------------

static void print_hex(const char *label, uint32_t v)
{
    char hex[12];
    uintToStringHex(hex, v);
    putsUart0(label);
    putsUart0(hex);
    putcUart0('\n');
}


static inline void print_flag(const char *name)
{
    putsUart0("  - "); putsUart0(name); putcUart0('\n');
}
static void printInstructionAtPC(uint32_t pc)
{
    char hex[12];

    // PC bit[0] indicates Thumb state, so clear it before dereferencing
    uint32_t aligned_pc = pc & ~1u;

    // Optional: avoid crashing if PC is outside known memory
    bool in_flash = (aligned_pc < 0x00040000u);                   // 256 KB Flash
    bool in_sram  = (aligned_pc >= 0x20000000u && aligned_pc < 0x20008000u);
    if (!in_flash && !in_sram)
    {
        putsUart0("  [!] PC points outside readable memory\n");
        return;
    }

    // Read two half-words (Thumb instructions are 16 or 32 bits)
    volatile const uint16_t *h = (const uint16_t *)aligned_pc;
    uint16_t hw0 = h[0];
    uint16_t hw1 = h[1];

    putsUart0("  Instr @PC (halfwords): ");
    uintToStringHex(hex, hw0);
    putsUart0(hex);
    putcUart0(' ');
    uintToStringHex(hex, hw1);
    putsUart0(hex);
    putcUart0('\n');
}
// ------------------ Dump helpers ------------------

void printFaultDumpActive(uint32_t *active_sp, uint32_t *msp, uint32_t *psp)
{
    putsUart0("\n--- Fault Dump ---\n");
   // print_dec("Process ID: ", pid);

    print_hex("MSP: 0x", (uint32_t)msp);
    print_hex("PSP: 0x", (uint32_t)psp);
    print_hex("Active SP: 0x", (uint32_t)active_sp);

    // Decode the stacked frame from the *actual* active SP
    exc_frame_t *f = (exc_frame_t *)active_sp;

    print_hex(" \n\nR0 : 0x", f->r0);
    print_hex(" R1 : 0x", f->r1);
    print_hex(" R2 : 0x", f->r2);
    print_hex(" R3 : 0x", f->r3);
    print_hex(" R12: 0x", f->r12);
    print_hex(" LR : 0x", f->lr);
    print_hex(" PC : 0x", f->pc);
    printInstructionAtPC(f->pc);
    print_hex(" xPSR: 0x", f->xpsr);
}

// Legacy wrapper preserved for pendsvC (2-arg call sites)
void printFaultDump(uint32_t *msp, uint32_t *psp)
{
    uint32_t *active_sp = (psp != 0) ? psp : msp;
    printFaultDumpActive(active_sp, msp, psp);
}

// ------------------ Bit-field decoding (ARM CFSR/HFSR) ------------------
// CFSR (NVIC_FAULT_STAT_R) layout:
//  [ 7: 0] MMFSR
//  [15: 8] BFSR
//  [31:16] UFSR

static void decode_mmfsr(uint32_t cfsr)
{
    uint32_t mmfsr = (cfsr & 0x000000FFu);
    if (!mmfsr) return;

    putsUart0("\nMMFSR:\n");
    if (mmfsr & (1u << 0)) print_flag("IACCVIOL  (Instruction access violation)");
    if (mmfsr & (1u << 1)) print_flag("DACCVIOL  (Data access violation)");
    if (mmfsr & (1u << 3)) print_flag("MUNSTKERR (Unstacking error)");
    if (mmfsr & (1u << 4)) print_flag("MSTKERR   (Stacking error)");
    if (mmfsr & (1u << 5)) print_flag("MLSPERR   (Lazy FP preserve error)");
    if (mmfsr & (1u << 7)) print_flag("MMARVALID (MMFAR is valid)");
}

static void decode_bfsr(uint32_t cfsr)
{
    uint32_t bfsr = (cfsr >> 8) & 0xFFu;
    if (!bfsr) return;

    putsUart0("BFSR:\n");
    if (bfsr & (1u << 0)) print_flag("IBUSERR     (Instruction bus error)");
    if (bfsr & (1u << 1)) print_flag("PRECISERR   (Precise data bus error)");
    if (bfsr & (1u << 2)) print_flag("IMPRECISERR (Imprecise data bus error)");
    if (bfsr & (1u << 3)) print_flag("UNSTKERR    (Unstacking error)");
    if (bfsr & (1u << 4)) print_flag("STKERR      (Stacking error)");
    if (bfsr & (1u << 5)) print_flag("LSPERR      (Lazy FP preserve error)");
    if (bfsr & (1u << 7)) print_flag("BFARVALID   (BFAR is valid)");
}

static void decode_ufsr(uint32_t cfsr)
{
    uint32_t ufsr = (cfsr >> 16) & 0xFFFFu;
    if (!ufsr) return;

    putsUart0("UFSR:\n");
    if (ufsr & (1u << 0))  print_flag("UNDEFINSTR (Undefined instruction)");
    if (ufsr & (1u << 1))  print_flag("INVSTATE   (Invalid state)");
    if (ufsr & (1u << 2))  print_flag("INVPC      (Invalid PC load by EXC_RETURN)");
    if (ufsr & (1u << 3))  print_flag("NOCP       (No coprocessor)");
    if (ufsr & (1u << 8))  print_flag("UNALIGNED  (Unaligned access)");
    if (ufsr & (1u << 9))  print_flag("DIVBYZERO  (Divide by zero)");
}

static void decode_hfsr(uint32_t hfsr)
{
    if (!hfsr) return;

    putsUart0("HFSR:\n");
    if (hfsr & (1u << 1))  print_flag("VECTTBL (Bus fault on vector table read)");
    if (hfsr & (1u << 30)) print_flag("FORCED  (Escalated configurable fault)");
    if (hfsr & (1u << 31)) print_flag("DEBUGEVT (Debug event)");
}

static void print_exc_return(uint32_t exc_return)
{
    print_hex("\nEXC_RETURN (handler LR): 0x", exc_return);

    // Quick decode (ARMv7-M):
    //  0xFFFFFFF1 -> return to Handler, use MSP
    //  0xFFFFFFF9 -> return to Thread,  use MSP
    //  0xFFFFFFFD -> return to Thread,  use PSP
    if (exc_return == 0xFFFFFFF1) putsUart0("  -> to Handler, MSP\n");
    else if (exc_return == 0xFFFFFFF9) putsUart0("  -> to Thread, MSP\n");
    else if (exc_return == 0xFFFFFFFD) putsUart0("  -> to Thread, PSP\n");

}


// ------------------ Fault Handlers (active_sp, msp, psp, EXEC_RETURN) ------------------

void hardFaultC(uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return)
{
    putsUart0("\n*** Hard Fault ***\n");

    //print pid here

    printFaultDumpActive(active_sp, msp, psp);
    print_exc_return(exc_return);

    uint32_t hfsr = NVIC_HFAULT_STAT_R;
    uint32_t cfsr = NVIC_FAULT_STAT_R;

    print_hex("HFSR: 0x", hfsr);
    print_hex("CFSR: 0x", cfsr);
    decode_hfsr(hfsr);
    decode_mmfsr(cfsr);
    decode_bfsr(cfsr);
    decode_ufsr(cfsr);

    if (cfsr & (1u << 7))  print_hex("MMFAR: 0x", NVIC_MM_ADDR_R);
    if (cfsr & (1u << 15)) print_hex("BFAR : 0x", NVIC_FAULT_ADDR_R);

    while(1);
}

void usageFaultC(uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return)
{
    putsUart0("\n*** Usage Fault ***\n");

    //print pid here

    printFaultDumpActive(active_sp, msp, psp);
    print_exc_return(exc_return);

    uint32_t cfsr = NVIC_FAULT_STAT_R;
    print_hex("CFSR: 0x", cfsr);
    decode_ufsr(cfsr);
    while(1);
}

void busFaultC(uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return)
{
    putsUart0("\n*** Bus Fault ***\n");

    //print pid here

    printFaultDumpActive(active_sp, msp, psp);
    print_exc_return(exc_return);

    uint32_t cfsr = NVIC_FAULT_STAT_R;
    print_hex("CFSR: 0x", cfsr);
    decode_bfsr(cfsr);

    if (cfsr & (1u << 15)) print_hex("BFAR : 0x", NVIC_FAULT_ADDR_R);

    while(1);
}

void mpuFaultC(uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return)
{
    putsUart0("\n*** MPU Fault ***\n");

    //print pid here

    printFaultDumpActive(active_sp, msp, psp);
    print_exc_return(exc_return);

    uint32_t cfsr = NVIC_FAULT_STAT_R;
    print_hex("CFSR: 0x", cfsr);
    decode_mmfsr(cfsr);

    if (cfsr & (1u << 7)) print_hex("MMFAR: 0x", NVIC_MM_ADDR_R);

    // If you want to reschedule/kill offender, you can PendSV here:
     NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
}

// ------------------ PendSV (unchanged signature) ------------------

//void pendsvC(uint32_t *msp, uint32_t *psp)
//{
//    uint32_t cfsr = NVIC_FAULT_STAT_R;
//    const uint32_t mpuMask = NVIC_FAULT_STAT_IERR | NVIC_FAULT_STAT_DERR;
//
//
//        putsUart0("\n*** PendSV ***\n");
//
//        char* process = "pid";
//        putsUart0("\nPendsv fault in process ");
//        putsUart0(process);
//        putcUart0('\n');
//
//
//    if ((cfsr & mpuMask) != 0)
//    {
//        // Clear the MemManage fault *status* bits (write-1-to-clear)
//        NVIC_FAULT_STAT_R = mpuMask;
//
//        // Also clear the MemManage fault *pending* latch
//        NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_MEMP;  // write-1-to-clear
//
//        putsUart0(" - called from MPU\n");
//    }
//
//    // Legacy helper keeps old behavior (choose PSP if present)
//    //printFaultDump(msp, psp);
//    // Context switch logic can live here (save/restore R4-R11 using PSP)
//
//
//        while(1);
//}
