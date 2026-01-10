// Shell functions
//Aisosa Okunbor

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef SHELL_H_
#define SHELL_H_

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
#include "kernel.h"

// for PS
typedef struct
{
    uint32_t pid;
    char name[16];
    uint8_t state;
    uint32_t sleep;   // if STATE_DELAYED
    uint32_t ticks;   // we can only reuse tcb[i].ticks
    uint32_t cpuTimeScaled; // holds cpuTime * 100
    uint8_t totalCpuTime;      // will be 0 unless you add accounting
} PS;

typedef struct
{
    uint8_t count;
    PS row[MAX_TASKS];
} PS_PRINT;

static PS ps_screen;

// for IPCS
typedef struct
{
    uint8_t id;
    uint32_t ownerPid;   // 0 if none
    uint8_t qSize;
    uint32_t qPid[MAX_MUTEX_QUEUE_SIZE];
} MUTEX_PRINT;

typedef struct
{
    uint8_t id;
    uint8_t count;
    uint8_t qSize;
    uint32_t qPid[MAX_SEMAPHORE_QUEUE_SIZE];
} SEMA_PRINT;

typedef struct
{
    MUTEX_PRINT mutex[MAX_MUTEXES];
    SEMA_PRINT sema[MAX_SEMAPHORES];
} IPCS_PRINT;

static IPCS_PRINT ipcs_screen;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Pins
#define RED_LED PORTF,1
#define BLUE_LED PORTF,2
#define GREEN_LED PORTF,3
#define PUSH_BUTTON PORTF,4

#define MAX_CHARS 80
#define MAX_FIELDS 5
typedef struct _USER_DATA
{
char buffer[MAX_CHARS+1];
uint8_t fieldCount;
uint8_t fieldPosition[MAX_FIELDS];
char fieldType[MAX_FIELDS];
} USER_DATA;

//String functions
void getsUart0(USER_DATA *data);
void parseFields(USER_DATA *data);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
bool strEqual(const char *s1, const char *s2);
void Lowercase(char str[]);
void uitoa(uint32_t num, char str[]);
void uintToStringHex(char *buffer, uint32_t value);

//Commands
void reboot();
void run();
void ps();
void ipcs();
void kill(uint32_t pid);
void pkill(const char name[]);
void pi( bool on);
void preempt(bool on);
void sched(bool pri_on);
void pidof(const char name[]);
void restart(const char name[]);
void setprio(const char name[], uint8_t priority);
void causebus();
void causehard();
void causeusage();
void test1();
void test2();

//shell function
//void initHw();
void shell(void);
#endif
