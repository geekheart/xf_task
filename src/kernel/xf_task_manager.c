/**
 * @file xf_task_manager.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-02-29
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "xf_task_kernel_config.h"
#include "../port/xf_task_port_internal.h"
#include "xf_task_manager.h"
#include "xf_task_base.h"


/* ==================== [Defines] =========================================== */

#define TAG "manager"

/* ==================== [Typedefs] ========================================== */

typedef struct _xf_task_manager_handle_t {
    xf_task_t current_task;                         /*!< 当前执行任务 */
    xf_task_t urgent_task;                          /*!< 紧急任务 */
    xf_task_list_t ready_list[XF_TASK_PRIORITY_LEVELS];  /*!< 任务就绪队列 */
#if XF_TASK_READY_BITMAP_ENABLE
    uint32_t ready_bitmap[(XF_TASK_PRIORITY_LEVELS + 31U) / 32U];
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
    xf_task_base_t **timer_heap;                    /*!< 定时任务最小堆 */
    size_t timer_heap_size;
    size_t timer_heap_cap;
#endif
    xf_task_list_t blocked_list;                         /*!< 任务阻塞队列 */
    xf_task_list_t suspend_list;                         /*!< 任务挂起队列，挂起任务不参与调度，需要手动恢复 */
    xf_task_list_t destroy_list;                         /*!< 任务销毁队列，进行异步销毁 */
    xf_task_on_idle_t on_idle;                      /*!< 空闲任务回调 */
#if XF_TASK_HUNGER_IS_ENABLE
    xf_task_list_t hunger_list;                          /*!< 任务饥饿队列，达到其指定值进行跳跃 */
#endif // XF_TASK_HUNGER_IS_ENABLE
#if XF_TASK_CONTEXT_IS_ENABLE
    xf_task_context_t context;                      /*!< 调度器上下文 */
#endif // XF_TASK_CONTEXT_IS_ENABLE
} xf_task_manager_handle_t;

/* ==================== [Static Prototypes] ================================= */

static inline void xf_task_run(xf_task_base_t *task);
static inline void xf_task_update_timeout(xf_task_base_t *task, xf_task_time_t now);
#if XF_TASK_READY_BITMAP_ENABLE
static inline void xf_task_ready_list_add(xf_task_manager_handle_t *manager, xf_task_base_t *task, uint16_t prio);
static inline void xf_task_ready_list_del(xf_task_manager_handle_t *manager, xf_task_base_t *task);
static inline int xf_task_ready_find_first(xf_task_manager_handle_t *manager);
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
static inline void xf_task_timer_heap_init(xf_task_manager_handle_t *manager);
static inline void xf_task_timer_heap_free(xf_task_manager_handle_t *manager);
static inline bool xf_task_timer_heap_push(xf_task_manager_handle_t *manager, xf_task_base_t *task);
static inline xf_task_base_t *xf_task_timer_heap_pop(xf_task_manager_handle_t *manager);
static inline xf_task_base_t *xf_task_timer_heap_top(xf_task_manager_handle_t *manager);
static inline void xf_task_timer_heap_remove(xf_task_manager_handle_t *manager, xf_task_base_t *task);
#endif

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

xf_task_manager_t xf_task_manager_create(xf_task_on_idle_t on_idle)
{
    xf_task_manager_handle_t *manager = (xf_task_manager_handle_t *)xf_task_malloc(sizeof(xf_task_manager_handle_t));
    XF_TASK_ASSERT(manager, NULL, TAG, "memory alloc failed!");

    manager->current_task = NULL;
    manager->urgent_task = NULL;
    manager->on_idle = on_idle;

    for (size_t i = 0; i < XF_TASK_PRIORITY_LEVELS; i++) {
        xf_task_list_init(&manager->ready_list[i]);
    }
#if XF_TASK_READY_BITMAP_ENABLE
    for (size_t i = 0; i < (XF_TASK_PRIORITY_LEVELS + 31U) / 32U; i++) {
        manager->ready_bitmap[i] = 0U;
    }
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
    xf_task_timer_heap_init(manager);
#endif
    xf_task_list_init(&manager->blocked_list);
    xf_task_list_init(&manager->destroy_list);
    xf_task_list_init(&manager->suspend_list);
#if XF_TASK_HUNGER_IS_ENABLE
    xf_task_list_init(&manager->hunger_list);
#endif // XF_TASK_HUNGER_IS_ENABLE

    return (xf_task_manager_t)manager;
}

