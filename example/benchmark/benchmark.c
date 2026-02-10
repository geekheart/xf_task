#include "benchmark_config.h"
#include "port.h"
#include "xf_task.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *mode;
    int task_target;
    int max_tasks;
    int created_tasks;
    int delay_ms;
    int work;
    int progress_ms;
    int duration_s;
    int busy_idle;
    int priority;
    int stack_size;
} bench_config_t;

static bench_config_t s_cfg = {
    .mode = BENCH_MODE_STR,
    .task_target = BENCH_TASKS,
    .max_tasks = BENCH_MAX_TASKS,
    .created_tasks = 0,
    .delay_ms = BENCH_DELAY_MS,
    .work = BENCH_WORK,
    .progress_ms = BENCH_PROGRESS_MS,
    .duration_s = BENCH_DURATION_S,
    .busy_idle = BENCH_BUSY_IDLE,
    .priority = BENCH_PRIORITY,
    .stack_size = BENCH_STACK_SIZE,
};

static volatile uint64_t s_exec_total = 0;
static volatile uint64_t s_sink = 0;
static uint64_t s_exec_last = 0;
static xf_task_time_t s_start_ms = 0;
static xf_task_time_t s_last_ms = 0;

static void bench_work(int work)
{
    for (int i = 0; i < work; i++) {
        s_sink += (uint64_t)i;
    }
}

#if BENCH_MODE == BENCH_MODE_NTASK
static void ntask_worker(xf_task_t task)
{
    XF_NTASK_BEGIN(task);

    while (1) {
        s_exec_total++;
        if (s_cfg.work > 0) {
            bench_work(s_cfg.work);
        }
        xf_ntask_delay((uint32_t)s_cfg.delay_ms);
    }

    XF_NTASK_END();
}

#elif BENCH_MODE == BENCH_MODE_CTASK
static void ctask_worker(xf_task_t task)
{
    (void)task;
    while (1) {
        s_exec_total++;
        if (s_cfg.work > 0) {
            bench_work(s_cfg.work);
        }
        xf_ctask_delay((uint32_t)s_cfg.delay_ms);
    }
}
#else
static void ttask_worker(xf_task_t task)
{
    (void)task;
    s_exec_total++;
    if (s_cfg.work > 0) {
        bench_work(s_cfg.work);
    }
}
#endif

static void progress_task(xf_task_t task)
{
    (void)task;
    xf_task_time_t now = task_get_tick();
    if (s_start_ms == 0) {
        s_start_ms = now;
        s_last_ms = now;
        s_exec_last = s_exec_total;
        return;
    }

    uint64_t dt_ms = (uint64_t)(now - s_last_ms);
    if (dt_ms == 0) {
        return;
    }

    uint64_t exec_delta = s_exec_total - s_exec_last;
    uint64_t exec_rate = (exec_delta * 1000ULL) / dt_ms;
    uint64_t per_task = 0;
    if (s_cfg.created_tasks > 0) {
        per_task = exec_rate / (uint64_t)s_cfg.created_tasks;
    }

    s_exec_last = s_exec_total;
    s_last_ms = now;

    printf("bench: mode=%s tasks=%d delay_ms=%d work=%d exec_rate=%llu/s per_task=%llu/s total=%llu\n", s_cfg.mode,
           s_cfg.created_tasks, s_cfg.delay_ms,
           s_cfg.work, (unsigned long long)exec_rate, (unsigned long long)per_task, (unsigned long long)s_exec_total);

    if (s_cfg.duration_s > 0) {
        uint64_t elapsed_ms = (uint64_t)(now - s_start_ms);
        if (elapsed_ms >= (uint64_t)s_cfg.duration_s * 1000ULL) {
            exit(0);
        }
    }
}

static void idle_no_sleep(unsigned long int max_idle_ms)
{
    (void)max_idle_ms;
}

static void bench_load_config(void)
{
    if (s_cfg.delay_ms <= 0) {
        s_cfg.delay_ms = 1;
    }
    if (s_cfg.task_target < 0) {
        s_cfg.task_target = 0;
    }
    if (s_cfg.max_tasks <= 0) {
        s_cfg.max_tasks = 1000;
    }
}

static void bench_create_tasks(void)
{
    int target = s_cfg.task_target;
    if (target == 0) {
        target = s_cfg.max_tasks;
    }

    for (int i = 0; i < target; i++) {
        xf_task_t task = NULL;
#if BENCH_MODE == BENCH_MODE_NTASK
        task = xf_ntask_create(ntask_worker, NULL, (uint16_t)s_cfg.priority);
#elif BENCH_MODE == BENCH_MODE_CTASK
        task = xf_ctask_create(ctask_worker, NULL, (uint16_t)s_cfg.priority, (size_t)s_cfg.stack_size);
#else
        task = xf_ttask_create_loop(ttask_worker, NULL, (uint16_t)s_cfg.priority, (uint32_t)s_cfg.delay_ms);
#endif
        if (task == NULL) {
            break;
        }
        s_cfg.created_tasks++;
    }
}

int main(void)
{
    bench_load_config();

    setvbuf(stdout, NULL, _IONBF, 0);
#if BENCH_MODE == BENCH_MODE_CTASK
#if !XF_TASK_CONTEXT_IS_ENABLE
#error "ctask benchmark requires XF_TASK_CONTEXT_DISABLE=0"
#endif
    xf_task_context_init(create_context, swap_context);
#endif
    xf_task_tick_init(task_get_tick);
    xf_task_manager_default_init(s_cfg.busy_idle ? idle_no_sleep : task_on_idle);

    bench_create_tasks();

    if (s_cfg.created_tasks == 0) {
        printf("bench: no tasks created, check BENCH_MODE/BENCH_TASKS\n");
        return 1;
    }

    printf("bench_config: mode=%s target=%d created=%d delay_ms=%d work=%d progress_ms=%d duration_s=%d busy_idle=%d priority=%d stack_size=%d\n",
           s_cfg.mode,
           s_cfg.task_target, s_cfg.created_tasks, s_cfg.delay_ms, s_cfg.work, s_cfg.progress_ms, s_cfg.duration_s,
           s_cfg.busy_idle, s_cfg.priority,
           s_cfg.stack_size);

    xf_ttask_create_loop(progress_task, NULL, 0, (uint32_t)s_cfg.progress_ms);

    while (1) {
        xf_task_manager_run_default();
    }

    return 0;
}
