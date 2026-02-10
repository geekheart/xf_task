/**
 * @file xf_task_config.h
 * @brief Benchmark configuration
 */

#ifndef __XF_TASK_CONFIG_H__
#define __XF_TASK_CONFIG_H__

#define USE_GNU_UC 0

#ifdef __cplusplus
extern "C" {
#endif

#define XF_TASK_CONTEXT_DISABLE 0
#define XF_TASK_HUNGER_ENABLE 0
#define XF_TASK_MBUS_ENABLE 0
#define XF_TASK_LOG_LEVEL XF_TASK_LOG_ERROR

#if USE_GNU_UC
#include <ucontext.h>
#define XF_TASK_CONTEXT_TYPE ucontext_t
#else
#define XF_TASK_CONTEXT_TYPE void*
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_TASK_CONFIG_H__
