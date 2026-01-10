//Aisosa Okunbor
#ifndef MEMORYHANDLER_H_
#define MEMORYHANDLER_H_

#include <stdint.h>
#include <stddef.h>


#define MAX_ALLOCATIONS 28

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

// Add to memoryhandler.h

void   standard_size(uint32_t* size);
void  *malloc_heap(uint32_t size_in_bytes);     // returns 1024-aligned block or NULL
void   free_heap(void *p);                 // safe for NULL
void   updateMpuFromMalloc(void);
void   debugMpuAndMalloc(void);
#endif
