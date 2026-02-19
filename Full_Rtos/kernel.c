// Kernel functions
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
#include "tm4c123gh6pm.h"
#include "shell.h"
#include "uart0.h"
#include "mm.h"
#include "kernel.h"
#include "asm.h"
#include "tasks.h"

// SVC codes
#define PENDSV  1
#define SLEEP   2
#define LOCK    3
#define UNLOCK  4
#define WAIT    5
#define POST    6
#define REBOOT  7
#define SCHED   8
#define PREEMPT 9
#define PIDOF   10
#define KILL    11
#define PKILL   12
#define RESTART 13
#define PS      14
#define IPCS    15
#define SETPRIO 16
//#define RUN     17
//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------
uint64_t srdMasks = 0;
// uint32_t lastTav = 0;
// mutex
typedef struct _mutex
{
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;
mutex mutexes[MAX_MUTEXES];

// semaphore
typedef struct _semaphore
{
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;
semaphore semaphores[MAX_SEMAPHORES];

// task states
#define STATE_INVALID           0 // no task
#define STATE_UNRUN             1 // task has never been run
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_SEMAPHORE 4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_MUTEX     5 // has run, but now blocked by mutex
#define STATE_KILLED            6 // task has been killed

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks

// control
bool priorityScheduler = true;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = false;          // preemption (true) or cooperative (false)

// tcb
#define NUM_PRIORITIES   8
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

} tcb[MAX_TASKS];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool initMutex(uint8_t mutex)
{
    bool ok = (mutex < MAX_MUTEXES);
    if (ok)
    {
        mutexes[mutex].lock = false;
        mutexes[mutex].lockedBy = 0;
    }
    return ok;
}

bool initSemaphore(uint8_t semaphore, uint8_t count)
{
    bool ok = (semaphore < MAX_SEMAPHORES);
    {
        semaphores[semaphore].count = count;
    }
    return ok;
}

//initialize systick for 1ms system timer
void initRtos(void)
{
    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
        tcb[i].sp = 0;
        tcb[i].priority = 0xFF;           // Initialize to invalid
        tcb[i].currentPriority = 0xFF;    // Initialize to invalid
        tcb[i].ticks = 0;
        tcb[i].srd = 0;
        tcb[i].mutex = 0xFF;
        tcb[i].semaphore = 0xFF;
        tcb[i].size = 0;
        tcb[i].name[0] = '\0';
    }

   // start sysTick timer
   NVIC_ST_CTRL_R = 0;                         // disable SysTick before setup
   //NVIC_ST_CTRL_R |= NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN; // set to run from sys clock and enable interrupts
   NVIC_ST_RELOAD_R = 40000 - 1;                   // set interval of 1ms
   NVIC_ST_CURRENT_R = 0;
   NVIC_ST_CTRL_R |= NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE;      // enable SysTick timer to begin running


   //enable wide timer 5 clock
   SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R5;
   _delay_cycles(3);

   //disable timer
   WTIMER5_CTL_R = 0;
   //64-bit timer
   WTIMER5_CFG_R |= 0x4;

   //one shot
   WTIMER5_TAMR_R |= TIMER_TBMR_TBMR_1_SHOT ;

   //TAILR_R handles  31:0
   //TAILR_R handle   63:32
   WTIMER5_TAILR_R = 0;


}

