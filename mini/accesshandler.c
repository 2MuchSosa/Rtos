// Memory allocation and permissions Library

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "accesshandler.h"
#include "faultsISR.h"
#include "uart0.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern uint32_t pid;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void enablempu(void)
{

    NVIC_MPU_CTRL_R = NVIC_MPU_CTRL_ENABLE | NVIC_MPU_CTRL_PRIVDEFEN;
}

void allowFlashAccess(void)
{
    // R5 = 0x00000000, 256kiB
    NVIC_MPU_NUMBER_R = 5;
    NVIC_MPU_BASE_R   = 0x00000000;
    NVIC_MPU_ATTR_R   = (1u<<0)        // ENABLE
                      | (17u<<1)       // SIZE=17 -> 256 KiB
                      | (0x06u<<24)    // AP=110 (Priv RO, Unpriv RO)  [or AP=001 to deny unpriv by default]
                      | (0u<<28);      // XN=0 (allow execute)

}

void allowPeripheralAccess(void)
{
    NVIC_MPU_NUMBER_R = 4;                          //Region Number
    NVIC_MPU_BASE_R = 0x40000000;                   //Base Address of Peripherals
    NVIC_MPU_ATTR_R   = (1u<<0)                     // ENABLE Region
                      | (25u<<1)                    // SIZE=25 -> 64 MiB
                      | (0x03u<<24)                 // AP=011 (Priv+Unpriv RW)
                      | (1u<<28);                   // XN=1
}

void setupSramAccess(void)
{
    uint32_t ATTR_MASK = NVIC_MPU_ATTR_ENABLE | (12 << 1) | (0x03 << 24) | (0xFF << 8); // enable, region size 8kb (2^13), RWX access, all subregions disabled

    int i;
    for (i = 0; i < 4; i++)
    {
        NVIC_MPU_NUMBER_R = i; // select region i
        NVIC_MPU_BASE_R = (0x20000000 + i * 0x2000); // base address increment by 8kB
        NVIC_MPU_ATTR_R = ATTR_MASK;
    }
}

uint64_t createNoSramAccessMask(void)
{
    // returns value of srd bits to disable access to all SRAM
    // 4 regions * 8 subregions = 32 bits to disable
    // MPU convention: 1 = disabled, 0 = enabled
    return 0x00000000FFFFFFFF;   // lower 32 bits indicate which MPU subregion rules are disabled
}

// applies the srdBitMask to all SRAM regions
// Input: srdBitMask in MPU convention (0 = enable access, 1 = disable access)
void applySramAccessMask(uint64_t srdBitMask)
{
        srdBitMask &= 0x00FFFFFF;
    // ----- Region 0 -----
        // SRD[3:0] must stay 1 (disable OS 4 KiB at 0x2000_0000..0x2000_0FFF)
        // SRD[7:4] come from bits 0..3 of m
        NVIC_MPU_NUMBER_R = 0;
        uint32_t attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
        uint8_t r0 = (uint8_t)(((srdBitMask & 0x0Fu) << 4) | 0x0Fu);
        NVIC_MPU_ATTR_R = attr | ((uint32_t)r0 << 8);

        // ----- Region 1 -----
        // bits 4 -> 11  SRD[7:0]
        NVIC_MPU_NUMBER_R = 1;
        attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
        uint8_t r1 = (uint8_t)((srdBitMask >> 4) & 0xFFu);
        NVIC_MPU_ATTR_R = attr | ((uint32_t)r1 << 8);

        // ----- Region 2 -----
        // bits 12 -> 19 SRD[7:0]
        NVIC_MPU_NUMBER_R = 2;
        attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
        uint8_t r2 = (uint8_t)((srdBitMask >> 12) & 0xFFu);
        NVIC_MPU_ATTR_R = attr | ((uint32_t)r2 << 8);

//        // ----- Region 3 -----
//        // bits 20 -> 27  SRD[7:0]
//        NVIC_MPU_NUMBER_R = 3;
//        attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
//        uint8_t r3 = (uint8_t)((srdBitMask >> 20) & 0xFFu);
//        NVIC_MPU_ATTR_R = attr | ((uint32_t)r3 << 8);

        // Region 3: Always fully enabled for stack
        NVIC_MPU_NUMBER_R = 3;
        attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
        NVIC_MPU_ATTR_R = attr;  // SRD = 0x00 (all enabled)



}

// adds access to the requested SRAM address range
// This function uses MPU convention: clears bits (0 = enabled) to grant access
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes)
{
    //Check if the size is a valid multiple of 1KB
    if (size_in_bytes % 1024 != 0)
    {
        putsUart0("Incorrect sram access window Size\n");
        return;
    }

    //Check if the address range of heap is 0x20001000-0x20008000
    if ((uint32_t)baseAdd < 0x20000000 || (uint32_t)baseAdd + size_in_bytes > 0x20008000)
    {
        putsUart0("sram access window: incorrect range\n");
        return;
    }

    // Calculate compact heap bitmap in dices (0-27)
    // Heap bit 0 corresponds to 0x20001000 (first 1KB of heap)
    uint32_t start = ((uint32_t)baseAdd - 0x20000000) >> 10;                     // start subregion (find offset and divide by 1024)
    uint32_t end   = ((uint32_t)baseAdd - 0x20000000 + size_in_bytes - 1) >> 10; // end subregion

    // Clear bits to enable access (MPU convention: 0 = enabled)
    int i;
    for (i = start; i <= end; i++)
    {
        *srdBitMask &= ~((uint64_t) 1 << i); // Clear bit to ENABLE access (MPU: 0 = enabled)
    }
}


