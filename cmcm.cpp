#include <string.h>

#include "cmcm.h"

typedef struct {
    void* sp;
    uint8_t flags;
} cmcm_task_t;

#define CMCM_TASK_INUSE   (1 << 0)
#define CMCM_TASK_PAUSED  (1 << 1)

// interrupt control and state register
#define ICSR (*(volatile uint32_t*)0xE000ED04)

static cmcm_task_t tasks[CMCM_MAX_NUM_TASKS];
static uint8_t __attribute__((aligned(8))) stack_space[CMCM_STACK_SIZE * CMCM_MAX_NUM_TASKS];

// the first context switch (from MSP to PSP) will increment this
static int current_task = -1;

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
    } sw_frame;

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
    } hw_frame;
} cmcm_stack_frame_t;

namespace cmcm {

static void destroy_task(void)
{
    tasks[current_task].flags = 0;
    yield();
    while (1);
}

void create_task(void (*handler)(void))
{
    // find first available slot
    int index;
    for (index = 0; tasks[index].flags & CMCM_TASK_INUSE && index < CMCM_MAX_NUM_TASKS; index++);
    if (index >= CMCM_MAX_NUM_TASKS) {
        return;
    }

    void* stack = stack_space + (index * CMCM_STACK_SIZE);
    memset(stack, 0, CMCM_STACK_SIZE);

    // set sp to the start of the stack (highest address)
    tasks[index].sp = ((uint8_t*)stack) + CMCM_STACK_SIZE;

    // initialize the start of the stack as if it had been
    // pushed via a context switch

    // tasks[index].sp -= sizeof(cmcm_stack_frame_t);
    tasks[index].sp = (cmcm_stack_frame_t*)(tasks[index].sp) - 1;

    cmcm_stack_frame_t* frame = (cmcm_stack_frame_t*)tasks[index].sp;

    frame->hw_frame.lr = (void*)destroy_task;
    frame->hw_frame.pc = (void*)handler;
    frame->hw_frame.psr = 0x21000000; // default PSR value

    tasks[index].flags |= CMCM_TASK_INUSE;
}

int get_current_task(void)
{
    return current_task;
}

void sleep(uint32_t ticks)
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

static void* push_context(void)
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

static void pop_context(void* psp)
{
    // loads registers with contents of the PSP stack
    // additional registers are popped in hardware
    __asm__("LDMFD %0!, {r4-r11}\n"
            "MSR psp, %0\n"
            :
            : "r"(psp));
}

void context_switch()
{
    // the first context switch will be called from the MSP
    // in that case we do not need to save the context
    // since we never return to MSP
    if (current_task != -1) {
        tasks[current_task].sp = push_context();
    }

    // find next running task
    uint8_t running;
    do {
        current_task++;
        if (current_task >= CMCM_MAX_NUM_TASKS) {
            current_task = 0;
        }

        uint8_t flags = tasks[current_task].flags;
        running = (flags & CMCM_TASK_INUSE) & !(flags & CMCM_TASK_PAUSED);
    } while (!running);

    pop_context(tasks[current_task].sp);

    __asm__("bx %0"
            :
            : "r"(0xFFFFFFFD));
}

void yield(void)
{
    // manually trigger pend_sv
    ICSR |= (1 << 28);

    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
}

void pause()
{
    tasks[current_task].flags |= CMCM_TASK_PAUSED;
    yield();
}

void resume(int task_id)
{
    // critial section, could be called from any task
    disable_interrupts();
    tasks[task_id].flags &= ~CMCM_TASK_PAUSED;
    enable_interrupts();
}

void disable_interrupts(void)
{
    __asm__("CPSID i");
}

void enable_interrupts(void)
{
    __asm__("CPSIE i");
}

}