//Implemented prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void)
{
    bool ok;
    static uint8_t task = 0xFF;
    static uint8_t lastTaskAtPrio[NUM_PRIORITIES] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ok = false;

    if(priorityScheduler)
    {
        // Search from highest priority (0) to lowest (NUM_PRIORITIES-1)
        uint8_t prio;
        for(prio = 0; prio < NUM_PRIORITIES; prio++)
        {
            // Start searching after the last task run at this priority
            uint8_t searchStart;
            if(lastTaskAtPrio[prio] == 0xFF)
            {
                searchStart = 0;
            }else
            {
                searchStart = (lastTaskAtPrio[prio] + 1) % MAX_TASKS;
            }

            // Check all tasks at this priority level (round-robin within priority)
            uint8_t i;
            for(i = 0; i < MAX_TASKS; i++)
            {
                uint8_t taskIndex = (searchStart + i) % MAX_TASKS;

                // Skip invalid tasks
                if(tcb[taskIndex].state == STATE_INVALID || tcb[task].state == STATE_KILLED)
                {
                    continue;
                }
                // Check if task is runnable and at current priority level
                if((tcb[taskIndex].state == STATE_READY || tcb[taskIndex].state == STATE_UNRUN) &&
                   tcb[taskIndex].currentPriority == prio)
                {
                    // Found next runnable task at this priority
                    lastTaskAtPrio[prio] = taskIndex;
                    return taskIndex;
                }
            }
        }

        // No runnable task found - return idle task (usually index 0)
        return 0;
    }
    else
    {
        // Round-robin scheduling
        while (!ok)
        {
            task++;
            if (task >= MAX_TASKS)
                task = 0;

            // Skip invalid tasks
            if(tcb[task].state == STATE_INVALID || tcb[task].state == STATE_KILLED)
                continue;

            ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
        }
        return task;
    }
}

//modified this function to start the operating system
// by calling scheduler, set srd bits, setting PSP, ASP bit, call fn with fn add in R0
// fn set TMPL bit, and PC <= fn
void startRtos(void)
{
    taskCurrent = rtosScheduler();


    tcb[taskCurrent].state = STATE_READY;


    //start(void* sp, void* fn)
    setpsp(tcb[taskCurrent].sp);
    usepsp();
    applySramAccessMask(tcb[taskCurrent].srd);
    //clearunpriv();                      // switch to upriviledged mode

    // Jump to the selected task (switches to unprivileged and jumps to task function)
    startTask(tcb[taskCurrent].pid);

}


// add task if room in task list
// store the thread name
// allocate stack space and store top of stack in sp and spInit
// set the srd bits based on the memory allocation


bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes)
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
    //max taks is 12
    if (taskCount < MAX_TASKS)
    {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS))
        {
            found = (tcb[i++].pid ==  fn);
        }
        if (!found)
        {
            // find first available tcb record
            i = 0;
            while (tcb[i].state != STATE_INVALID) {i++;}


            //save current task and temporarily set to new task for malloc
            uint8_t savedTaskCurrent = taskCurrent;
            taskCurrent = i;
            tcb[i].srd = createNoSramAccessMask();
            void* stackBase = malloc_heap(stackBytes);



            taskCurrent = savedTaskCurrent;

            //tcb[i].initalsp = malloc_heap(stackBytes);
            if(stackBase == NULL)
            {
                return false;
            }
            //set the stackpointer of the thread
            tcb[i].sp = (void*) ((uint32_t)stackBase + stackBytes);

            //save the threads inital stack pointer
            //Calculate this value!!
            // Calculated initial stack pointer (base address of allocated memory)
            //tcb[i].initalsp = (void*)((uint32_t)tcb[i].sp - tcb[i].size);

            //tcb[i].srd = createNoSramAccessMask();
            //save the size of the thread onto our tcb
            tcb[i].size = stackBytes;
            //save the state of the thread onto our tcb
            tcb[i].state = STATE_UNRUN;
            //save the pid of the thread onto our tcb
            tcb[i].pid = fn;

            //set the stackpointer of the thread
            //tcb[i].sp = (void*) ((uint32_t) (tcb[i].sp) + (stackBytes));




            tcb[i].priority = priority;
            tcb[i].currentPriority = priority;
            tcb[i].ticks = 0;
            tcb[i].mutex = 0xFF;
            tcb[i].semaphore = 0xFF;
            tcb[i].cpuTime = 0;

             // Copy thread name
            uint8_t j;
            for(j = 0; j < 15 && name[j] != '\0'; j++)
            {
                tcb[i].name[j] = name[j];
            }
            tcb[i].name[j] = '\0';


            // increment task count
            taskCount++;
            ok = true;

        }
    }
    return ok;
}

