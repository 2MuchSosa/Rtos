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



void reboot()
{
    putsUart0("  REBOOTING.................\n\n");
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


