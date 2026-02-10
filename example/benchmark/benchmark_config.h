/**
 * @file benchmark_config.h
 * @brief Benchmark configuration (compile-time).
 */

#ifndef __BENCHMARK_CONFIG_H__
#define __BENCHMARK_CONFIG_H__

/* Benchmark mode */
#define BENCH_MODE_TTASK 1
#define BENCH_MODE_NTASK 2
#define BENCH_MODE_CTASK 3

#ifndef BENCH_MODE
#define BENCH_MODE BENCH_MODE_TTASK
#endif

#if BENCH_MODE == BENCH_MODE_TTASK
#define BENCH_MODE_STR "ttask"
#elif BENCH_MODE == BENCH_MODE_NTASK
#define BENCH_MODE_STR "ntask"
#elif BENCH_MODE == BENCH_MODE_CTASK
#define BENCH_MODE_STR "ctask"
#else
#define BENCH_MODE_STR "ttask"
#endif

/* Task creation */
#ifndef BENCH_TASKS
#define BENCH_TASKS 10000
#endif

#ifndef BENCH_MAX_TASKS
#define BENCH_MAX_TASKS 200000
#endif

/* Task behavior */
#ifndef BENCH_DELAY_MS
#define BENCH_DELAY_MS 1
#endif

#ifndef BENCH_WORK
#define BENCH_WORK 0
#endif

/* Runtime */
#ifndef BENCH_PROGRESS_MS
#define BENCH_PROGRESS_MS 1000
#endif

#ifndef BENCH_DURATION_S
#define BENCH_DURATION_S 10
#endif

#ifndef BENCH_BUSY_IDLE
#define BENCH_BUSY_IDLE 1
#endif

#ifndef BENCH_PRIORITY
#define BENCH_PRIORITY 1
#endif

#ifndef BENCH_STACK_SIZE
#define BENCH_STACK_SIZE 8192
#endif

#endif /* __BENCHMARK_CONFIG_H__ */
