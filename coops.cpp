#include "coops.h"

#include <string.h>
#include "tick.h"
#include "stm32_def.h"

typedef struct {
    void* sp;
    uint8_t flags;
} task_t;

#define COOPS_TASK_INUSE   (1 << 0)
#define COOPS_TASK_PAUSED  (1 << 1)

static task_t tasks[COOPS_MAX_NUM_TASKS];
static uint8_t __attribute__((aligned(8))) stackSpace[COOPS_STACK_SIZE * COOPS_MAX_NUM_TASKS];

// the first context switch (from MSP to PSP) will increment this

typedef struct {
    // we explicity push these registers
    struct {
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t r8;
        uint32_t r9;
        uint32_t r10;
        uint32_t r11;
    } swFrame;

    // these registers are pushed by the hardware
    struct {
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r12;
        void* lr;
        void* pc;
        uint32_t psr;
    } hwFrame;
} stack_frame_t;

static void destroyTask(void)
{
    tasks[coops.getCurrentTask()].flags = 0;
    coops.yield();
    while (1);
}

static void* pushContext(void)
{
    void* psp;

    // copies registers to the PSP stack
    // additional registers are pushed in hardware
    __asm__("MRS %0, psp\n"
            "STMDB %0!, {r4-r11}\n"
            "MSR psp, %0\n"
            : "=r"(psp));

    return psp;
}

static void popContext(void* psp)
{
    // loads registers with contents of the PSP stack
    // additional registers are popped in hardware
    __asm__("LDMFD %0!, {r4-r11}\n"
            "MSR psp, %0\n"
            :
            : "r"(psp));
}

CooperativeScheduler& CooperativeScheduler::instance()
{
    static CooperativeScheduler instance;
    return instance;
}

void CooperativeScheduler::createTask(void (*handler)(void))
{
    // find first available slot
    int index;
    for (index = 0; tasks[index].flags & COOPS_TASK_INUSE && index < COOPS_MAX_NUM_TASKS; index++);
    if (index >= COOPS_MAX_NUM_TASKS) {
        return;
    }

    void* stack = stackSpace + (index * COOPS_STACK_SIZE);
    memset(stack, 0, COOPS_STACK_SIZE);

    // set sp to the start of the stack (highest address)
    tasks[index].sp = ((uint8_t*)stack) + COOPS_STACK_SIZE;

    // initialize the start of the stack as if it had been
    // pushed via a context switch

    // tasks[index].sp -= sizeof(stack_frame_t);
    tasks[index].sp = (stack_frame_t*)(tasks[index].sp) - 1;

    stack_frame_t* frame = (stack_frame_t*)tasks[index].sp;

    frame->hwFrame.lr = (void*)destroyTask;
    frame->hwFrame.pc = (void*)handler;
    frame->hwFrame.psr = 0x21000000; // default PSR value

    tasks[index].flags |= COOPS_TASK_INUSE;
}

int CooperativeScheduler::getCurrentTask(void)
{
    return currentTask;
}

void CooperativeScheduler::sleep(uint32_t ticks)
{
    uint32_t start = tick_get();

    while (1) {
        if (tick_since(start) >= ticks) {
            // enough time has passed
            break;
        }

        yield();
    }
}

void CooperativeScheduler::contextSwitch()
{
    // the first context switch will be called from the MSP
    // in that case we do not need to save the context
    // since we never return to MSP
    if (currentTask != -1) {
        tasks[currentTask].sp = pushContext();
    }

    // find next running task
    uint8_t running;
    do {
        currentTask++;
        if (currentTask >= COOPS_MAX_NUM_TASKS) {
            currentTask = 0;
        }

        uint8_t flags = tasks[currentTask].flags;
        running = (flags & COOPS_TASK_INUSE) & !(flags & COOPS_TASK_PAUSED);
    } while (!running);

    popContext(tasks[currentTask].sp);

    __asm__("bx %0"
            :
            : "r"(0xFFFFFFFD));
}

void CooperativeScheduler::yield(void)
{
    // manually trigger pend_sv
    // ICSR |= (1 << 28);
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;

    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
}

void CooperativeScheduler::pause()
{
    tasks[currentTask].flags |= COOPS_TASK_PAUSED;
    yield();
}

void CooperativeScheduler::resume(int task_id)
{
    // critial section, could be called from any task
    disableInterrupts();
    tasks[task_id].flags &= ~COOPS_TASK_PAUSED;
    enableInterrupts();
}

CooperativeScheduler::CooperativeScheduler()
{

}

void CooperativeScheduler::disableInterrupts(void)
{
    __asm__("CPSID i");
}

void CooperativeScheduler::enableInterrupts(void)
{
    __asm__("CPSIE i");
}

CooperativeScheduler& coops = CooperativeScheduler::instance();