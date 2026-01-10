// Shell functions
//Aisosa Okunbor

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

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
#include "uart0.h"
#include "mm.h"
#include "faults.h"
#include "asm.h"
#include "shell.h"

#include "kernel.h"


// Blocking function that writes a serial character when the UART buffer is not full
 void putcUart0(char c);

// Blocking function that writes a string when the UART buffer is not full
 void putsUart0(char* str);

// Blocking function that returns with serial data once the buffer is not empty
extern char getcUart0();


void getsUart0(USER_DATA *data)
{
    // Step (a): Initialize a local variable, count, to zero.
    uint8_t count = 0;
    char c;
    while(true)
    {
    // Step (f): Loop until the function exits in (d) or (e).
       if(kbhitUart0())
       {
        // Step (b): Get a character from UART0.
        c = getcUart0();


        // Step (c): Handle backspace if count > 0.
        if ((c == 8 || c == 127) && count > 0)
        {
            count--;  // Decrement count to erase the last character.
        }
        // Step (d): If the character is a carriage return (Enter key), end the string.
        else if (c == 13)
        {
            data->buffer[count] = '\0'; // Null terminator to end the string.
            return;
        }
        // Step (e): Handle spaces and printable characters.
        else if (c >= 32)
        {
            data->buffer[count] = c; // Store the character and increment count.
            count++;

            // If the buffer is full, end the string.
            if (count == MAX_CHARS)
            {
                data->buffer[count] = '\0'; // Null terminator.
                return;
            }
        }
       } else 
       {
           yield();
       }
}
}

void parseFields(USER_DATA *data)
{
    uint8_t i;
    char c;
    data->fieldCount = 0;
    char previous = '\0';

     // parsing through the buffer
    for(i = 0; i < MAX_CHARS && data->buffer[i] != '\0' && data->fieldCount < MAX_FIELDS; i ++)
    {
        // set range of ASCII values that is requested from us in the lab document
        bool alpha = (data->buffer[i] > 64 && data->buffer[i] < 91) || (data->buffer[i] > 96 && data->buffer[i] < 123);
        bool numeric = (data->buffer[i] > 47 && data->buffer[i] < 58) || (data->buffer[i] > 44 && data->buffer[i] < 47);

        c = data->buffer[i];
        if(previous == '\0' && c != '\0')
        {
            // checking for letter
            if(alpha)
            {
                data->fieldPosition[data->fieldCount] = i;
                data->fieldType[data->fieldCount] = 'a';
                data->fieldCount++;
            }
            // checking for number
            else if(numeric)
            {
                data->fieldPosition[data->fieldCount] = i;
                data->fieldType[data->fieldCount] = 'n';
                data->fieldCount++;
            }
        }
        if(!(alpha || numeric))
        {
            data->buffer[i] = '\0';
            c = '\0';
        }
        previous = c;


    }
    return;

}
// function to return the value of a field requested if the field number is in range or NULL otherwise.
char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
    // Check if the field number is valid (within range)
    if (fieldNumber <= data->fieldCount)
    {
        return &data->buffer[data->fieldPosition[fieldNumber]];
    }else
    return NULL; // Return NULL if the field number is out of range
}

// function to return the integer value of the field if the field number is in range and the field type is numeric or 0 otherwise.
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber)
{
    // Check if fieldNumber is within range and if the field type is numeric
        if (fieldNumber > data->fieldCount || data->fieldType[fieldNumber] != 'n')
        {
            return 0; // Return NULL if fieldNumber is out of range or non numeric
        }
           char* str = getFieldString(data, fieldNumber);
           int32_t result = 0;
           bool isNegative = false;

           // Check if the number is negative
           if (*str == '-')
           {
               isNegative = true;
               str++; // Skip the negative sign
           }

           // Convert string to integer
           while (*str)
           {
               if (*str >= '0' && *str <= '9')
               {
                   result = result * 10 + (*str - '0');
               }
               else
               {
                   break; // Stop if a non-numeric character is found
               }
               str++;
           }

           // If the number was negative, return the negative value
           if (isNegative)
           {
               result = -result;
           }

           return result;
        /*else
        {
            //char* str = &data->buffer[data->fieldPosition[fieldNumber]];
            return atoi(&data->buffer[data->fieldPosition[fieldNumber]]);
        }*/
}



// function which returns true if the command matches the first field and the number of arguments (excluding the command field) is greater than or equal to the requested number of minimum arguments.
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
    // Checking if the first field matches the command and if there are enough arguments
    if (strcmp(&data->buffer[data->fieldPosition[0]], strCommand) == 0 && (minArguments < data->fieldCount ))
    {
        return true;
    }
    return false;
}

