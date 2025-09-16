#ifndef  STRINGHANDLER_H_
#define  STRINGHANDLER_H_

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

#define MAX_CHARS 80
#define MAX_FIELDS 5
typedef struct _USER_DATA
{
char buffer[MAX_CHARS+1];
uint8_t fieldCount;
uint8_t fieldPosition[MAX_FIELDS];
char fieldType[MAX_FIELDS];
} USER_DATA;

void getsUart0(USER_DATA *data);
void parseFields(USER_DATA *data);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
bool strEqual(const char *s1, const char *s2);
void Lowercase(char str[]);
void uitoa(uint32_t num, char str[]);

#endif