xf_task_err_t xf_task_manager_set_idle(xf_task_manager_t manager, xf_task_on_idle_t on_idle)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL!");
    XF_TASK_ASSERT(on_idle, XF_TASK_ERR_INVALID_ARG, TAG, "on_idle must not be NULL!");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;
    manager_handle->on_idle = on_idle;
    return XF_TASK_OK;
}

void xf_task_manager_delete(xf_task_manager_t manager)
{
    XF_TASK_ASSERT(manager, XF_TASK_RETURN_VOID, TAG, "manager must not be NULL!");
#if XF_TASK_TIMER_HEAP_ENABLE
    xf_task_timer_heap_free((xf_task_manager_handle_t *)manager);
#endif
    xf_task_free(manager);
}

void xf_task_manager_run(xf_task_manager_t manager)
{
    XF_TASK_ASSERT(manager, XF_TASK_RETURN_VOID, TAG, "manager_handle must not be NULL");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;
#if !XF_TASK_READY_BITMAP_ENABLE
    volatile uint32_t index = 0;
#endif
    // 阻塞的最小时间，即是空闲的最大时间
    volatile int32_t max_idle_ms = INT32_MAX;
    // 保存ticks用于后续校准最大空闲
    volatile uint32_t idle_time_ticks = 0;
    volatile bool is_get_func = false;
    xf_task_base_t *task, *_task;

    xf_task_time_t now = xf_task_get_ticks();

    // 阻塞任务队列处理（轮询任务）
    xf_task_list_for_each_entry_safe(task, _task, &manager_handle->blocked_list, xf_task_base_t, node) {
        // 更新信号
        task->vfunc->update(task, now);

        // 检查信号，如果符合则加入就绪
        if (XF_TASK_BITS_CHECK(task->signal, XF_TASK_SIGNAL_READY)) {
            xf_task_list_del_init(&task->node);
            xf_task_base_set_state(task, XF_TASK_STATE_READY); // 设置为就绪态
#if XF_TASK_READY_BITMAP_ENABLE
            xf_task_ready_list_add(manager_handle, task, task->priority);
#else
            xf_task_list_add_tail(&task->node, &manager_handle->ready_list[task->priority]);
#endif
            XF_TASK_BITS_SET0(task->signal, XF_TASK_SIGNAL_READY);
#if XF_TASK_HUNGER_IS_ENABLE
            if (XF_TASK_BITS_CHECK(task->flag, XF_TASK_FALG_FEEL_HUNGERY)) {
                xf_task_list_add_tail(&task->hunger_node, &manager_handle->hunger_list);
            }
#endif // XF_TASK_HUNGER_IS_ENABLE
        }

        // 事件触发任务，这里始终等于0，不会被计算进入
        // 计算阻塞任务中最小时间，如果进入空闲，则这里的时间是空闲任务最大时间
        if (task->delay == 0 || task->timeout > 0 || -task->timeout > max_idle_ms || task->state != XF_TASK_STATE_BLOCKED) {
            continue;
        }
        max_idle_ms = -task->timeout;
        idle_time_ticks = now;
    }

#if XF_TASK_TIMER_HEAP_ENABLE
    // 处理定时任务堆
    while (true) {
        xf_task_base_t *top = xf_task_timer_heap_top(manager_handle);
        if (top == NULL) {
            break;
        }
        if ((int64_t)((int64_t)top->wake_up - (int64_t)now) > 0) {
            break;
        }
        task = xf_task_timer_heap_pop(manager_handle);
        task->vfunc->update(task, now);

        if (XF_TASK_BITS_CHECK(task->signal, XF_TASK_SIGNAL_READY)) {
            xf_task_base_set_state(task, XF_TASK_STATE_READY);
#if XF_TASK_READY_BITMAP_ENABLE
            xf_task_ready_list_add(manager_handle, task, task->priority);
#else
            xf_task_list_add_tail(&task->node, &manager_handle->ready_list[task->priority]);
#endif
            XF_TASK_BITS_SET0(task->signal, XF_TASK_SIGNAL_READY);
#if XF_TASK_HUNGER_IS_ENABLE
            if (XF_TASK_BITS_CHECK(task->flag, XF_TASK_FALG_FEEL_HUNGERY)) {
                xf_task_list_add_tail(&task->hunger_node, &manager_handle->hunger_list);
            }
#endif
        } else if (task->state == XF_TASK_STATE_BLOCKED) {
            if (task->delay > 0 && !XF_TASK_BITS_CHECK(task->flag, XF_TASK_FLAG_POLL)) {
                if (!xf_task_timer_heap_push(manager_handle, task)) {
                    xf_task_list_add_tail(&task->node, &manager_handle->blocked_list);
                }
            } else {
                xf_task_list_add_tail(&task->node, &manager_handle->blocked_list);
            }
        }
    }

    // 更新最大空闲时间（定时堆顶）
    xf_task_base_t *heap_top = xf_task_timer_heap_top(manager_handle);
    if (heap_top != NULL) {
        int64_t remain = (int64_t)heap_top->wake_up - (int64_t)now;
        int32_t remain_ms = xf_task_ticks_to_msec(remain);
        if (remain_ms < 0) {
            remain_ms = 0;
        }
        if (remain_ms < max_idle_ms) {
            max_idle_ms = remain_ms;
            idle_time_ticks = now;
        }
    }
#endif // XF_TASK_TIMER_HEAP_ENABLE

    // 如果有紧急任务则优先执行紧急任务，并跳过后续调度
    if (NULL != manager_handle->urgent_task) {
        task = (xf_task_base_t *)manager_handle->urgent_task;
        manager_handle->urgent_task = NULL;
        xf_task_run(task);
        return;
    }

    // 就绪任务队列处理
    // 这里决定了它的优先级数值越小优先级越高
#if XF_TASK_READY_BITMAP_ENABLE
    int ready_index = xf_task_ready_find_first(manager_handle);
    if (ready_index >= 0) {
        task = xf_task_list_first_entry(&manager_handle->ready_list[ready_index], xf_task_base_t, node);
        xf_task_run(task);
        is_get_func = true;
    }
#else
    for (index = 0; index < XF_TASK_PRIORITY_LEVELS; index++) {
        if (xf_task_list_empty(&manager_handle->ready_list[index])) {
            continue;
        }
        // 选取相对最高优先级的任务作为执行任务
        if (false == is_get_func) {
            task = xf_task_list_first_entry(&manager_handle->ready_list[index], xf_task_base_t, node);
            xf_task_run(task);
            is_get_func = true;
            break;
        }
    }
#endif

    // 上述循环正常退出，则说明没有就绪任务，运行空闲任务
    if (false == is_get_func) {
        // 空闲时间，处理一下需要删除的任务
        xf_task_list_for_each_entry_safe(task, _task, &manager_handle->destroy_list, xf_task_base_t, node) {
            xf_task_list_del_init(&task->node);
            task->delete (task);
        }
        // 进一步修正空闲时间
        xf_task_time_t ticks = xf_task_get_ticks();
        max_idle_ms -=  xf_task_ticks_to_msec(ticks - idle_time_ticks);
        max_idle_ms = max_idle_ms < 0 ? 0 : max_idle_ms;
        // 执行空闲回调
        if (manager_handle->on_idle != NULL) {
            manager_handle->on_idle(max_idle_ms);
        }
    }
#if XF_TASK_HUNGER_IS_ENABLE
    else {
        // 对事件触发任务一视同仁
        // 对感受饥饿的任务进行临时优先级跳跃
        xf_task_list_for_each_entry_safe(task, _task, &manager_handle->hunger_list, xf_task_base_t, hunger_node) {
            xf_task_update_timeout(task, now);
            uint32_t level = 0;
            // 计算爬升等级
            if (task->timeout > 0 && task->hunger_time > 0) 
            {
                level = task->timeout / task->hunger_time;
            }
            
            // 限制爬升等级
            int priority = (int)task->priority - (int)level;
            if (priority < 0) {
                priority = 0;
            }

            // 重置其优先级
#if XF_TASK_READY_BITMAP_ENABLE
            xf_task_ready_list_del(manager_handle, task);
            xf_task_ready_list_add(manager_handle, task, (uint16_t)priority);
#else
            xf_task_list_del_init(&task->node);
            xf_task_list_add(&task->node, &manager_handle->ready_list[priority]);
#endif
        }
    }
#endif // XF_TASK_HUNGER_IS_ENABLE

}

