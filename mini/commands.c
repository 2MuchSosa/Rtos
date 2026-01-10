//Aisosa Okunbor
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include "clock.h"
#include "tm4c123gh6pm.h"
#include "wait.h"
#include "gpio.h"
#include "nvic.h"
#include "uart0.h"
#include "commands.h"
#include "stringhandler.h"
#include "accesshandler.h"
#include "memoryhandler.h"
#include "faultsISR.h"
#include "asm.h"



void reboot()
{

    NVIC_APINT_R =  NVIC_APINT_VECTKEY  | NVIC_APINT_SYSRESETREQ;
}


void ps()
{
    putsUart0("  PS called\n\n");
}


void ipcs()
{
    putsUart0("  IPCS called\n\n");
}


void kill(uint32_t pid)
{
    char buffer[12];          // big enough for a 32-bit number
    uitoa(pid, buffer);       // convert pid to string

    putsUart0(" PID ");
    putsUart0(buffer);        // now safe to print
    putsUart0(" killed.\n\n");

}


void pkill(const char name[])
{
    //print the process name and killed. Example: Process Dog has been killed
    putsUart0(" Process ");
    putsUart0(name);
    putsUart0(" has been killed.\n\n");

    //turn of the red led if its on and turn on the blue led
    // setPinValue(RED_LED, 0);
    // waitMicrosecond(100000);
    // setPinValue(BLUE_LED, 1);
    // waitMicrosecond(100000);

}


void pi(bool on)
{
    if(!on)
    {
        putsUart0(" pi off\n\n");
    }
    else
    {
        putsUart0(" pi on\n\n");
    }
}


void preempt(bool on)
{
    if(!on)
    {
        putsUart0(" preempt off.\n\n");
    }
    else
    {
        putsUart0(" preempt on.\n\n");
    }
}


void sched(bool prio_on)
{
    if(!prio_on)
    {
        putsUart0(" sched rr\n\n");
    }
    else
    {
        putsUart0(" sched prio\n\n");
    }
}


void pidof(const char name[])
{
    //print the process name and that it has launched
    putsUart0(name);
    putsUart0(" Launched.\n\n");
}


void run()
{
    //turn on a red LED if the function is used

   // setPinValue(RED_LED, 1);
    // waitMicrosecond(100000);
    putsUart0(" hi\n\n");
}
void causeusage()
{
    // UsageFault: divide-by-zero (requires DIV0 trap)

        volatile uint32_t a = 1, b = 0;
        volatile uint32_t c = a / b;   // -> UFSR.DIVBYZERO
        (void)c;

}
void causebus()
{
    // BusFault: precise data bus error (unmapped address) -> BFSR.PRECISERR + BFARVALID

        volatile uint32_t *bad = (uint32_t*)0xA0000000; // clearly unmapped region
        *bad = 0xDEADBEEF;                               // -> BusFault


}
void causehard()
{
    // 1) Make sure configurable fault handlers are *disabled*
        NVIC_SYS_HND_CTRL_R &= ~(NVIC_SYS_HND_CTRL_MEM  |
                                 NVIC_SYS_HND_CTRL_BUS  |
                                 NVIC_SYS_HND_CTRL_USAGE);

        // 2) Cause a precise BusFault by writing to an unmapped address
        volatile uint32_t *bad = (uint32_t *)0xA0000000; // definitely invalid on TM4C
        *bad = 0xDEADBEEF;  // -> BusFault, but escalates to HardFault since handlers are disabled
}
void test2()
{

    void* ptr1 = malloc_heap(1024);
    void* ptr2 = malloc_heap(512);
    void* ptr3 = malloc_heap(1536);

    void* ptr4 = malloc_heap(1024);
    void* ptr5 = malloc_heap(1024);
    void* ptr6 = malloc_heap(1024);
////


    void* ptr7 = malloc_heap(1024);


    void* ptr8 = malloc_heap(512);
    void* ptr9 = malloc_heap(4096);
    free_heap(ptr2);
    free_heap(ptr5);

    setunpriv();


    uint32_t* ptr5_typed = (uint32_t*)ptr5;
     ptr5_typed[0] = 0x12345678;

}
void test1()
{
    putsUart0("\n=== MPU Malloc Test ===\n");

    // Allocate memory
    void* p1 = malloc_heap(1024);
    void* p2 = malloc_heap(3072);
    void* p3 = malloc_heap(8192);
    void* p4 = malloc_heap(8192);

//    char buffer[100];
//    sprintf(buffer, "p1=0x%08X p2=0x%08X\n", (uint32_t)p1, (uint32_t)p2);
//    putsUart0(buffer);
//    sprintf(buffer, "p3=0x%08X p4=0x%08X\n", (uint32_t)p3, (uint32_t)p4);
//    putsUart0(buffer);
//
//    putsUart0("\n--- After allocations ---\n");
//    debugMpuAndMalloc();

//    // Free p4
//    putsUart0("\n--- Freeing p4 ---\n");
      free_heap(p4);
//    debugMpuAndMalloc();

    // Switch to unprivileged
//    putsUart0("\n--- Switching to unprivileged mode ---\n");
    setunpriv();

    uint32_t* ptr4 = (uint32_t*)p4;

   // putsUart0("Attempting write to freed p4...\n");
    ptr4[0] = 0xDEADBEEF;  // Should FAULT!

    //putsUart0("ERROR: Write succeeded! MPU not blocking freed memory!\n");
}

