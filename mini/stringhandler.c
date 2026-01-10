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


// Blocking function that writes a serial character when the UART buffer is not full
extern void putcUart0(char c);

// Blocking function that writes a string when the UART buffer is not full
extern void putsUart0(char* str);

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