xf_task_t xf_task_manager_get_current_task(xf_task_manager_t manager)
{
    XF_TASK_ASSERT(manager, NULL, TAG, "manager must not be NULL!");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    return manager_handle->current_task;
}

xf_task_err_t xf_task_manager_task_ready(xf_task_manager_t manager, xf_task_t task)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL!");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    xf_task_base_t *task_base = task;

#if XF_TASK_READY_BITMAP_ENABLE
    if (task_base->state == XF_TASK_STATE_READY && task_base->ready_index < XF_TASK_PRIORITY_LEVELS) {
        xf_task_ready_list_del(manager_handle, task_base);
    } else {
        xf_task_list_del_init(&task_base->node);
    }
#else
    xf_task_list_del_init(&task_base->node);
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
    if (task_base->timer_index >= 0) {
        xf_task_timer_heap_remove(manager_handle, task_base);
    }
#endif

    xf_task_base_set_state(task, XF_TASK_STATE_READY);
#if XF_TASK_READY_BITMAP_ENABLE
    xf_task_ready_list_add(manager_handle, task_base, task_base->priority);
#else
    xf_task_list_add_tail(&task_base->node, &manager_handle->ready_list[task_base->priority]);
#endif

    return XF_TASK_OK;
}

xf_task_err_t xf_task_manager_task_suspend(xf_task_manager_t manager, xf_task_t task)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL!");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    xf_task_base_t *task_base = task;

