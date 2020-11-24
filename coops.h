#ifndef COOPS_H
#define COOPS_H

#include <stdint.h>

// stacks for all tasks are allocated statically so tune
// these values so as to not needlessly waste RAM
#ifndef COOPS_MAX_NUM_TASKS
#define COOPS_MAX_NUM_TASKS 8
#endif

#ifndef COOPS_STACK_SIZE
#define COOPS_STACK_SIZE 2048
#endif

class CooperativeScheduler {

public:
    static CooperativeScheduler& instance();

    CooperativeScheduler(const CooperativeScheduler&) = delete;
    CooperativeScheduler(CooperativeScheduler&&) = delete;
    CooperativeScheduler& operator=(const CooperativeScheduler&) = delete;
    CooperativeScheduler& operator=(CooperativeScheduler&&) = delete;

    void createTask(void (*handler)(void));
    int getCurrentTask(void);
    void contextSwitch(void);
    void yield(void);
    void sleep(uint32_t ticks);
    void pause(void);
    void resume(int task_id);

private:
    CooperativeScheduler();

    void disableInterrupts(void);
    void enableInterrupts(void);

    int currentTask = -1;
};

extern CooperativeScheduler& coops;

#endif // COOPS_H
