// Memory Library
//Aisosa Okunbor
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// 16 MHz external crystal oscillator

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "mm.h"
#include "tm4c123gh6pm.h"
#include "faults.h"
#include "uart0.h"
#include "kernel.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern uint32_t heap[7168];  // FIXED: 7168 uint32_t = 28KB (not 28*1024)
struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;                     // used to uniquely identify thread (add of task fn)
    void *sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint64_t srd;                  // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    uint8_t mutex;                 // index of the mutex in use or blocking the thread
    uint8_t semaphore;             // index of the semaphore that is blocking the thread
    uint32_t size;
    uint32_t cpuTime;
    uint32_t pingpong;


} extern tcb[MAX_TASKS];
extern uint8_t taskCurrent;

allocation memory_map[MAX_ALLOCATIONS] = {0};
uint8_t map_count = 0;

// Total: 28 subregions (bits 0-27 all available for allocation)
// Each subregion is 1024B, so 28 subregions = 28KB available
// Heap starts at 0x20001000
//36bits
uint64_t subregions = 0x00000000FFFFFFFF;  //bits 0-27 set to 1 (all available)

//-----------------------------------------------------------------------------
// Memory Map
//-----------------------------------------------------------------------------

// ------------------------------------------------------------------------
// HEAP VISUALIZATION (28KB available for malloc)
// ------------------------------------------------------------------------
// 0x2000 8000  |----------------|
//              |                |
//              | 8kB - Region 3 |  8 subregions of 1024B each  - bits 20-27
// 0x2000 6000  |----------------|
//              |                |
//              | 8kB - Region 2 |  8 subregions of 1024B each  - bits 12-19
// 0x2000 4000  |----------------|
//              |                |  
//              | 8kB - Region 1 |  8 subregions of 1024B each  - bits 4-11
//              |                |  
// 0x2000 2000  |----------------|
//              |                |  
//              | 4kB - Region 0 |  4 subregions of 1024B each  - bits 0-3
// 0x2000 1000  |----------------|  <- heap base
//              | [4kB reserved] |
// 0x2000 0000  |----------------|

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
// Updates MPU SRD masks based on current malloc bitmap
//uint16_t mallocedsize (uint16_t tcbmalloc)
//{
//    return tcbmalloc;
//}

//This is for killthread from kenerl.c
//iterate through the memory manager and manually free any areas associated with that pid
void free_pid(void *pid)
{
    uint8_t i;

    //find the task index that matches this pid
    uint8_t taskIndex = 0xFF;
    for(i =0; i < MAX_TASKS; i++)
    {
        if(tcb[i].pid == pid)
        {
                taskIndex = i;
                break;
        }
    }


    if(taskIndex == 0xFF)
    {
        return;
    }

    //free all memory allocated by this task
    for(i = 0; i < MAX_ALLOCATIONS; i++)
    {
        if(memory_map[i].pid == taskIndex && memory_map[i].start != 0)
        {
            free_heap(memory_map[i].start);
        }
    }
}



// Round size up to nearest multiple of 1024B
void standardize_size(uint32_t* size)
{
    if (*size == 0) 
    {
        *size = 1024;  // minimum allocation
        return;
    }
    
    // Round up to nearest multiple of 1024
    // 0x3FF = 1023 (10 bits set)
    if (*size & 0x3FF)  // if not already a multiple of 1024
    {
        *size = (*size & ~0x3FF) + 1024;  // clear lower 10 bits and add 1024
    }
}