#if XF_TASK_READY_BITMAP_ENABLE
    if (task_base->state == XF_TASK_STATE_READY && task_base->ready_index < XF_TASK_PRIORITY_LEVELS) {
        xf_task_ready_list_del(manager_handle, task_base);
    } else {
        xf_task_list_del_init(&task_base->node);
    }
#else
    xf_task_list_del_init(&task_base->node);
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
    if (task_base->timer_index >= 0) {
        xf_task_timer_heap_remove(manager_handle, task_base);
    }
#endif

    xf_task_base_set_state(task, XF_TASK_STATE_SUSPEND);
    xf_task_list_add_tail(&task_base->node, &manager_handle->suspend_list);

    return XF_TASK_OK;
}

xf_task_err_t xf_task_manager_task_destory(xf_task_manager_t manager, xf_task_t task)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL!");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    xf_task_base_t *task_base = task;

#if XF_TASK_READY_BITMAP_ENABLE
    if (task_base->state == XF_TASK_STATE_READY && task_base->ready_index < XF_TASK_PRIORITY_LEVELS) {
        xf_task_ready_list_del(manager_handle, task_base);
    } else {
        xf_task_list_del_init(&task_base->node);
    }
#else
    xf_task_list_del_init(&task_base->node);
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
    if (task_base->timer_index >= 0) {
        xf_task_timer_heap_remove(manager_handle, task_base);
    }
#endif

    xf_task_base_set_state(task, XF_TASK_STATE_DELETE);
    xf_task_list_add_tail(&task_base->node, &manager_handle->destroy_list);

    return XF_TASK_OK;
}

xf_task_err_t xf_task_manager_task_blocked(xf_task_manager_t manager, xf_task_t task)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL!");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    xf_task_base_t *task_base = task;