// modified this function to kill a thread
//free memory, remove any pending semaphore waiting,
//unlock any mutexes, mark state as killed
void killThread(_fn fn)
{
                /* Our goal is to:
                    1. Mark the state as killed
                    2. Free the memory from the stack
                    3. Remove it from any semaphore queues
                    4. Unlock any held mutexes
                 */
    uint8_t i;

    //find the thread by its function pointer
    for(i = 0; i < MAX_TASKS; i++)
    {
        if(tcb[i].pid == fn && tcb[i].state != STATE_INVALID)
        {

            //free the stack memory
            if(tcb[i].sp != NULL)
            {
                free_pid(fn);
                void* stackBase = (void*) ((uint32_t) tcb[i].sp - tcb[i].size);
                free_heap(stackBase);

            }


            //mark it killed
            tcb[i].state = STATE_KILLED;

            //remove from any semaphore queues
            if(tcb[i].semaphore != 0xFF)
            {
                uint8_t sem = tcb[i].semaphore;

                uint8_t j;
                //find and remove from semaphore queue
                for(j = 0; j < semaphores[sem].queueSize; j++)
                {
                    if(semaphores[sem].processQueue[j] == i)
                    {
                        //shift remaining tasks up
                        uint8_t k;
                        for(k = j; k < semaphores[sem].queueSize -1; k++)
                        {
                            semaphores[sem].processQueue[k] = semaphores[sem].processQueue[k+1];

                        }
                        semaphores[sem].queueSize--;
                        break;
                    }
                }
                tcb[i].semaphore = 0xFF;
            }

            //unlock any held mutexes
            if(tcb[i].mutex != 0xFF)
            {
                 uint8_t mut = tcb[i].mutex;

                //Added a fix for if the current task is in the mutex wait queue
                uint8_t q;
                for(q = 0; q < mutexes[mut].queueSize; q++)
                {
                    if(mutexes[mut].processQueue[q] == i)
                    {
                        //shift the remaining tasks up
                        uint8_t k;
                        for( k = q; k < mutexes[mut].queueSize - 1; k++)
                        {
                            mutexes[mut].processQueue[k] = mutexes[mut].processQueue[k+1];
                        }
                            mutexes[mut].queueSize--;
                            break;
                    }
                }

                
                //adresses if the mutex is locked by the current task
                if(mutexes[mut].lockedBy == i)
                {
                    //restore priority if using priority inheritance
                    tcb[i].currentPriority = tcb[i].priority;

                    //give the mutex to the next task waiting in line
                    if(mutexes[mut].queueSize > 0)
                    {
                        mutexes[mut].lockedBy = mutexes[mut].processQueue[0];
                        tcb[mutexes[mut].processQueue[0]].state = STATE_READY;
                        tcb[mutexes[mut].processQueue[0]].mutex = mut;

                        //shift queue(move other up the line)

                        uint8_t k;
                        for(k = 0; k < mutexes[mut].queueSize - 1; k++)
                        {
                            mutexes[mut].processQueue[k] = mutexes[mut].processQueue[k+1];
                        }
                        mutexes[mut].queueSize--;
                    }else
                    {
                        //is no one else waiting? then unlock the mutex
                        mutexes[mut].lock = false;
                        mutexes[mut].lockedBy = 0xFF;
                    }
                }
            }

            //DO NOT DO THIS
            // //clear its spot in the TCB
            // tcb[i].state = STATE_INVALID;
            // tcb[i].pid = 0;
            // tcb[i].sp = 0;
            // tcb[i].initalsp = 0;
            // tcb[i].mutex = 0xFF;
            // tcb[i].semaphore = 0xFF;
            // taskCount--;


            //DO NOT HAVE TO DO THIS
            if(i == taskCurrent)
            {

                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
                return;
            }




           // break;
        }

    }
}


//modified this function to restart a thread, including creating a stack
void restartThread(_fn fn)
{
     __asm volatile (" SVC #13");     // supervisor call to trigger restart thread
}

//modified this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
    __asm volatile (" SVC #16");     // supervisor call to trigger
}

//modified this function to yield execution back to scheduler using pendsv
void yield(void)
{
   __asm volatile (" SVC #1");     // supervisor call to trigger pendSV

}

//modified this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm volatile (" SVC #2");     // supervisor call to trigger sleep
}

