// Tasks
//Aisosa Okunbor
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "wait.h"
#include "kernel.h"
#include "shell.h"
#include "tasks.h"

#define BLUE_LED   PORTF,2 // on-board blue LED
#define RED_LED    PORTE,0 // off-board red LED
#define PB6        PORTE,1 // off-board Push-button
#define PB5        PORTE,2 // off-board Push-button
#define PB4        PORTE,3 // off-board Push-button
#define ORANGE_LED PORTA,2 // off-board orange LED
#define YELLOW_LED PORTA,3 // off-board yellow LED
#define GREEN_LED  PORTA,4 // off-board green LED
#define PB1        PORTA,5 // off-board Push-button
#define PB2        PORTA,6 // off-board Push-button
#define PB3        PORTA,7 // off-board Push-button

#define b1_mask 32
#define b2_mask 64
#define b3_mask 128
#define b4_mask 8
#define b5_mask 4
#define b6_mask 2

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
// Added initialization for blue, orange, red, green, and yellow LEDs
// Added initialization for 6 pushbuttons
void initHw(void)
{
    // Setup LEDs and pushbuttons
    enablePort(PORTA);
    enablePort(PORTE);
    enablePort(PORTF);

    _delay_cycles(3);

   selectPinPushPullOutput(BLUE_LED);
   selectPinPushPullOutput(RED_LED);
   selectPinPushPullOutput(ORANGE_LED);
   selectPinPushPullOutput(YELLOW_LED);
   selectPinPushPullOutput(GREEN_LED);
   selectPinDigitalInput(PB1);
   selectPinDigitalInput(PB2);
   selectPinDigitalInput(PB3);
   selectPinDigitalInput(PB4);
   selectPinDigitalInput(PB5);
   selectPinDigitalInput(PB6);
   enablePinPullup(PB1);
   enablePinPullup(PB2);
   enablePinPullup(PB3);
   enablePinPullup(PB4);
   enablePinPullup(PB5);
   enablePinPullup(PB6);

    // Power-up flash
    setPinValue(GREEN_LED, 1);
    waitMicrosecond(250000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(250000);
}

// added code to return a value from 0-63 indicating which of 6 PBs are pressed
uint8_t readPbs(void)
{
    uint8_t buttons = 0x00;

   if(!(getPinValue(PB1)))
   {
       buttons |=  0x01; // returns 0b0000 0001  = 1
   }

   if(!(getPinValue(PB2)))
   {
       buttons |=  0x02; // returns 0b0000 0010  = 2
   }
   if(!(getPinValue(PB3)))
   {
       buttons |= 0x04;  // returns 0b0000 0100  = 4
   }
   if(!(getPinValue(PB4)))
   {
       buttons |= 0x08; // returns 0b0000 1000  = 8
   }

   if(!(getPinValue(PB5)))
   {
       buttons |= 0x10; // returns 0b0001 0000  = 16
   }
   if(!(getPinValue(PB6)))
   {
       buttons |= 0x20;// returns 0b0010 0000  = 32
   }
   return buttons;
}

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle(void)
{
    while(true)
    {
        setPinValue(ORANGE_LED, 1);
        waitMicrosecond(1000);
        setPinValue(ORANGE_LED, 0);
        yield();
    }
}

void flash4Hz(void)
{
    while(true)
    {
        setPinValue(GREEN_LED, !getPinValue(GREEN_LED));
        sleep(125);
    }
}

void oneshot(void)
{
    while(true)
    {
        wait(flashReq);
        setPinValue(YELLOW_LED, 1);
        sleep(1000);
        setPinValue(YELLOW_LED, 0);
    }
}

void partOfLengthyFn(void)
{
    // represent some lengthy operation
    waitMicrosecond(990);
    // give another process a chance to run
    yield();
}

void lengthyFn(void)
{
    uint16_t i;
    while(true)
    {
        lock(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
        }
        setPinValue(RED_LED, !getPinValue(RED_LED));
        unlock(resource);
    }
}

void readKeys(void)
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            setPinValue(YELLOW_LED, !getPinValue(YELLOW_LED));
            setPinValue(RED_LED, 1);
        }
        if ((buttons & 2) != 0)
        {
            post(flashReq);
            setPinValue(RED_LED, 0);
        }
        if ((buttons & 4) != 0)
        {
            //restartThread(flash4Hz);
            restart("flash4hz");
            
        }
        if ((buttons & 8) != 0)
        {
             pkill("flash4hz");
            //killThread(flash4Hz);
            
            
           
        }
        if ((buttons & 16) != 0)
        {
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce(void)
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--;
            else
                count = 10;
        }
        post(keyReleased);
    }
}

void uncooperative(void)
{
    while(true)
    {
        while (readPbs() == 8)
        {
        }
        yield();
    }
}

void errant(void)
{
    uint32_t* p = (uint32_t*)0x20000000;
    while(true)
    {
        while (readPbs() == 32)
        {
            *p = 0;
        }
        yield();
    }
}

void important(void)
{
    while(true)
    {
        lock(resource);
        setPinValue(BLUE_LED, 1);
        sleep(1000);
        setPinValue(BLUE_LED, 0);
        unlock(resource);
    }
}