#if XF_TASK_READY_BITMAP_ENABLE
    if (task_base->state == XF_TASK_STATE_READY && task_base->ready_index < XF_TASK_PRIORITY_LEVELS) {
        xf_task_ready_list_del(manager_handle, task_base);
    } else {
        xf_task_list_del_init(&task_base->node);
    }
#else
    xf_task_list_del_init(&task_base->node);
#endif
#if XF_TASK_TIMER_HEAP_ENABLE
    if (task_base->timer_index >= 0) {
        xf_task_timer_heap_remove(manager_handle, task_base);
    }
#endif

    xf_task_base_set_state(task, XF_TASK_STATE_BLOCKED);
#if XF_TASK_TIMER_HEAP_ENABLE
    if (task_base->delay > 0 && !XF_TASK_BITS_CHECK(task_base->flag, XF_TASK_FLAG_POLL)) {
        if (!xf_task_timer_heap_push(manager_handle, task_base)) {
            xf_task_list_add_tail(&task_base->node, &manager_handle->blocked_list);
        }
    } else {
        xf_task_list_add_tail(&task_base->node, &manager_handle->blocked_list);
    }
#else
    xf_task_list_add_tail(&task_base->node, &manager_handle->blocked_list);
#endif

    return XF_TASK_OK;
}

#if XF_TASK_CONTEXT_IS_ENABLE
xf_task_context_t *xf_task_manager_get_context(xf_task_manager_t manager)
{
    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    return &manager_handle->context;
}
#endif // XF_TASK_CONTEXT_IS_ENABLE

xf_task_err_t xf_task_set_urgent_task_with_manager(xf_task_manager_t manager, xf_task_t task, bool force)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL");
    XF_TASK_ASSERT(task, XF_TASK_ERR_INVALID_ARG, TAG, "task must not be NULL");

    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;

    if (manager_handle->urgent_task != NULL && !force) {
        return XF_TASK_ERR_BUSY;
    }

    manager_handle->urgent_task = task;
    xf_task_base_set_state(task, XF_TASK_STATE_READY);

    return XF_TASK_OK;
}

xf_task_err_t xf_task_manager_set_compensation_time(xf_task_manager_t manager, xf_task_time_t time_ms)
{
    XF_TASK_ASSERT(manager, XF_TASK_ERR_INVALID_ARG, TAG, "manager must not be NULL");

    // tickless状态下，通过偏移时间计算出补偿的时间，并给所有阻塞任务进行补偿
    xf_task_manager_handle_t *manager_handle = (xf_task_manager_handle_t *)manager;
    xf_task_base_t *task, *_task;
    xf_task_time_t compensation_ticks = xf_task_msec_to_ticks(time_ms);
    xf_task_list_for_each_entry_safe(task, _task, &manager_handle->blocked_list, xf_task_base_t, node) {
        task->wake_up -= compensation_ticks;
    }
#if XF_TASK_TIMER_HEAP_ENABLE
    for (size_t i = 0; i < manager_handle->timer_heap_size; i++) {
        manager_handle->timer_heap[i]->wake_up -= compensation_ticks;
    }
#endif

    return XF_TASK_OK;
}

/* ==================== [Static Functions] ================================== */

