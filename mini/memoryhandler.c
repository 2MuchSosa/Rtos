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

#include <stdint.h>
#include <stdbool.h>
#include "memoryhandler.h"
#include "accesshandler.h"
#include "tm4c123gh6pm.h"

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

extern uint32_t heap[7168];  // FIXED: 7168 uint32_t = 28KB (not 28*1024)

allocation memory_map[MAX_ALLOCATIONS] = {0};
uint8_t map_count = 0;
uint32_t pid = 0;

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
    
    // Add entry to memory map
    uint8_t h;
    for (h = 0; h < MAX_ALLOCATIONS; h++)
    {
        if (memory_map[h].start == 0)
        {
            memory_map[h].start = address;
            memory_map[h].length = (uint16_t)size_in_bytes;
            memory_map[h].pid = pid;
            map_count++;
            break;
        }
    }
    
    // Update MPU permissions to grant access to allocated memory
    updateMpuFromMalloc();

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
    uint8_t i;

    for (i = 0; i < MAX_ALLOCATIONS; i++)
    {
        if (memory_map[i].start == p)
        {
            size = memory_map[i].length;
            
            // Clear memory map entry
            memory_map[i].start = 0;
            memory_map[i].length = 0;
            memory_map[i].pid = 0;
            
            map_count--;
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        return;  // address not found in memory map
    }
    
    // Calculate which subregions to free
    uint32_t offset = (uint32_t*)p - heap;  // offset in words from heap base
    uint8_t start_subregion = offset / 256;  // FIXED: No addition needed
    uint8_t num_subregions = size >> 10;  // divide by 1024
    
    // Mark subregions as free (set bits to 1)
    uint8_t x;
    for (x = 0; x < num_subregions; x++)
    {
        subregions |= ((uint64_t)1 << (start_subregion + x));
    }

    // Update MPU permissions to revoke access to freed memory
    updateMpuFromMalloc();
}

//-----------------------------------------------------------------------------
// MPU Integration Helper
//-----------------------------------------------------------------------------

// Updates MPU SRD masks based on current malloc bitmap
void updateMpuFromMalloc(void)
{


    uint64_t mpuMask = subregions & 0x00FFFFFF;

    applySramAccessMask(mpuMask);
}
void debugMpuAndMalloc(void)
{
    extern uint64_t subregions;
    char buffer[100];

    // Show malloc bitmap
    sprintf(buffer, "subregions bitmap: 0x%08X\n", (uint32_t)subregions);
    putsUart0(buffer);

    // Show each region's SRD
    int i;
    for (i = 0; i < 4; i++)
    {
        NVIC_MPU_NUMBER_R = i;
        uint32_t base = NVIC_MPU_BASE_R;
        uint32_t attr = NVIC_MPU_ATTR_R;
        uint8_t srd = (attr >> 8) & 0xFF;

        sprintf(buffer, "Region %d: Base=0x%08X SRD=0x%02X ", i, base, srd);
        putsUart0(buffer);

        // Decode SRD bits
        putsUart0("(");
        int j;
        for (j = 0; j < 8; j++) {
            if (srd & (1 << j)) {
                putsUart0("D");  // Disabled
            } else {
                putsUart0("E");  // Enabled
            }
        }
        putsUart0(")\n");
    }
}
