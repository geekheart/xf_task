/**
 * @file xf_task_config_internal.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief xf_task 模块内部配置总头文件。
 *        确保 xf_hal_config.h 的所有定义都有默认值。
 * @version 0.1
 * @date 2024-07-04
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_TASK_CONFIG_INTERNAL_H__
#define __XF_TASK_CONFIG_INTERNAL_H__

/* ==================== [Includes] ========================================== */

#include "xf_task_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/**
 * @brief 是否使用标准库 stdint.h。
 * 
 */
#if !defined(XF_TASK_STDINT_ENABLE) || (XF_TASK_STDINT_ENABLE)
#   define XF_TASK_STDINT_IS_ENABLE (1)
#else
#   define XF_TASK_STDINT_IS_ENABLE (0)
#endif

#if XF_TASK_STDINT_IS_ENABLE
#   include <stdint.h>
#endif

/**
 * @brief 是否使用标准库 stddef.h。
 * 
 */
#if !defined(XF_TASK_STDDEF_ENABLE) || (XF_TASK_STDDEF_ENABLE)
#   define XF_TASK_STDDEF_IS_ENABLE (1)
#else
#   define XF_TASK_STDDEF_IS_ENABLE (0)
#endif

#if XF_TASK_STDDEF_IS_ENABLE
#   include <stddef.h>
#endif

/**
 * @brief 是否使用标准库 stdbool.h。
 * 
 */
#if !defined(XF_TASK_STDBOOL_ENABLE) || (XF_TASK_STDBOOL_ENABLE)
#   define XF_TASK_STDBOOL_IS_ENABLE (1)
#else
#   define XF_TASK_STDBOOL_IS_ENABLE (0)
#endif

#if XF_TASK_STDBOOL_IS_ENABLE
#   include <stdbool.h>
#endif

/**
 * @brief 是否使用标准库 stdlib.h。
 * 
 */
#if !defined(XF_TASK_STDLIB_ENABLE) || (XF_TASK_STDLIB_ENABLE)
#   define XF_TASK_STDLIB_IS_ENABLE (1)
#else
#   define XF_TASK_STDLIB_IS_ENABLE (0)
#endif

#if XF_TASK_STDLIB_IS_ENABLE
#   include <stdlib.h>
#   define xf_task_malloc(size) malloc(size)
#   define xf_task_free(ptr) free(ptr)
#endif

/**
 * @brief 是否使用标准库 stdio.h。
 * 
 */
#if !defined(XF_TASK_STDIO_ENABLE) || (XF_TASK_STDIO_ENABLE)
#   define XF_TASK_STDIO_IS_ENABLE (1)
#else
#   define XF_TASK_STDIO_IS_ENABLE (0)
#endif

#if XF_TASK_STDIO_IS_ENABLE
#   include <stdio.h>
#   define xf_task_printf(format, ...) printf(format, ##__VA_ARGS__)
#endif

/**
 * @brief 是否使用标准库 string.h。
 * 
 */
#if !defined(XF_TASK_STRING_ENABLE) || (XF_TASK_STRING_ENABLE)
#   define XF_TASK_STRING_IS_ENABLE (1)
#else
#   define XF_TASK_STRING_IS_ENABLE (0)
#endif

#if XF_TASK_STRING_IS_ENABLE
#   include <string.h>
#   define xf_task_bzero(ptr, size) memset(ptr, 0, size)
#   define xf_task_memcpy(dest, src, size) memcpy(dest, src, size)
#   define xf_task_strcmp(str1, str2) strcmp(str1, str2)
#endif


/**
 * @brief 是否打开 ctask 功能。
 */
#if !defined(XF_TASK_CONTEXT_DISABLE) || (XF_TASK_CONTEXT_DISABLE)
#   define XF_TASK_CONTEXT_IS_ENABLE (0)
#else
#   define XF_TASK_CONTEXT_IS_ENABLE (1)
#endif

