#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <stdint.h>
#include <stdbool.h>


void reboot();
void ps();
void ipcs();
void kill(uint32_t pid);
void pkill(const char name[]);
void pi( bool on);
void preempt(bool on);
void sched(bool pri_on);
void pidof(const char name[]);
void run();

#endif