static inline void xf_task_run(xf_task_base_t *task)
{
    xf_task_manager_handle_t *manager = (xf_task_manager_handle_t *)task->manager;

#if XF_TASK_HUNGER_IS_ENABLE
    if (XF_TASK_BITS_CHECK(task->flag, XF_TASK_FALG_FEEL_HUNGERY)) {
        xf_task_list_del_init(&task->hunger_node);
    }
#endif // XF_TASK_HUNGER_IS_ENABLE

#if XF_TASK_READY_BITMAP_ENABLE
    if (task->ready_index < XF_TASK_PRIORITY_LEVELS) {
        xf_task_ready_list_del(manager, task);
    } else {
        xf_task_list_del_init(&task->node);
    }
#else
    xf_task_list_del_init(&task->node);                   // 从原有链表中脱离
#endif
    manager->current_task = task;                       // 放入当前执行的任务
    xf_task_update_timeout(task, xf_task_get_ticks());
    task->vfunc->exec(manager);                           // 执行任务
    manager->current_task = NULL;

    // 如果设置成功，则进入阻塞状态。如果设置不成功（删除或挂起）则不管它
    if (xf_task_base_set_state(task, XF_TASK_STATE_BLOCKED) == XF_TASK_OK) {
#if XF_TASK_TIMER_HEAP_ENABLE
        if (task->delay > 0 && !XF_TASK_BITS_CHECK(task->flag, XF_TASK_FLAG_POLL)) {
            if (!xf_task_timer_heap_push(manager, task)) {
                xf_task_list_add_tail(&task->node, &manager->blocked_list);
            }
        } else {
            xf_task_list_add_tail(&task->node, &manager->blocked_list);
        }
#else
        xf_task_list_add_tail(&task->node, &manager->blocked_list);
#endif
    }
}

static inline void xf_task_update_timeout(xf_task_base_t *task, xf_task_time_t now)
{
    int64_t timeout = (int64_t)now - (int64_t)task->wake_up;
    task->timeout = xf_task_ticks_to_msec(timeout);
}

#if XF_TASK_READY_BITMAP_ENABLE
static inline void xf_task_ready_list_add(xf_task_manager_handle_t *manager, xf_task_base_t *task, uint16_t prio)
{
    uint32_t word = (uint32_t)prio >> 5U;
    uint32_t bit = 1U << (prio & 31U);
    task->ready_index = prio;
    xf_task_list_add_tail(&task->node, &manager->ready_list[prio]);
    manager->ready_bitmap[word] |= bit;
}

static inline void xf_task_ready_list_del(xf_task_manager_handle_t *manager, xf_task_base_t *task)
{
    uint16_t prio = task->ready_index;
    uint32_t word = (uint32_t)prio >> 5U;
    uint32_t bit = 1U << (prio & 31U);
    xf_task_list_del_init(&task->node);
    task->ready_index = (uint16_t)XF_TASK_PRIORITY_LEVELS;
    if (xf_task_list_empty(&manager->ready_list[prio])) {
        manager->ready_bitmap[word] &= ~bit;
    }
}

static inline int xf_task_ready_find_first(xf_task_manager_handle_t *manager)
{
    for (uint32_t wi = 0; wi < (XF_TASK_PRIORITY_LEVELS + 31U) / 32U; wi++) {
        uint32_t w = manager->ready_bitmap[wi];
        if (w == 0U) {
            continue;
        }
        for (uint32_t bi = 0; bi < 32U; bi++) {
            if (w & (1U << bi)) {
                uint32_t idx = wi * 32U + bi;
                if (idx < XF_TASK_PRIORITY_LEVELS) {
                    return (int)idx;
                }
                return -1;
            }
        }
    }
    return -1;
}
#endif // XF_TASK_READY_BITMAP_ENABLE

#if XF_TASK_TIMER_HEAP_ENABLE
static inline void xf_task_timer_heap_init(xf_task_manager_handle_t *manager)
{
    manager->timer_heap = NULL;
    manager->timer_heap_size = 0;
    manager->timer_heap_cap = 0;
}

static inline void xf_task_timer_heap_free(xf_task_manager_handle_t *manager)
{
    if (manager->timer_heap != NULL) {
        xf_task_free(manager->timer_heap);
        manager->timer_heap = NULL;
    }
    manager->timer_heap_size = 0;
    manager->timer_heap_cap = 0;
}

static inline void xf_task_timer_heap_swap(xf_task_base_t **a, xf_task_base_t **b)
{
    xf_task_base_t *tmp = *a;
    *a = *b;
    *b = tmp;
    int32_t idx = (*a)->timer_index;
    (*a)->timer_index = (*b)->timer_index;
    (*b)->timer_index = idx;
}