/**
 * @brief 设置 xf_task 时间戳类型宏。
 */
#ifndef XF_TASK_TIME_TYPE
#   define XF_TASK_TIME_TYPE uint64_t
#endif

/**
 * @brief Xf_task_log 相关
 * 
 */
#define XF_TASK_LOG_NONE             (0)
#define XF_TASK_LOG_ERROR            (1)
#define XF_TASK_LOG_WARN             (2)
#define XF_TASK_LOG_INFO             (3)
#define XF_TASK_LOG_DEBUG            (4)

/**
 * @brief log 等级，不同log等级会影响日志的输出。
 * 
 */
#ifndef XF_TASK_LOG_LEVEL
#   define XF_TASK_LOG_LEVEL XF_TASK_LOG_INFO
#endif

#define xf_task_log_level(level, tag, format, ...) xf_task_printf("%c-%s[:%d(%s)]: "format"\n", #level[7], tag, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#if XF_TASK_LOG_LEVEL >= XF_TASK_LOG_ERROR
#   define XF_TASK_LOGE(tag, format, ...)  xf_task_log_level(XF_TASK_LOG_ERROR, tag, format, ##__VA_ARGS__)
#else
#   define XF_TASK_LOGE(tag, format, ...)  (void)(tag)
#endif

#if XF_TASK_LOG_LEVEL >= XF_TASK_LOG_WARN
#   define XF_TASK_LOGW(tag, format, ...)  xf_task_log_level(XF_TASK_LOG_WARN, tag, format, ##__VA_ARGS__)
#else
#   define XF_TASK_LOGW(tag, format, ...)  (void)(tag)
#endif

#if XF_TASK_LOG_LEVEL >= XF_TASK_LOG_INFO
#   define XF_TASK_LOGI(tag, format, ...)  xf_task_log_level(XF_TASK_LOG_INFO, tag, format, ##__VA_ARGS__)
#else
#   define XF_TASK_LOGI(tag, format, ...)  (void)(tag)
#endif

#if XF_TASK_LOG_LEVEL >= XF_TASK_LOG_DEBUG
#   define XF_TASK_LOGD(tag, format, ...)  xf_task_log_level(XF_TASK_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#else
#   define XF_TASK_LOGD(tag, format, ...)  (void)(tag)
#endif

#define XF_TASK_RETURN_VOID 

#define XF_TASK_ASSERT(condition, retval, tag, format, ...) \
    do { \
        if (!(condition)) { \
            XF_TASK_LOGE((tag), format, ##__VA_ARGS__);\
            return retval; \
        } \
    } while (0)

#define XF_TASK_BITS_CHECK(src, bits_mask) (!!((src) & (bits_mask)))
#define XF_TASK_BITS_SET0(var, bits_mask) ((var) &= ~(bits_mask))
#define XF_TASK_BITS_SET1(var, bits_mask) ((var) |= (bits_mask))

/* ==================== [Typedefs] ========================================== */

/**
 * @brief xf_task 时间戳类型。
 */
typedef XF_TASK_TIME_TYPE xf_task_time_t;

/**
 * @brief xf_task 错误码枚举。
 * 
 */
typedef enum _xf_task_err_t
{
    XF_TASK_OK = 0,                /*!< 成功 */
    XF_TASK_ERR_NO_MEM,
    XF_TASK_ERR_INVALID_ARG,       /*!< 参数错误 */
    XF_TASK_ERR_INVALID_STATE,     /*!< 状态错误 */
    XF_TASK_ERR_NOT_SUPPORTED,     /*!< 不支持的操作 */
    XF_TASK_ERR_NOT_FOUND,
    XF_TASK_ERR_BUSY,              /*!< 资源忙 */
    XF_TASK_ERR_TIMEOUT,                 /*!< 超时 */
} xf_task_err_t;

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_TASK_CONFIG_INTERNAL_H__
