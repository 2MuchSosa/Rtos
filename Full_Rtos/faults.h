//Aisosa Okunbor
#ifndef FAULTS_H_
#define FAULTS_H_
#include <stdint.h>

//-----------------------------------------------------------------------------
// Exception frame automatically stacked by Cortex-M on exception entry
//-----------------------------------------------------------------------------
typedef struct
{
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;     // R14 at fault
    uint32_t pc;     // Return PC
    uint32_t xpsr;   // Program status
} exc_frame_t;


//-----------------------------------------------------------------------------
// Dump helpers
//-----------------------------------------------------------------------------
// New: uses the actual active SP passed from asm.s
void printFaultDumpActive(uint32_t *active_sp, uint32_t *msp, uint32_t *psp);

// Legacy: keeps your old 2-arg signature (chooses PSP if non-null, else MSP)
void printFaultDump(uint32_t *msp, uint32_t *psp);

//-----------------------------------------------------------------------------
// C-side handlers called by asm.s stubs
//  asm.s sets: R0 = active SP, R1 = MSP, R2 = PSP, R3 = EXEC_RETURN
//-----------------------------------------------------------------------------
void enableFaultsAndTraps(void);
void hardFaultC (uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return);
void usageFaultC(uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return);
void busFaultC  (uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return);
void mpuFaultC  (uint32_t *active_sp, uint32_t *msp, uint32_t *psp, uint32_t exc_return);

// Do NOT change this one (matches your current asm)
//void pendsvC    (uint32_t *msp, uint32_t *psp);



#endif // FAULTSISR_H_