// Allocate Memory from Heap
void* malloc_heap(uint32_t size_in_bytes)
{
    // Check if memory map is full
    if (map_count >= MAX_ALLOCATIONS)
    {
        return (void*) 0;
    }
    
    // Check for valid size (max 28KB available)
    if (size_in_bytes == 0 || size_in_bytes > 28672)
    {
        return (void*) 0;
    }

    standardize_size(&size_in_bytes);
    
    // Calculate number of 1KB subregions needed
    uint64_t num_subregions = size_in_bytes >> 10;  // divide by 1024
    
    // Search for contiguous free subregions (bits 0-27)
    uint64_t start_index = 0;
    bool found = false;
    uint64_t i;
    uint64_t j;
    
    // FIXED: Start at bit 0, search up to bit 27
    for (i = 0; i <= (28 - num_subregions); i++)
    {
        // Check if we have num_subregions contiguous free subregions starting at i
        bool all_free = true;
        for (j = 0; j < num_subregions; j++)
        {
            if (!((subregions >> (i + j)) & 1))
            {
                all_free = false;
                break;
            }
        }
        
        if (all_free)
        {
            start_index = i;
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        return (void*) 0;  // no contiguous block found
    }
    
    // Mark subregions as allocated (clear bits)
    // after the 1st iteration we get subregions = 0x00000FFFFFFE
    uint64_t k;
    for (k = 0; k < num_subregions; k++)
    {
        subregions &= ~((uint64_t)1 << (start_index + k));
    }
    
    // Calculate address: heap base + offset
    // Each subregion is 1024B = 256 uint32_t words
    // FIXED: No subtraction needed since we start from bit 0
    uint32_t* address = heap + (start_index * 256);
    
    // init tcb srdbits
    //tcb[taskCurrent].srd = ~0;

    // Add entry to memory map
    uint8_t h;
    uint8_t size = size_in_bytes / 1024;
    for (h = 0; h < MAX_ALLOCATIONS; h++)
    {
        if (memory_map[h].start == 0)
        {
            for (i=0; i<size; i++) 
            {
                memory_map[h+i].start = address;
                memory_map[h+i].length = (uint16_t)size_in_bytes;
                memory_map[h+i].pid = taskCurrent;
                tcb[taskCurrent].srd &= ~((uint64_t)1 << (h+i));
                //new
                //tcb[taskCurrent].srd &= ~((uint64_t)1 << (start_index+i));
                map_count++;
            }

            addSramAccessWindow(&tcb[taskCurrent].srd, (uint32_t*)address, size_in_bytes);
            break;
        }
    }
    


    return (void*)address;
}

// Free Memory from Heap
void free_heap(void* p)
{
    if (p == 0)
    {
        return;  // null pointer, nothing to free
    }
    
    // Find allocation in memory map
    bool found = false;
    uint16_t size = 0;
    //uint8_t owner = 0xFF;
    uint8_t freedChunks = 0;
    uint8_t i;

    for (i = 0; i < MAX_ALLOCATIONS; i++)
    {
        if (memory_map[i].start == p)
        {
            if(!found)
            {
                size = memory_map[i].length;
                //owner = memory_map[i].pid;
                found = true;
            }
            // Clear memory map entry
            memory_map[i].start = 0;
            memory_map[i].length = 0;
            memory_map[i].pid = 0;
            //new
            freedChunks++;
            // map_count--;
            // found = true;
            break;
        }
    }


    
    if (!found)
    {
        return;  // address not found in memory map
    }
    


    //decrement by the number of chunks cleared ( not just 1)
    map_count -= freedChunks;
    // Calculate which subregions to free
    uint32_t offset = (uint32_t*)p - heap;  // offset in words from heap base
    uint8_t start_subregion = offset / 256;  // FIXED: No addition needed
    uint8_t num_subregions = size >> 10;  // divide by 1024
    
    // Mark subregions as free (set bits to 1)
    uint8_t x;
    for (x = 0; x < num_subregions; x++)
    {
        subregions |= ((uint64_t)1 << (start_subregion + x));

        //revokes access in current task's SRD mask
        tcb[taskCurrent].srd |= ((uint64_t) 1 << (start_subregion + x));
        
    }

}

//-----------------------------------------------------------------------------
// MPU Integration Helper
//-----------------------------------------------------------------------------


void initMpu(void)
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
    uint32_t srdMask = 0xFF00;
    uint32_t srdBits = (uint32_t)srdBitMask;

    // ----- Region 0 -----
    // SRD[3:0] must stay 1 (disable OS 4 KiB at 0x2000_0000..0x2000_0FFF)
    // SRD[7:4] come from bits 0..3 of m
    NVIC_MPU_NUMBER_R = 0;
    uint32_t attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
    NVIC_MPU_ATTR_R = attr | (((srdBits << 12) | 0xF00u) & srdMask);

    // ----- Region 1 -----
    // bits 4 -> 11  SRD[7:0]
    NVIC_MPU_NUMBER_R = 1;
    attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
    NVIC_MPU_ATTR_R = attr | ((srdBits << 4) & srdMask);

    // ----- Region 2 -----
    // bits 12 -> 19 SRD[7:0]
    NVIC_MPU_NUMBER_R = 2;
    attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
    NVIC_MPU_ATTR_R = attr | ((srdBits >> 4) & srdMask);

    // ----- Region 3 -----
    // bits 20 -> 27  SRD[7:0]
    NVIC_MPU_NUMBER_R = 3;
    attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
    NVIC_MPU_ATTR_R = attr | ((srdBits >> 12) & srdMask);

        // Region 3: Always fully enabled for stack
/*      NVIC_MPU_NUMBER_R = 3;
        attr = NVIC_MPU_ATTR_R & ~(0xFFu << 8);
        NVIC_MPU_ATTR_R = attr;  // SRD = 0x00 (all enabled)
*/
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
    // Heap bit 0 corresponds to 0x20000000 (first 1KB of heap)
    uint32_t start = ((uint32_t)baseAdd - 0x20000000) >> 10;                     // start subregion (find offset and divide by 1024)
    uint32_t end   = ((uint32_t)baseAdd - 0x20000000 + size_in_bytes - 1) >> 10; // end subregion

    // Clear bits to enable access (MPU convention: 0 = enabled)
    int i;
    for (i = start; i <= end; i++)
    {
        *srdBitMask &= ~((uint64_t) 1 << i); // Clear bit to ENABLE access (MPU: 0 = enabled)
        
    }
}

void initMemoryManager(void)
{

    allowFlashAccess();
    allowPeripheralAccess();
    setupSramAccess();
}