//compare strings
bool strEqual(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        if (*s1 != *s2)
        {
            return false;
        }
        s1++;
        s2++;
    }
    return *s1 == *s2; 
}

void Lowercase(char str[])
{
    int i = 0;
    while (str[i] != '\0')   // loop until null terminator
    {
        if (str[i] >= 'A' && str[i] <= 'Z')
        {
            str[i] = str[i] + 32;  // convert to lowercase
        }
        i++;
    }
}

void uitoa(uint32_t num, char str[])
{
    int i = 0;
    char temp[12]; // enough for 32-bit int
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (num > 0)
    {
        temp[i++] = (num % 10) + '0'; // take last digit
        num /= 10;
    }

    // reverse the string into output buffer
    int j = 0;
    while (i > 0)
    {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

int32_t atoi_simple(const char *str)
{
    int32_t result = 0;
    int sign = 1;

    // Skip leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
        str++;

    // Handle optional sign
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    // Convert digits
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}



void uintToStringHex(char *buffer, uint32_t value)
{
    static const char HEX[16] = "0123456789ABCDEF";

    buffer[0] = '0';
    buffer[1] = 'x';
    int bufIndex = 2;

    int i;
    for (i = 0 ; i < 8; i++)
    {
        if (i == 4)
            buffer[bufIndex++] = ' ';

        uint8_t nibble = (value >> ((7 - i) * 4)) & 0xF;
        buffer[bufIndex++] = HEX[nibble];
    }

    buffer[bufIndex] = '\0';
}

uint32_t parseHex(const char* str)
{
    uint32_t result = 0;
    
    // Skip leading whitespace
    while (*str == ' ' || *str == '\t')
        str++;
    
    // Skip "0x" or "0X" prefix if present
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
        str += 2;
    
    // Parse hex digits
    while (*str)
    {
        char c = *str;
        uint8_t digit;
        
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            break;  // Stop at first non-hex character
        
        result = (result << 4) | digit;
        str++;
    }
    
    return result;
}

// Modified getFieldHex function - add this to shell.c
uint32_t getFieldHex(USER_DATA* data, uint8_t fieldNumber)
{
    // Check if fieldNumber is within range
    if (fieldNumber > data->fieldCount)
    {
        return 0;
    }
    
    char* str = getFieldString(data, fieldNumber);
    if (str == NULL)
        return 0;
    
    return parseHex(str);
}
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------


//COMMANDS//
void reboot()
{

    __asm volatile (" SVC #7");
}


void ps()
{
    //svcps() -> calls ps svc call in kernel.c
    //update the struct here in shell
    //pass that updated struct to the svc call to print
    PS_PRINT ps_data;

    //call svc 13 to populate ps_data
    svcPs(&ps_data);

    //print header
    putsUart0("\n");
    putsUart0("Name            PID          State  Sleep(ms)  CPU%\n");
    putsUart0("------------------------------------------------------------\n");

    uint8_t i;
    for(i = 0; i < ps_data.count; i++)
    {
        char buffer[80];

        //print the task name 
        putsUart0(ps_data.row[i].name);
        uint8_t nameLen = 0;
        while(ps_data.row[i].name[nameLen] != '\0') nameLen++;
        uint8_t spaces = 16 - nameLen;
        while(spaces--)putcUart0(' ');


        //Print PID in hex
        uintToStringHex(buffer, ps_data.row[i].pid);
        putsUart0(buffer);
        putsUart0("  ");


        //Print state
        switch(ps_data.row[i].state)
        {
            case 1: putsUart0("UNRUN  "); break;
            case 2: putsUart0("READY  "); break;
            case 3: putsUart0("DELAYED"); break;
            case 4: putsUart0("BLK_SEM"); break;
            case 5: putsUart0("BLK_MTX"); break;
            case 6: putsUart0("KILLED "); break;
            default: putsUart0("INVALID"); break;
        }
        putsUart0("  ");

        //Print sleep ticks when in delayed state
        if(ps_data.row[i].state == 3)
        {
            uitoa(ps_data.row[i].sleep, buffer);
            putsUart0(buffer);
            uint8_t len = 0;
            while(buffer[len] != '\0') len++;
            spaces =  11 -len;
            while(spaces--)putcUart0(' ');
        }
        else
        {
            putsUart0("-          ");
        }

        // Calculate CPU percentage here in shell using regular division
        uint8_t percentage = 0;
        if(ps_data.row[i].totalCpuTime > 0)
        {
        // Simple division: (cpuTime * 100) / totalCpuTime
        percentage = (uint8_t)(ps_data.row[i].cpuTimeScaled / ps_data.row[i].totalCpuTime);

        // Clamp to 100% max (shouldn't happen, but safety check)
        if(percentage > 100)
        {
            percentage = 100;
        }
        }
        //print CPU percentage
        uitoa(percentage, buffer);
        putsUart0(buffer);
        putsUart0("%");


        putsUart0("\n");
    }
    putsUart0("\n");
}


void ipcs()
{
    //svcIpcs() -> calls Ipcs svc call in kernel.c
    //upadate the struct here in shell
    //pass that updated truct to the svc call to print
    IPCS_PRINT ipcs_data;
    svcIpcs(&ipcs_data);

    putsUart0("\n=== MUTEXES ===\n");
    uint8_t i, j;

    for(i = 0; i < MAX_MUTEXES; i++)
    {
        char buffer[80];
        putsUart0("Mutex");
        uitoa(i,buffer);
        putsUart0(buffer);
        putsUart0(":");

        //print the mutex owner
        if(ipcs_data.mutex[i].ownerPid != 0)
        {
            putsUart0("Owner=");
            uintToStringHex(buffer, ipcs_data.mutex[i].ownerPid);
            putsUart0(buffer);
        }
        else
        {
            putsUart0("Owner=NONE");
        }

        //print mutex queue
        putsUart0(", Queue=[");
        if(ipcs_data.mutex[i].qSize > 0)
        {
            for(j = 0; j < ipcs_data.mutex[i].qSize; j++)
            {
                if(j > 0) putsUart0(", ");
                uintToStringHex(buffer, ipcs_data.mutex[i].qPid[j]);
                putsUart0(buffer);
            }
        }else 
        {
            putsUart0("empty");
        }
        putsUart0("]\n");
    }
    
    putsUart0("\n=== SEMAPHORES ===\n");
    for(i = 0; i < MAX_SEMAPHORES; i++)
    {
        char buffer[80];
        
        putsUart0("Semaphore ");
        uitoa(i, buffer);
        putsUart0(buffer);
        putsUart0(": Count=");
        uitoa(ipcs_data.sema[i].count, buffer);
        putsUart0(buffer);
        
        // Print queue
        putsUart0(", Queue=[");
        if(ipcs_data.sema[i].qSize > 0)
        {
            for(j = 0; j < ipcs_data.sema[i].qSize; j++)
            {
                if(j > 0) putsUart0(", ");
                uintToStringHex(buffer, ipcs_data.sema[i].qPid[j]);
                putsUart0(buffer);
            }
        }
        else
        {
            putsUart0("empty");
        }
        putsUart0("]\n");
    }
    putsUart0("\n");
}




void kill(uint32_t pid)
{
    uint32_t result = svcKill(pid);


    if(result != 0)
    {
        char buffer[20];
        putsUart0("PID ");
        uintToStringHex(buffer, pid);
        putsUart0(buffer);
        putsUart0(" killed.\n\n");
    }
    else 
    {
        putsUart0("PID not found.\n\n");
    }

}


void pkill(const char name[])
{

    uint32_t result = svcPkill(name);

    if(result != 0)
    {
        //print the process name and killed. Example: Process Dog has been killed
        putsUart0(" Process ");
        putsUart0(name);
        putsUart0(" has been killed.\n\n");

    }else
    {
        //print the process name and killed. Example: Process Dog has been killed
        putsUart0(" Process ");
        putsUart0(name);
        putsUart0(" does not exist.\n\n");
    }


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
    __asm volatile (" SVC #9");
}


void sched(bool prio_on)
{
    __asm volatile (" SVC #8");
}


void pidof(const char name[])
{
    uint32_t pid = svcPidof(name);
    if(pid != 0)
    {
        char buffer[20];
        putsUart0("PID of ");
        putsUart0(name);
        uintToStringHex(buffer, pid);
        putsUart0(" ");
        putsUart0(buffer);
        putsUart0("\n\n");

    }else
    {
        putsUart0("Process ");
        putsUart0(name);
        putsUart0(" not found. \n\n");
    }

    
}

void restart(const char name[])
{
    uint32_t pid = svcPidof(name);

    if(pid == 0)
    {
        putsUart0("Process ");
        putsUart0(name);
        putsUart0(" not found.\n\n");
        return;

    }
    //call restart thread with the function pointer
    restartThread((_fn)pid);

        putsUart0("Process ");
        putsUart0(name);
        putsUart0(" restarted.\n\n");
}

void setprio(const char name[], uint8_t priority)
{
    //get the pid using pidof svc call
    uint32_t pid = svcPidof(name);

    if(pid == 0)
    {
        putsUart0("Process ");
        putsUart0(name);
        putsUart0(" not found.\n\n");
        return;
    }

     setThreadPriority((_fn)pid, priority);

    
}


void run(const char name[])
{
    uint32_t pid = svcPidof(name);

    if(pid == 0)
    {
        putsUart0("Process ");
        putsUart0(name);
        putsUart0(" not found.\n\n");
        return;
    }
    
    // Simply call restartThread instead of svcRun
    restartThread((_fn)pid);

    putsUart0("Process ");
    putsUart0(name);
    putsUart0(" restarted.\n\n");
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
//    free_heap(p4);
//    debugMpuAndMalloc();

    // Switch to unprivileged
//    putsUart0("\n--- Switching to unprivileged mode ---\n");
    setunpriv();

    uint32_t* ptr4 = (uint32_t*)p4;

   // putsUart0("Attempting write to freed p4...\n");
    ptr4[0] = 0xDEADBEEF;  // Should FAULT!

    //putsUart0("ERROR: Write succeeded! MPU not blocking freed memory!\n");
}


 //added processing for the shell commands through the UART here
void shell(void)
{ 

    USER_DATA data;
    //shell written in here
    putsUart0("Aisosa Okunbor's RTOS Project - 2025\n");
    //boolean signals for certain commands, on | priority  = true, off | RR = false.
    bool pi_sig = false;
    bool preempt_sig = false;
    bool sched_sig = false;
    while(!kbhitUart0())
    {
        yield();
    {

    getsUart0(&data);
    Lowercase(data.buffer);
    putsUart0("\n");
    parseFields(&data);



    if(isCommand(&data,"reboot",0))
    {

        reboot();
    }
    else if(isCommand(&data, "ps", 0))
    {

        ps();
    }
    else if (isCommand(&data, "ipcs", 0))
    {
        ipcs();
    }
// Replace the kill command handler in shell() function with this:

    else if (isCommand(&data, "kill", 1))
    {
    // Get the field as a string first to check format
    char* pidStr = getFieldString(&data, 1);
    uint32_t pid;

    // Check if it's hex format (starts with 0x or contains a-f/A-F)
    bool isHex = false;
    if (pidStr[0] == '0' && (pidStr[1] == 'x' || pidStr[1] == 'X'))
        isHex = true;
    else
    {
        // Check if string contains hex digits a-f
        uint8_t i = 0;
        while (pidStr[i] != '\0')
        {
            if ((pidStr[i] >= 'a' && pidStr[i] <= 'f') || 
                (pidStr[i] >= 'A' && pidStr[i] <= 'F'))
            {
                isHex = true;
                break;
            }
            i++;
        }
    }

    // Parse accordingly
    if (isHex)
        pid = getFieldHex(&data, 1);
    else
        pid = getFieldInteger(&data, 1);

    kill(pid);
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
    else if (isCommand(&data, "restart", 1))
    {
        char* str = getFieldString(&data, 1);
        Lowercase(str);
        restart(str);
    }
    else if (isCommand(&data, "setprio", 2))
    {
        char* processName = getFieldInteger(&data,1);
        int32_t priority = getFieldInteger(&data, 2);
        Lowercase(processName);

        //valid priority range
        if(priority < 0 || priority >= 8)
        {
            putsUart0("Invalid priority. Must be 0-7 (0=highest).\n\n");
        }
        else
        {
            setprio(processName, (uint8_t)priority);
        }

    }
    else if  (isCommand(&data, "run", 1))
    {
        char* str = getFieldString(&data, 1);
        Lowercase(str);
        run(str);

    }
    else if (isCommand(&data, "test", 1))
    {
        uint32_t pid = getFieldInteger(&data, 1);
        if(pid == 1)
        {
            test1();
        }
        else
         test2();

    }
    else if (isCommand(&data, "help", 0))
    {
        putsUart0("Commands:\n");
        putsUart0("  reboot - Reboots uC.\n");
        putsUart0("  ps - Displays the process(thread).\n");
        putsUart0("  ipcs - Displays the inter-process(thread) communication status.\n");
        putsUart0("  kill <pid> - Kills the process(thread) with the matching PID.\n");
        putsUart0("  pkill <proc_name> - Kills the thread based on the process name.\n");
        putsUart0("  pi ON|OFF - Turns priority inheritance on or off.\n");
        putsUart0("  preempt ON|OFF - Turns preemption on or off.\n");
        putsUart0("  sched PRIO|RR - Selects priority or round-robin scheduling.\n");
        putsUart0("  pidof <proc_name> - Displays the PID of the process(thread).\n");
        putsUart0("  restart <proc_name> - restarts thread to be caught by scheduler.\n");
        putsUart0("  run <proc_name> - Runs the selected program in the background.\n\n");
        putsUart0("  setprio <proc_name> <priority> - Sets thread priority (0-7, 0=highest).\n");
    }
    else
        putsUart0("Invalid command\n\n");
}
    }
}