static inline void xf_task_timer_heap_sift_up(xf_task_manager_handle_t *manager, size_t idx)
{
    while (idx > 0) {
        size_t parent = (idx - 1U) >> 1U;
        if (manager->timer_heap[parent]->wake_up <= manager->timer_heap[idx]->wake_up) {
            break;
        }
        xf_task_timer_heap_swap(&manager->timer_heap[parent], &manager->timer_heap[idx]);
        idx = parent;
    }
}

static inline void xf_task_timer_heap_sift_down(xf_task_manager_handle_t *manager, size_t idx)
{
    size_t size = manager->timer_heap_size;
    while (true) {
        size_t left = (idx << 1U) + 1U;
        size_t right = left + 1U;
        size_t smallest = idx;
        if (left < size && manager->timer_heap[left]->wake_up < manager->timer_heap[smallest]->wake_up) {
            smallest = left;
        }
        if (right < size && manager->timer_heap[right]->wake_up < manager->timer_heap[smallest]->wake_up) {
            smallest = right;
        }
        if (smallest == idx) {
            break;
        }
        xf_task_timer_heap_swap(&manager->timer_heap[smallest], &manager->timer_heap[idx]);
        idx = smallest;
    }
}

static inline bool xf_task_timer_heap_reserve(xf_task_manager_handle_t *manager, size_t new_cap)
{
    xf_task_base_t **new_heap = (xf_task_base_t **)xf_task_malloc(sizeof(xf_task_base_t *) * new_cap);
    if (new_heap == NULL) {
        return false;
    }
    for (size_t i = 0; i < manager->timer_heap_size; i++) {
        new_heap[i] = manager->timer_heap[i];
    }
    if (manager->timer_heap != NULL) {
        xf_task_free(manager->timer_heap);
    }
    manager->timer_heap = new_heap;
    manager->timer_heap_cap = new_cap;
    return true;
}

static inline bool xf_task_timer_heap_push(xf_task_manager_handle_t *manager, xf_task_base_t *task)
{
    if (task->timer_index >= 0) {
        xf_task_timer_heap_remove(manager, task);
    }
    if (manager->timer_heap_size == manager->timer_heap_cap) {
        size_t new_cap = manager->timer_heap_cap == 0 ? 16U : manager->timer_heap_cap * 2U;
        if (!xf_task_timer_heap_reserve(manager, new_cap)) {
            return false;
        }
    }
    size_t idx = manager->timer_heap_size++;
    manager->timer_heap[idx] = task;
    task->timer_index = (int32_t)idx;
    xf_task_timer_heap_sift_up(manager, idx);
    return true;
}

static inline xf_task_base_t *xf_task_timer_heap_pop(xf_task_manager_handle_t *manager)
{
    if (manager->timer_heap_size == 0) {
        return NULL;
    }
    xf_task_base_t *top = manager->timer_heap[0];
    top->timer_index = -1;
    manager->timer_heap_size--;
    if (manager->timer_heap_size > 0) {
        manager->timer_heap[0] = manager->timer_heap[manager->timer_heap_size];
        manager->timer_heap[0]->timer_index = 0;
        xf_task_timer_heap_sift_down(manager, 0);
    }
    return top;
}

static inline xf_task_base_t *xf_task_timer_heap_top(xf_task_manager_handle_t *manager)
{
    if (manager->timer_heap_size == 0) {
        return NULL;
    }
    return manager->timer_heap[0];
}

static inline void xf_task_timer_heap_remove(xf_task_manager_handle_t *manager, xf_task_base_t *task)
{
    int32_t idx = task->timer_index;
    if (idx < 0 || (size_t)idx >= manager->timer_heap_size) {
        task->timer_index = -1;
        return;
    }
    task->timer_index = -1;
    manager->timer_heap_size--;
    if ((size_t)idx == manager->timer_heap_size) {
        return;
    }
    manager->timer_heap[idx] = manager->timer_heap[manager->timer_heap_size];
    manager->timer_heap[idx]->timer_index = idx;
    xf_task_timer_heap_sift_down(manager, (size_t)idx);
    xf_task_timer_heap_sift_up(manager, (size_t)idx);
}
#endif // XF_TASK_TIMER_HEAP_ENABLE
