#ifndef ACCESSHANDLER_H_
#define ACCESSHANDLER_H_

#include <stdint.h>

void enablempu(void);
void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
uint64_t createNoSramAccessMask(void);
void applySramAccessMask(uint64_t srdBitMask);
void addSramAccessWindow(uint64_t *srdBitMask, uint32_t *baseAdd, uint32_t size_in_bytes);



#endif
