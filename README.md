# RTOS

In this project, I built a Real-Time Operating System (RTOS) for the TIVA C TM4C123GXL Microcontroller.

Before the final build, I completed two smaller projects:

* The Nano project
* The Mini project 

## Overview
### Nano Project
The nano project focused on verifying the shell and basic command handling:

* Ensures the shell prints correctly to the terminal.
* Confirms commands respond appropriately to user input.

### Mini Project
The mini project focused on memory management and fault handling:

* Implemented a custom memory manager and Memory Protection Unit(MPU).
* Created a custom `malloc` and `free`
* built a heap implementation
* catch and report:
    - Memory Management Faults
    - Bus Faults
    - Usage Faults
    - Hard Faults
Including printing the offending instruction and fault dump to the screen.



### Shell Directions


```txt
help                       - Print available commands
reboot                     - Reboot the microcontroller
ps                         - Display running processes (threads)
ipcs                       - Display IPC (thread) communication status
kill <pid>                 - Kill the process (thread) with the given PID
pkill <proc_name>          - Kill the thread by process name
pi ON|OFF                  - Enable/disable priority inheritance
preempt ON|OFF             - Enable/disable preemption
sched PRIO|RR              - Select priority or round-robin scheduling
pidof <proc_name>          - Display the PID of a process (thread)
restart <proc_name>        - Restart a thread so it can be scheduled again
run <proc_name>            - Run a program in the background
setprio <proc_name> <prio> - Set thread priority (0–7, where 0 is highest)

* Note: Additional commands were added in the full RTOS build that may not exist in the Nano project. *
```
## Learning Experiences
Scheduling and Tasks:
* Designed a priority and round-robin scheduler
* Created tasks and allocated memory
* Killed tasks safely
* Learned and implemented task states while monitoring their transitions

Inter-task Communication:
* Passed data through queues by value
* Passed data through queues by reference
* Implemented direct task notifications

Synchronization and Data Protection:
* Implemented and used semaphores and mutexes
* Created ways to avoid race conditions
* Implemented and used software timers

Performance:
* Calculated per-task CPU usage(%)