//modified this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{
   __asm volatile (" SVC #5");     // supervisor call to trigger wait
}

//modified this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm volatile (" SVC #6");     // supervisor call to trigger post
}

//modified this function to lock a mutex using pendsv
void lock(int8_t mutex)
{
    __asm volatile (" SVC #3");     // supervisor call to trigger lock
}

//modified this function to unlock a mutex using pendsv
void unlock(int8_t mutex)
{
     __asm volatile (" SVC #4");     // supervisor call to trigger unlock
}

//modified this function to add support for the system timer
//in preemptive mode, added code to request task switch
void systickIsr(void)
{
    uint8_t i;
    for(i = 0; i < MAX_TASKS; ++i)
    {
        //check if task has ticks remaining
        if(tcb[i].state == STATE_DELAYED && tcb[i].ticks > 0)
        {
            //decrement ticks and check if 0
            if(--tcb[i].ticks == 0)
            {
                //set to ready when ticks reaches 0
                    tcb[i].state = STATE_READY;

            }
        }
    }

    // Add preemption support
    if(preemption)
    {
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
    }
}

//in coop and preemptive, modified this function to add support for task switching
//process UNRUN and READY tasks differently
void pendSvIsr(void)
{
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_UNPEND_SV;
    //stop timer coming into pendsv
    WTIMER5_CTL_R = 0;
/////////////////////// CPU TIME CALCULATION/////////////////////////////
        //calculate time it took task to run

    //tcb[taskcurrent]. (useA?:BufferA:BufferB) += WTIMER5_TAV_R;
    //useA?: swap every X ms and zero buffer you are about to write to
    // set WTIMER5_TAV_R = 0;
    //then at the end of pendsvISR start counting again

    tcb[taskCurrent].cpuTime += WTIMER5_TAV_R;
    tcb[taskCurrent].pingpong += WTIMER5_TAV_R;



//////////////////////////////////////////////////////////////////////




    uint32_t cfsr = NVIC_FAULT_STAT_R;
    const uint32_t mpuMask = NVIC_FAULT_STAT_IERR | NVIC_FAULT_STAT_DERR;

    if ((cfsr & mpuMask) != 0)
    {
        // Clear the MemManage fault *status* bits (write-1-to-clear)
        NVIC_FAULT_STAT_R = mpuMask | NVIC_FAULT_STAT_MMARV;

        // Also clear the MemManage fault *pending* latch
        NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_MEMP;

        putsUart0(" - called from MPU\n");

        // signal task as cooked
        tcb[taskCurrent].state = STATE_KILLED;
    } else if (cfsr) {
        while (true);
    }

    if(tcb[taskCurrent].state == STATE_KILLED)
    {
            //select next task and we dont save the context of the killed task
            taskCurrent = rtosScheduler();

            applySramAccessMask(tcb[taskCurrent].srd);
            setpsp(tcb[taskCurrent].sp);
            swpop();
            return;

    }
    //setPinValue(RED_LED, 0);
    //push s/w regs -> from core regs onto psp
    swpush();

    tcb[taskCurrent].sp = getpsp();
    taskCurrent = rtosScheduler();

//    applySramAccessMask(tcb[taskCurrent].srd);
//    setpsp(tcb[taskCurrent].sp);
    if(tcb[taskCurrent].state == STATE_UNRUN)
    {
        /*
        Cortex-M4F Register Set (pg 75 of the datasheet)
        =======================

        Low registers (R0-R7):
            R0
            R1
            R2
            R3    General-purpose
            R4    registers
            R5
            R6
            R7

        High registers (R8-R12):
            R8
            R9    General-purpose
            R10   registers
            R11
            R12

        Special registers:
            SP  (R13)  - Stack Pointer (PSP/MSP) - Banked version of SP
            LR  (R14)  - Link Register
            PC  (R15)  - Program Counter

            xPSR        - Program Status Register
            PRIMASK
            FAULTMASK   Exception mask registers
            BASEPRI
            CONTROL    - Control register
         */

            //hardware push
            // is there a way to do this in pure assembl or is this a good method?
            uint32_t *p = tcb[taskCurrent].sp;
            p -=8; // decrementing our stack pointer
            p[7] = 0x01000000; // xPSR: set bit 24 for thumb mode
            p[6] = (uint32_t) tcb[taskCurrent].pid; // PC
            p[5] = 0x00000000; // LR
            p[4] = 0x0000000C; // R12
            p[3] = 0x00000003; // R3
            p[2] = 0x00000002; // R2
            p[1] = 0x00000001; // R1
            p[0] = 0x00000000; // R0
            tcb[taskCurrent].sp = p;

            //software push
            // is there a way to do this in pur assembly or is this a good method?
            p -=9; //decrement
            p[8] = 0x00000004; // R4
            p[7] = 0x00000005; // R5
            p[6] = 0x00000006; // R6
            p[5] = 0x00000007; // R7
            p[4] = 0x00000008; // R8
            p[3] = 0x00000009; // R9
            p[2] = 0x0000000A; // R10
            p[1] = 0x0000000B; // R11
            p[0] = 0xFFFFFFFD; // LR

            tcb[taskCurrent].state = STATE_READY;
    }


    
    applySramAccessMask(tcb[taskCurrent].srd);
    setpsp(tcb[taskCurrent].sp);

    //start timer
    WTIMER5_CTL_R |= 0x1;

    //pop from psp onto core regs

    swpop();

}


