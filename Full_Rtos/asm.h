//Aisosa Okunbor
#ifndef _asm_h
#define _asm_h
#include <stdint.h>
#include "shell.h"
// -----------------------------------------------------------------------------
// Low-level Cortex-M stack pointer control and register utilities
// -----------------------------------------------------------------------------

// Read Main Stack Pointer (MSP)
uint32_t *getmsp(void);

// Read Process Stack Pointer (PSP)
uint32_t *getpsp(void);

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
//void pendsvHandler(void);

void start(void* sp, void* fn);

// Start first task - jumps to function address in R0
void startTask(void* fn);

//software push and pop
void swpush();
void swpop();


//for pidofSvc call
uint32_t svcPidof(const char *name);
//for svckill call
uint32_t svcKill(uint32_t pid);
uint32_t svcPkill(const char* name);
//for svcRestart
//uint32_t svcRevive(const char* name);

//for svcPc
uint32_t svcPs(PS_PRINT* ps_data);
//for svcIpcs
uint32_t svcIpcs(IPCS_PRINT* ipcs_data);
//for svcRun
//uint32_t svcRun(const char* name);

#endif
