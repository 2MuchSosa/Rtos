//Aisosa Okunbor
#ifndef _asm_h
#define _asm_h

#include <stdint.h>

// -----------------------------------------------------------------------------
// Low-level Cortex-M stack pointer control and register utilities
// -----------------------------------------------------------------------------

// Read Main Stack Pointer (MSP)
uint32_t *getmsp(void);

// Read Process Stack Pointer (PSP)
uint32_t *getmsp(void);

// Set Process Stack Pointer
void setpsp(uint32_t *psp);
void setmsp(uint32_t *msp);

// Switch Thread mode to use PSP (CONTROL.SPSEL = 1)
void usepsp(void);
void setunpriv(void);
void clearunpriv(void);

// -----------------------------------------------------------------------------
// Exception stubs - implemented in asm.s, these are entry points for fault ISRs
// Each one gathers MSP/PSP and jumps to the C handler (e.g., hardFaultC)
// -----------------------------------------------------------------------------

void hardFaultHandler(void);
void usageFaultHandler(void);
void busFaultHandler(void);
void mpuFaultHandler(void);
void pendsvHandler(void);


#endif
