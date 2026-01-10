// Memory manager functions
//Aisosa Okunbor

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef MM_H_
#define MM_H_

#include <stdint.h>
#include <stddef.h>

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
#define MAX_ALLOCATIONS 28
//extern uint64_t blob;
// struct for heap allocations
typedef struct
{
    uint32_t* start;        // starting address of allocation
    uint16_t  length;       // length of allocation
    uint8_t   pid;          // process id of owner
} allocation;


// External declarations
extern allocation memory_map[MAX_ALLOCATIONS];
extern uint8_t map_count;
extern uint32_t pid;
extern uint64_t subregions;  // Make subregions accessible

//malloc and free 
//uint64_t tcbsrds (uint64_t tcbsrd);
void free_pid(void *pid);
void *malloc_heap(uint32_t size_in_bytes);
void free_heap(void *address_from_malloc);

//MPU
void initMpu(void);
void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
uint64_t createNoSramAccessMask(void);
void applySramAccessMask(uint64_t srdBitMask);
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes);
void initMemoryManager(void);

#endif
