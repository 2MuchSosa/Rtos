// Aisosa Okunbor
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//RTOS SHELL INTERFACE

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

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




// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

#define MAX_PROCESSES 10
#define NAME_LEN 16


/*Updates needed:
  -Make the OS case insensitive
  -Make the OS not use string functions so we save space
 */

 //Hardware initalization
void initHW()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
     enablePort(PORTF);
     _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
    enablePinPullup(PUSH_BUTTON);

}



void shell(void)
{

            USER_DATA data;
            //shell written in here

            //boolean signals for certain commands, on | priority  = true, off | RR = false.
            bool pi_sig = false;
            bool preempt_sig = false;
            bool sched_sig = false;
          while(true)
          {
          if(kbhitUart0())
          {

            getsUart0(&data);
            Lowercase(data.buffer);
            putsUart0("\n");
            parseFields(&data);
            // uint8_t i;
            // for (i = 0; i < data.fieldCount; i++)
            // {
            //     putcUart0(data.fieldType[i]);
            //     putcUart0('\t');
            //     putsUart0(&data.buffer[data.fieldPosition[i]]);
            //     putcUart0('\n');
            // }


            if(isCommand(&data,"reboot",0))
            {
                // int32_t num1 = getFieldInteger(&data, 1);
                // int32_t num2 = getFieldInteger(&data, 2);
                reboot();
            }
            else if(isCommand(&data, "ps", 0))
            {
                // char* str = getFieldString(&data, 1);
                // putsUart0("! ");
                // putsUart0(str);
                // putcUart0('\n');
                ps();
            }
            else if (isCommand(&data, "ipcs", 0))
            {
                ipcs();
            }
            else if (isCommand(&data, "kill", 1))
            {
                uint32_t pid = getFieldInteger(&data, 1);   // user typed: kill 32  -> str1 = "32"
                kill(pid);                               // call kill function with pid
            }
            else if (isCommand(&data, "pkill", 1))
            {
                char* str1 = getFieldString(&data, 1);
                Lowercase(str1);
                pkill(str1);
            }
            else if (isCommand(&data, "pi", 1))
            {
                char* str2= getFieldString(&data, 1);
                Lowercase(str2);
                if(strEqual(str2, "on"))
                {
                    pi_sig = true;
                    pi(pi_sig);

                }
                else if (strEqual(str2, "off"))
                {
                    pi_sig = false;
                    pi(pi_sig);
                }
                else
                {
                    putsUart0("  Invalid Use of priority inheritance(pi) command.\n\n");
                }
            }
            else if (isCommand(&data, "preempt", 1))
            {
                char* str3= getFieldString(&data, 1);
                Lowercase(str3);
                if(strEqual(str3, "on"))
                {
                    preempt_sig = true;
                    preempt(preempt_sig);

                }
                else if (strEqual(str3, "off"))
                {
                    preempt_sig = false;
                    preempt(preempt_sig);
                }
                else
                {
                        putsUart0("  Invalid Use of preemption(preempt) command.\n\n");
                }
            }
            else if (isCommand(&data, "sched", 1))
            {
                char* str4= getFieldString(&data, 1);
                Lowercase(str4);
                if (strEqual(str4, "prio"))
                {
                        sched_sig = true;
                        sched(sched_sig);
                }
                else if (strEqual(str4, "rr"))
                {
                        sched_sig = false;
                        sched(sched_sig);
                }
                else
                {
                        putsUart0("  Invalid Use of scheduling(sched) command.\n\n");
                }
            }
            else if  (isCommand(&data, "pidof", 1))
            {
                char* str5= getFieldString(&data, 1);
                Lowercase(str5);
                pidof(str5);
            }
            else if  (isCommand(&data, "run", 0))
            {
                setPinValue(RED_LED, 1);
                run();

            }
            else if (isCommand(&data, "help", 0))
            {
                putsUart0("Commands:\n");
                putsUart0("  reboot - Reboots uC.\n");
                putsUart0("  ps - Displays the process(thread).\n");
                putsUart0("  ipcs - Displays the inter-process(thread) communication status.\n");
                putsUart0("  kill pid - Kills the process(thread) with the matching PID.\n");
                putsUart0("  pkill proc_name - Kills the thread based on the process name.\n");
                putsUart0("  pi ON|OFF - Turns priority inheritance on or off.\n");
                putsUart0("  preempt ON|OFF - Turns preemption on or off.\n");
                putsUart0("  sched PRIO|RR - Selects priority or round-robin scheduling.\n");
                putsUart0("  pidof proc_name - Displays the PID of the process(thread).\n");
                putsUart0("  run proc_name - Runs the selected program in the background.\n\n");
            }
            else
             putsUart0("Invalid command\n\n");
          }
          }
}

int main()
{
    initHW();
    initUart0();
    setUart0BaudRate(115200,40e6);
    putsUart0("Enter 'Help' for understanding of each command this RTOS uses.\n\n");
    while(true)
    {
        shell();
    }
}
