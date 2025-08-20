/**
 * @file xf_task_config_template.h
 * @author your name (you@domain.com)
 * @brief 用于指导用户配置xf_task的模板文件
 * @version 0.1
 * @date 2025-08-20
 * 
 * @copyright Copyright (c) 2025
 * 
 */
 
 #ifndef __XF_TASK_CONFIG_TEMPLATE_H__
 #define __XF_TASK_CONFIG_TEMPLATE_H__
 
 /* ==================== [Includes] ========================================== */
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /* ==================== [Defines] =========================================== */
 
 /**
  * @brief xf_task的优先级级别，范围从1到1024。
  * 如果不定义，内部默认配置为24。
  * 
  */
#define XF_TASK_PRIORITY_LEVELS  24

/**
 * @brief 是否开启饥饿值功能
 * 
 */
#define XF_TASK_HUNGER_ENABLE 1

/**
 * @brief 是否使用用户参数
 * 
 */
#define XF_TASK_USER_DATA_ENABLE 1

/**
 * @brief 配置任务的时钟频率，单位为Hz。该值决定了xf_task_clock_t返回的单位时间。
 *  如果不定义，内部默认配置为1000Hz。
 * 
 */
#define XF_TASK_TICKS_FREQUENCY 1000

/**
 * @brief 该配置依赖 XF_TASK_CONTEXT_ENABLE 是否开启。
 * 如果开启，则需要定义 XF_TASK_CONTEXT_TYPE 的类型。
 * 该类型将用于任务上下文的创建和切换。
 */
#define XF_TASK_CONTEXT_TYPE void*

/**
 * @brief 是否使用标准库 stdint.h。
 * 如果不使用则需要自己提供相关类型定义。
 */
#define XF_TASK_STDINT_ENABLE 1

/**
 * @brief 是否使用标准库 stddef.h。
 * 如果不使用则需要提供size_t类型
 */
#define XF_TASK_STDDEF_ENABLE 1

/**
 * @brief 是否使用标准库 stdbool.h。
 * 如果不使用则需要提供bool类型的定义
 */
#define XF_TASK_STDBOOL_ENABLE 1

/**
 * @brief 是否使用标准库 stdlib.h。
 * 如果不使用则需要提供 xf_task_malloc 和 xf_task_free 函数的定义。
 */
#define XF_TASK_STDLIB_ENABLE 1

/**
 * @brief 是否使用标准库 string.h。
 * 如果不使用则需要提供以下函数的实现：
 * - xf_task_bzero
 * - xf_task_memcpy
 * - xf_task_strcmp
 * 
 */
#define XF_TASK_STRING_ENABLE 1

/**
 * @brief 是否使用标准库 stdio.h。
 * 如果不使用则需要提供 xf_task_printf 函数的实现。
 * 
 */
#define XF_TASK_STDIO_ENABLE 1

/**
 * @brief 是否打开 ctask 功能
 * 如果打开，则需要自己定义XF_TASK_CONTEXT_TYPE类型。
 * 对接则需要提供xf_task_context_create和xf_task_context_swap函数。
 * 使用xf_task_context_init注册上下文创建和切换函数。
 * 
 */
#define XF_TASK_CONTEXT_DISABLE

/**
 * @brief log屏蔽等级，由高到底如下：
 * - XF_TASK_LOG_NONE: 不输出日志
 * - XF_TASK_LOG_ERROR: 只输出错误日志
 * - XF_TASK_LOG_WARN: 输出错误和警告日志
 * - XF_TASK_LOG_INFO: 输出错误、警告和信息日志
 * - XF_TASK_LOG_DEBUG: 输出所有日志，包括调试信息
 * 默认为 XF_TASK_LOG_INFO。
 */
#define XF_TASK_LOG_LEVEL XF_TASK_LOG_INFO

/**
 * @brief 设置xf_task时间戳类型宏。
 * 默认为uint64_t。
 */
#define XF_TASK_TIME_TYPE uint64_t

/**
 * @brief 是否打开 MBUS 功能
 * 
 */
#define XF_TASK_MBUS_ENABLE 1

/**
 * @brief 是否打开任务池功能。
 * 
 */
#define XF_TASK_POOL_ENABLE 1

/**
 * @brief 是否使用消息队列功能。
 * 
 */
#define XF_TASK_QUEUE_ENABLE 1

 /* ==================== [Typedefs] ========================================== */
 
 /* ==================== [Global Prototypes] ================================= */
 
 /* ==================== [Macros] ============================================ */
 
 #ifdef __cplusplus
 } /* extern "C" */
 #endif
 
 #endif // __XF_TASK_CONFIG_TEMPLATE_H__
 