//modified this function to add support for the service call
//in preemptive code, add code to handle synchronization primitives
void svCallIsr(void)
{
    uint32_t *psp = getpsp();                           // get process stack pointer. Also R0
    // go to pc
    uint32_t pc = psp[6];                               // offset by 6 words to get PC
//    uint16_t svcInstruction = *((uint16_t*) (pc - 2));  // svcInstruction was 1 prior to current PC. 2-Byte instruction size for Thumb.
//    uint8_t  svcNumber = svcInstruction & 0xFF;         // SVC number is lower byte of instruction

    //after pc back up to get the svc number
    //add or edit code
    uint8_t *svcPtr = (uint8_t*)(pc - 2);               // Point to SVC instruction
    //uint8_t svcNumber = svcPtr[0];                      // Lower byte contains SVC number

    // SVC instruction format: [DF] [imm8] in little-endian
    //uint8_t opcode = svcPtr[1];      // Should be 0xDF
    uint8_t svcNumber = svcPtr[0];   // The immediate value



    switch(svcNumber)
    {
        case PENDSV:
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;   // set pendsv flag
            break;
        case SLEEP:
            tcb[taskCurrent].ticks = *psp;              // set ticks to sleep from R0
            tcb[taskCurrent].state = STATE_DELAYED;     // set state to delayed
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;   // set pendsv flag to task switch
            break;
        case LOCK:
            /* Take into account of priority inheritance for mutexes 11/17/2025*/
            // check if mutex is unlocked
            if (mutexes[*psp].lock == false)
            {
                    //if the mutex is unlocked and has no ownership then give the task ownership and lock the mutex
                    mutexes[*psp].lock = true;
                    mutexes[*psp].lockedBy = taskCurrent;
                    // Track which mutex this task holds,index of the mutex in use or blocking the thread
                    tcb[taskCurrent].mutex = *psp;

            }
            else
            {       //if the mutex is locked and and a task attempts to access it
                    // set the tasks state to blocked by mutex
                    if(priorityInheritance && tcb[taskCurrent].currentPriority < tcb[mutexes[*psp].lockedBy].currentPriority)
                    {
                        // Give the mutex holder the higher priority temporarily
                        tcb[mutexes[*psp].lockedBy].currentPriority = tcb[taskCurrent].currentPriority;
                    }
                    tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;
                    //set the tasks mutex index in tcb
                    tcb[taskCurrent].mutex = *psp;
                    // add the task to mutex queue and increment queue size
                    mutexes[*psp].processQueue[mutexes[*psp].queueSize++] = taskCurrent;
                    //task switch
                    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            }
            break;
        case UNLOCK:
            //check if the current thread holds the mutex
            if(mutexes[*psp].lockedBy == taskCurrent)
            {
                // Restore original priority (reverse priority inheritance)
                tcb[taskCurrent].currentPriority = tcb[taskCurrent].priority;

                //if so check if the mutex queue is not empty
                if(mutexes[*psp].queueSize)
                {
                    //if the mutex queue is not empty, then give ownership of the mutex to the next task in the queue and set its state to ready
                    mutexes[*psp].lockedBy = mutexes[*psp].processQueue[0];
                    tcb[mutexes[*psp].processQueue[0]].state = STATE_READY;
                    // Update which mutex it now holds
                    tcb[mutexes[*psp].processQueue[0]].mutex= *psp;

                    //move up the other tasks in the queue  and decrement the size of the queue
                    uint8_t i;
                    for(i = 0; i < mutexes[*psp].queueSize -1; ++i)
                    {
                        mutexes[*psp].processQueue[i] = mutexes[*psp].processQueue[i+1];
                    }
                    mutexes[*psp].processQueue[i] = 0xFF;
                    mutexes[*psp].queueSize--;
                }
                else
                 {   //mutex is unlocked and ready to be given ownership to a task if nothing is in the queue
                       mutexes[*psp].lock = false;
                       // Clear ownership
                       mutexes[*psp].lockedBy = 0xFF;
                       //Clear mutex tracking for current task index of the mutex because its not in use or blocking the thread
                       tcb[taskCurrent].mutex = 0xFF;
                 }
                 //Clear mutex tracking for current task index of the mutex because its not in use or blocking the thread
                 tcb[taskCurrent].mutex = 0xFF;
             }
             else
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; //task switch if the current thread does not hold ownership of the mutex

            break;
        case WAIT:
             if(semaphores[*psp].count > 0)
             {
                semaphores[*psp].count--;
             }
             else
             {
                semaphores[*psp].processQueue[semaphores[*psp].queueSize++] = taskCurrent;
                tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;
                tcb[taskCurrent].semaphore = *psp;
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;       // set pendsv flag
             }
            break;
        case POST:
             if(semaphores[*psp].queueSize)
             {
                tcb[semaphores[*psp].processQueue[0]].state = STATE_READY;
                uint8_t i;
                for(i = 0; i < semaphores[*psp].queueSize -1; ++i)
                {
                    semaphores[*psp].processQueue[i] = semaphores[*psp].processQueue[i+1];

                }
                semaphores[*psp].processQueue[i] = 0xFF;
                semaphores[*psp].queueSize--;
             }
             else
             {
                semaphores[*psp].count++;
             }
            break;
        case REBOOT:
             NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ; // enable writing to register and request a restart
            break;
        case SCHED:
            priorityScheduler = *psp;
            if(priorityScheduler)
            {
                putsUart0("Priority Schedule is Currently Active.\n");
            }else
                putsUart0("Round Robin is Currently Active.\n");
            break;
        case PREEMPT:
            preemption = *psp;
            if(preemption)
            {
                putsUart0("Preemption is Currently On.\n");
            }else
                putsUart0("Preemption is Currently Off.\n");
            break;
        case PIDOF:
        {
                //store the users  selected task name
                //R0 containse the pointer to the process name
                const char* processName = (const char*) psp[0];
                uint8_t i;
                bool foundProc = false;

                //Search through all tasks for a matching name
                for(i = 0; i<MAX_TASKS; i++)
                {
                    // force tcb name to lowercase so it matches your shell input
                    uint8_t k;
                    for (k = 0; tcb[i].name[k] != '\0'; k++)
                    {
                        if (tcb[i].name[k] >= 'A' && tcb[i].name[k] <= 'Z')
                            tcb[i].name[k] += 32;
                    }
                        //compare task name with requested name and check for an invalid state
                        if(tcb[i].state != STATE_INVALID && strEqual(tcb[i].name, processName))
                        {
                            //Found matching task? -> return its PID in R0
                            psp[0] = (uint32_t)tcb[i].pid;
                            foundProc= true;
                            break;

                        }

                }

                //Return Null if not found at all
                if(!foundProc)
                {
                    *psp = 0;
                }

            break;
        }
        case KILL:
        {
            // PID is passed in r0
            uint32_t pidToKill = psp[0];
            bool foundPID = false;
            uint8_t i;

            //search for task with matching PID
            for(i = 0; i < MAX_TASKS; i++)
            {
                if(tcb[i].state != STATE_INVALID && (uint32_t)tcb[i].pid == pidToKill && tcb[i].state != STATE_KILLED)
                {
                    //kill the thread
                    killThread((_fn)tcb[i].pid);
                    //reutrn a success signal
                    psp[0] = 1;
                    foundPID = true;
                    break;
                }
            }

            //return  failure if pid isnt found
            if(!foundPID)
            {
                psp[0] = 0;
            }


            break;
        }
        case PKILL:
        {

            const char* processName = (const char*)psp[0];
            bool foundPID = false;
            uint8_t i;

            //search for task with matching name
            for(i = 0; i<MAX_TASKS; i++)
            {
                //Convert task name to loawercase
                if(tcb[i].state != STATE_INVALID ||tcb[i].state != STATE_KILLED)
                {
                    char lowerName[16];
                    uint8_t k;
                    for(k = 0; k < 16 && tcb[i].name[k] != '\0'; k++)
                    {
                        if(tcb[i].name[k] >= 'A' && tcb[i].name[k] <= 'Z')
                        {
                            lowerName[k] = tcb[i].name[k] + 32;
                        }else
                        {
                            lowerName[k] = tcb[i].name[k];
                        }

                    }
                    lowerName[k] = '\0';

                    if(strEqual(lowerName, processName))
                    {
                        //kill the thread
                        killThread((_fn)tcb[i].pid);

                        //reutrn a success signal
                        psp[0] = 1;
                        foundPID = true;
                        break;
                    }

                }
            }
                //return failure if pid isnt found
                if(!foundPID)
                {
                psp[0] = 0;
                }


            break;
        }
        case RESTART:
        {
            //get the function pointer from R0
            _fn fn = (_fn)psp[0];
            uint8_t i;
            bool found = false;

            //Find the thread by its function pointer
            for(i = 0; i < MAX_TASKS; i++)
            {
                if(tcb[i].pid == fn && tcb[i].state == STATE_KILLED)
                {
                    found = true;

                    //save the stack size
                    uint32_t stackSize = tcb[i].size;

                    //save the current taskCurrent
                    uint8_t savedTaskCurrent = taskCurrent;
                    taskCurrent = i;

                    //reset SRDs
                    tcb[i].srd = createNoSramAccessMask();


                    //Allocate a new stack
                    void* stackBase = malloc_heap(stackSize);
                    if(stackBase == NULL)
                    {
                        tcb[i].state = STATE_INVALID;
                        taskCurrent = savedTaskCurrent;
                        psp[0] = 0;
                        break;
                    }

                    //calculate the top of stack
                    tcb[i].sp = (void*)((uint32_t)stackBase + stackSize);
                    //mark the task as unrun so scheduler can see it
                    tcb[i].state = STATE_UNRUN;
                    //Clear blocking states
                    tcb[i].mutex = 0xFF;
                    tcb[i].semaphore = 0xFF;
                    tcb[i].ticks = 0;
                    tcb[i].currentPriority = tcb[i].priority;
                    tcb[i].cpuTime = 0;

                    //restore taskCurrent
                    taskCurrent = savedTaskCurrent;
                    //return a sccess signal
                    psp[0] = 1;
                    break;

                }
            }

            if(!found)
            {
                //if no thread is found we return a failed signal
                psp[0] = 0;
            }

            break;
        }
        case PS:
        {
            PS_PRINT* ps_data = (PS_PRINT*)psp[0];
            uint8_t i;
            uint8_t count = 0;

            // Calculate the total CPU time for the CPU%
            uint32_t totalCpuTime = 0;
            for(i = 0; i < MAX_TASKS; i++)
            {
                if(tcb[i].state != STATE_INVALID)
                {
                    totalCpuTime += tcb[i].cpuTime;
                }
            }

            // Fill in the PS struct
            for(i = 0; i < MAX_TASKS; i++)
            {
                if(tcb[i].state != STATE_INVALID)
                {
                    ps_data->row[count].pid = (uint32_t)tcb[i].pid;

                    // Copy name
                    uint8_t j;
                    for(j = 0; j < 16 && tcb[i].name[j] != '\0'; j++)
                    {
                        ps_data->row[count].name[j] = tcb[i].name[j];
                    }
                    ps_data->row[count].name[j] = '\0';

                    ps_data->row[count].state = tcb[i].state;
                    ps_data->row[count].sleep = tcb[i].ticks;

                    // Scale by 100 for percentage calculation
                    // cpuTime * 100 using bit shifting: (64 + 32 + 4) = 100
                    uint32_t scaled = (tcb[i].cpuTime << 6) + (tcb[i].cpuTime << 5) + (tcb[i].cpuTime << 2);
                    ps_data->row[count].cpuTimeScaled = scaled;
                    ps_data->row[count].totalCpuTime = totalCpuTime;

                    count++;  // Always increment count for each valid task
                }
            }

            ps_data->count = count;
            break;
        }
        case IPCS:
        {
            IPCS_PRINT* ipcs_data = (IPCS_PRINT*)psp[0];
            uint8_t i,j;

            //fill mutex information
            for(i = 0; i < MAX_MUTEXES; i++)
            {
                ipcs_data->mutex[i].id = i;


                //get mutex owners PID
                if(mutexes[i].lock && mutexes[i].lockedBy < MAX_TASKS)
                {
                    ipcs_data->mutex[i].ownerPid = (uint32_t)tcb[mutexes[i].lockedBy].pid;
                }
                else
                {
                    //mutex unlocked? -> then 0
                    ipcs_data->mutex[i].ownerPid = 0;

                }

                //copy queue size and PIDs
                ipcs_data->mutex[i].qSize = mutexes[i].queueSize;
                for(j = 0; j < mutexes[i].queueSize && j < MAX_SEMAPHORE_QUEUE_SIZE; j++)
                {
                    uint8_t taskIndex = mutexes[i].processQueue[j];
                    ipcs_data->mutex[i].qPid[j] = (uint32_t)tcb[taskIndex].pid;
                }
            }

            //Fill semaphore information
            for(i = 0; i < MAX_SEMAPHORES; i++)
            {
                ipcs_data->sema[i].id = i;
                ipcs_data->sema[i].count = semaphores[i].count;
                ipcs_data->sema[i].qSize = semaphores[i].queueSize;

                //copy queue PIDs
                for(j = 0; j < semaphores[i].queueSize && j < MAX_SEMAPHORE_QUEUE_SIZE; j++)
                {
                    uint8_t taskIndex = semaphores[i].processQueue[j];
                    ipcs_data->sema[i].qPid[j] = (uint32_t)tcb[taskIndex].pid;
                }
            }
            break;
        }
        case SETPRIO:
        {
            //r0 hold the pid
            //r1 hold the priority

            _fn fn = (_fn)psp[0];
            uint8_t newPriority = psp[1];


            uint8_t i;
            bool found = false;

            //find the thread by its function pointer
            for(i = 0; i < MAX_TASKS; i++)
            {
                if(tcb[i].pid == fn && tcb[i].state)
                {
                    found = true;
                    //update the tasks base priority
                    tcb[i].priority = newPriority;
                    //update current priority base on priority inheritance
                    if(tcb[i].mutex == 0xFF || !priorityInheritance)
                    {
                        //not holding a mutex or Priority Inheritance is off
                        tcb[i].currentPriority = newPriority;
                    }else
                    {
                        // you are ehre because the thread is holding a mutex
                        //only update currentPriority if new priority is higher than the inherited priority
                        if(newPriority < tcb[i].currentPriority)
                        {
                            tcb[i].currentPriority = newPriority;
                        }

                    }

                    //if priority scheduling is enabled and we changed priority
                    //we trigger a context switch to re-evaluate scheduling
                    if(priorityScheduler)
                    {
                        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
                    }
                    break;
                }
            }
            break;
        }

    }

}


