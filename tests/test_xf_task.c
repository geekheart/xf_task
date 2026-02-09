#include "unity.h"
#include "xf_task.h"

static xf_task_time_t g_ticks = 0;
static int g_idle_called = 0;
static unsigned long g_idle_last = 0;

static xf_task_time_t fake_clock(void)
{
    return g_ticks;
}

static void set_ticks(xf_task_time_t ticks)
{
    g_ticks = ticks;
}

static void idle_cb(unsigned long max_idle_ms)
{
    g_idle_called++;
    g_idle_last = max_idle_ms;
}

static int g_counter = 0;
static void ttask_inc(xf_task_t task)
{
    (void)task;
    g_counter++;
}

static int g_order[8] = {0};
static int g_order_idx = 0;

static void task_a(xf_task_t task)
{
    (void)task;
    g_order[g_order_idx++] = 1;
}

static void task_b(xf_task_t task)
{
    (void)task;
    g_order[g_order_idx++] = 2;
}

static xf_task_t g_current_seen = NULL;
static void task_record_current(xf_task_t task)
{
    xf_task_manager_t manager = xf_task_get_manager(task);
    g_current_seen = xf_task_manager_get_current_task(manager);
}

static int g_cb_sync = 0;
static int g_cb_async = 0;
static void mbus_cb_sync(const void *const data, void *user_data)
{
    (void)data;
    (void)user_data;
    g_cb_sync++;
}

static void mbus_cb_async(const void *const data, void *user_data)
{
    (void)data;
    (void)user_data;
    g_cb_async++;
}

static int g_event_hits = 0;
static void event_task(xf_task_t task)
{
    (void)task;
    g_event_hits++;
}

static int g_ntask_step = 0;
static int g_ntask_compare_ready = 0;
static int ntask_compare(xf_task_t task)
{
    (void)task;
    return g_ntask_compare_ready ? 0 : 1;
}

static void ntask_flow(xf_task_t task)
{
    XF_NTASK_BEGIN(task);
    g_ntask_step = 1;
    xf_ntask_yield();
    g_ntask_step = 2;
    xf_ntask_delay(5);
    g_ntask_step = 3;
    xf_ntask_until(ntask_compare);
    g_ntask_step = 4;
    xf_ntask_until_timeout(ntask_compare, 5);
    g_ntask_step = 5;
    xf_ntask_exit();
    XF_NTASK_END();
}

void setUp(void)
{
    g_counter = 0;
    g_order_idx = 0;
    for (int i = 0; i < 8; i++) {
        g_order[i] = 0;
    }
    g_ticks = 0;
    g_idle_called = 0;
    g_idle_last = 0;
    g_current_seen = NULL;
    g_cb_sync = 0;
    g_cb_async = 0;
    g_event_hits = 0;
    g_ntask_step = 0;
    g_ntask_compare_ready = 0;
}

void tearDown(void)
{
}

void test_default_manager_wrappers(void)
{
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_default_init(idle_cb));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_set_default_idle(idle_cb));

    xf_task_t task = xf_ttask_create(ttask_inc, NULL, 0, 5, 1);
    TEST_ASSERT_NOT_NULL(task);

    set_ticks(0);
    xf_task_manager_run_default();
    TEST_ASSERT_EQUAL_INT(0, g_counter);
    TEST_ASSERT_TRUE(g_idle_called > 0);

    set_ticks(5);
    xf_task_manager_run_default();
    TEST_ASSERT_EQUAL_INT(1, g_counter);

    TEST_ASSERT_NOT_NULL(xf_task_get_default_manager());
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_set_compensation_time_default(0));

    xf_task_t urgent = xf_ttask_create(ttask_inc, NULL, 0, 100, 1);
    TEST_ASSERT_NOT_NULL(urgent);
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_set_urgent_task(urgent, true));
}

void test_manager_urgent_task(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 2, .delay_ms = 100};
    xf_task_t task1 = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, task_a, NULL, 1, &config);
    xf_task_t task2 = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, task_b, NULL, 2, &config);
    TEST_ASSERT_NOT_NULL(task1);
    TEST_ASSERT_NOT_NULL(task2);

    xf_task_trigger(task1);
    xf_task_trigger(task2);
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_set_urgent_task_with_manager(manager, task2, true));

    set_ticks(0);
    xf_task_manager_run(manager);
    xf_task_manager_run(manager);

    TEST_ASSERT_EQUAL_INT(2, g_order_idx);
    TEST_ASSERT_EQUAL_INT(2, g_order[0]);
    TEST_ASSERT_EQUAL_INT(1, g_order[1]);

    xf_task_manager_delete(manager);
}

void test_manager_task_state_helpers(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 10};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_task_ready(manager, task));
    TEST_ASSERT_EQUAL_INT(XF_TASK_STATE_READY, xf_task_get_state(task));

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_task_blocked(manager, task));
    TEST_ASSERT_EQUAL_INT(XF_TASK_STATE_BLOCKED, xf_task_get_state(task));

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_task_suspend(manager, task));
    TEST_ASSERT_EQUAL_INT(XF_TASK_STATE_SUSPEND, xf_task_get_state(task));

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_task_destory(manager, task));
    TEST_ASSERT_EQUAL_INT(XF_TASK_STATE_DELETE, xf_task_get_state(task));

    set_ticks(0);
    xf_task_manager_run(manager);
    xf_task_manager_delete(manager);
}

void test_suspend_resume(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 10};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_suspend(task));

    set_ticks(10);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(0, g_counter);

    set_ticks(5);
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_resume(task));

    set_ticks(14);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(0, g_counter);

    set_ticks(15);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_counter);

    xf_task_manager_delete(manager);
}

void test_set_delay_and_timeout(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_set_delay(task, 20));

    set_ticks(10);
    xf_task_manager_run(manager);
    TEST_ASSERT_TRUE(xf_task_get_timeout(task) < 0);
    TEST_ASSERT_EQUAL_INT(0, g_counter);

    set_ticks(20);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_counter);

    xf_task_manager_delete(manager);
}

void test_compensation_time(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_manager_set_compensation_time(manager, 50));

    set_ticks(49);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(0, g_counter);

    set_ticks(50);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_counter);

    xf_task_manager_delete(manager);
}

void test_set_priority_and_ready_order(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task1 = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, task_a, NULL, 5, &config);
    xf_task_t task2 = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, task_b, NULL, 6, &config);
    TEST_ASSERT_NOT_NULL(task1);
    TEST_ASSERT_NOT_NULL(task2);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_set_priority(task2, 0));
    xf_task_trigger(task1);
    xf_task_trigger(task2);

    set_ticks(0);
    xf_task_manager_run(manager);
    xf_task_manager_run(manager);

    TEST_ASSERT_EQUAL_INT(2, g_order_idx);
    TEST_ASSERT_EQUAL_INT(2, g_order[0]);
    TEST_ASSERT_EQUAL_INT(1, g_order[1]);

    xf_task_manager_delete(manager);
}

void test_set_func_and_getters(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    int arg_val = 42;
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, task_a, &arg_val, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    TEST_ASSERT_EQUAL_PTR(&arg_val, xf_task_get_arg(task));
    TEST_ASSERT_EQUAL_PTR(manager, xf_task_get_manager(task));
    TEST_ASSERT_EQUAL_INT(XF_TASK_TYPE_TTASK, xf_task_get_type(task));
    TEST_ASSERT_EQUAL_INT(XF_TASK_STATE_BLOCKED, xf_task_get_state(task));
    TEST_ASSERT_EQUAL_INT(0, xf_task_get_priority(task));

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_set_func(task, task_b, &arg_val));
    xf_task_trigger(task);
    set_ticks(0);
    xf_task_manager_run(manager);

    TEST_ASSERT_EQUAL_INT(1, g_order_idx);
    TEST_ASSERT_EQUAL_INT(2, g_order[0]);

    xf_task_manager_delete(manager);
}

void test_manager_current_task(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, task_record_current, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    xf_task_trigger(task);
    set_ticks(0);
    xf_task_manager_run(manager);

    TEST_ASSERT_EQUAL_PTR(task, g_current_seen);

    xf_task_manager_delete(manager);
}

void test_ttask_count_and_reset(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 5, .delay_ms = 5};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ttask_set_count_max(task, 10));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ttask_set_count(task, 3));
    TEST_ASSERT_EQUAL_INT(3, (int)xf_ttask_get_count(task));

    set_ticks(5);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_counter);

    xf_task_reset(task);
    TEST_ASSERT_EQUAL_INT(10, (int)xf_ttask_get_count(task));

    xf_task_manager_delete(manager);
}

void test_task_pool_ttask_and_ntask(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_task_pool_t tpool = xf_ttask_pool_create_with_manager(2, manager, 5, 1);
    TEST_ASSERT_NOT_NULL(tpool);

    xf_task_t ttask = xf_task_init_from_pool(tpool, ttask_inc, NULL, 0);
    TEST_ASSERT_NOT_NULL(ttask);
    xf_task_trigger(ttask);

    set_ticks(5);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_counter);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_pool_delete(tpool));

    xf_task_pool_t npool = xf_ntask_pool_create_with_manager(2, manager);
    TEST_ASSERT_NOT_NULL(npool);
    xf_task_t ntask = xf_task_init_from_pool(npool, ntask_flow, NULL, 0);
    TEST_ASSERT_NOT_NULL(ntask);
    TEST_ASSERT_EQUAL_INT(XF_TASK_TYPE_NTASK, xf_task_get_type(ntask));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_pool_delete(npool));

    xf_task_manager_delete(manager);
}

void test_queue_api(void)
{
    xf_task_queue_t queue;
    int storage[3] = {0};
    int item = 0;

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_init(&queue, storage, sizeof(int), 3));
    TEST_ASSERT_TRUE(xf_task_queue_is_empty(&queue));

    item = 10;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_send(&queue, &item, XF_TASK_QUEUE_SEND_TO_BACK));
    item = 20;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_send(&queue, &item, XF_TASK_QUEUE_SEND_TO_BACK));

    TEST_ASSERT_EQUAL_INT(2, (int)xf_task_queue_count(&queue));
    TEST_ASSERT_EQUAL_INT(1, (int)xf_task_queue_available(&queue));

    item = 30;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_send(&queue, &item, XF_TASK_QUEUE_SEND_TO_BACK));
    TEST_ASSERT_TRUE(xf_task_queue_is_full(&queue));
    TEST_ASSERT_EQUAL_INT(XF_TASK_ERR_BUSY, xf_task_queue_send(&queue, &item, XF_TASK_QUEUE_SEND_TO_BACK));

    int *peek = (int *)xf_task_queue_peek(&queue);
    TEST_ASSERT_NOT_NULL(peek);
    TEST_ASSERT_EQUAL_INT(10, *peek);

    item = 0;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_receive(&queue, &item));
    TEST_ASSERT_EQUAL_INT(10, item);

    item = 40;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_send(&queue, &item, XF_TASK_QUEUE_SEND_TO_FRONT));

    TEST_ASSERT_EQUAL_INT(3, (int)xf_task_queue_count(&queue));

    item = 0;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_receive(&queue, &item));
    TEST_ASSERT_EQUAL_INT(40, item);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_remove_front(&queue));
    TEST_ASSERT_FALSE(xf_task_queue_is_empty(&queue));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_remove_front(&queue));
    TEST_ASSERT_TRUE(xf_task_queue_is_empty(&queue));

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_queue_reset(&queue));
}

void test_mbus_sync_async(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_reg_topic_with_manager(manager, 1, sizeof(int)));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_sub(1, mbus_cb_sync, NULL));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_sub(1, mbus_cb_async, NULL));

    int data = 123;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_pub_sync(1, &data));
    TEST_ASSERT_EQUAL_INT(1, g_cb_sync);
    TEST_ASSERT_EQUAL_INT(1, g_cb_async);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_pub_async(1, &data));
    set_ticks(0);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(2, g_cb_sync);
    TEST_ASSERT_EQUAL_INT(2, g_cb_async);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_unsub(1, mbus_cb_async));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_pub_sync(1, &data));
    TEST_ASSERT_EQUAL_INT(3, g_cb_sync);
    TEST_ASSERT_EQUAL_INT(2, g_cb_async);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_unsub_all(1));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_mbus_unreg_topic(1));

    xf_task_manager_delete(manager);
}

void test_event_or_and(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task_or = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, event_task, NULL, 0, &config);
    xf_task_t task_and = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, event_task, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task_or);
    TEST_ASSERT_NOT_NULL(task_and);

    xf_task_event_t event = xf_event_init(event);
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_event_reg(&event, task_or, 0x1, XF_TASK_EVENT_OR));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_event_reg(&event, task_and, 0x3, XF_TASK_EVENT_AND));

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_event_send(&event, 0x1));
    set_ticks(0);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_event_hits);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_event_send(&event, 0x3));
    xf_task_manager_run(manager);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(3, g_event_hits);

    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_event_unreg(&event, task_or));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_event_unreg(&event, task_and));

    xf_task_manager_delete(manager);
}

void test_ntask_flow_and_args(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_NTASK, ntask_flow, NULL, 0, NULL);
    TEST_ASSERT_NOT_NULL(task);

    int *buf = (int *)xf_ntask_args_create(task, "buf", sizeof(int));
    TEST_ASSERT_NOT_NULL(buf);
    *buf = 55;
    int *found = (int *)xf_ntask_args_find(task, "buf");
    TEST_ASSERT_EQUAL_INT(55, *found);

    int value = 123;
    xf_ntask_stack_t stack[1] = { {.addr = &value, .size = sizeof(value)} };
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ntask_stack_save(task, "buf", stack, 1));
    value = 0;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ntask_stack_load(task, "buf", stack, 1));
    TEST_ASSERT_EQUAL_INT(123, value);

    xf_task_t *ntaskp = (xf_task_t *)task;
    TEST_ASSERT_TRUE(xf_ntask_lc_is_first(ntaskp, "ntask_flow"));
    TEST_ASSERT_EQUAL_INT(0, (int)xf_ntask_get_lc(ntaskp, "ntask_flow"));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ntask_set_lc(ntaskp, "ntask_flow", 0));
    TEST_ASSERT_EQUAL_INT(XF_NTASK_NONE, (int)xf_ntask_get_exit_status(ntaskp));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ntask_set_exit_status(ntaskp, XF_NTASK_WAITING));
    TEST_ASSERT_EQUAL_INT(XF_NTASK_WAITING, (int)xf_ntask_get_exit_status(ntaskp));
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_ntask_set_exit_status(ntaskp, XF_NTASK_NONE));

    set_ticks(0);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(1, g_ntask_step);

    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(2, g_ntask_step);

    set_ticks(4);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(2, g_ntask_step);

    set_ticks(5);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(3, g_ntask_step);

    g_ntask_compare_ready = 1;
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(4, g_ntask_step);
    g_ntask_compare_ready = 0;

    set_ticks(9);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(4, g_ntask_step);

    set_ticks(10);
    xf_task_manager_run(manager);
    TEST_ASSERT_EQUAL_INT(5, g_ntask_step);

    xf_task_manager_delete(manager);
}

void test_hunger_enable_disable(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    xf_task_feel_hungery_enable(task, 10);
    xf_task_feel_hungery_disable(task);

    xf_task_manager_delete(manager);
}

#if XF_TASK_USER_DATA_IS_ENABLE
void test_user_data_api(void)
{
    xf_task_manager_t manager = xf_task_manager_create(NULL);
    TEST_ASSERT_NOT_NULL(manager);

    xf_ttask_config_t config = {.count = 1, .delay_ms = 100};
    xf_task_t task = xf_task_create_with_manager(manager, XF_TASK_TYPE_TTASK, ttask_inc, NULL, 0, &config);
    TEST_ASSERT_NOT_NULL(task);

    int user = 7;
    TEST_ASSERT_EQUAL_INT(XF_TASK_OK, xf_task_set_user_data(task, &user));
    TEST_ASSERT_EQUAL_PTR(&user, xf_task_get_user_data(task));

    xf_task_manager_delete(manager);
}
#endif

int main(void)
{
    if (xf_task_tick_init(fake_clock) != XF_TASK_OK) {
        return 1;
    }

    UNITY_BEGIN();
    RUN_TEST(test_default_manager_wrappers);
    RUN_TEST(test_manager_urgent_task);
    RUN_TEST(test_manager_task_state_helpers);
    RUN_TEST(test_suspend_resume);
    RUN_TEST(test_set_delay_and_timeout);
    RUN_TEST(test_compensation_time);
    RUN_TEST(test_set_priority_and_ready_order);
    RUN_TEST(test_set_func_and_getters);
    RUN_TEST(test_manager_current_task);
    RUN_TEST(test_ttask_count_and_reset);
    RUN_TEST(test_task_pool_ttask_and_ntask);
    RUN_TEST(test_queue_api);
    RUN_TEST(test_mbus_sync_async);
    RUN_TEST(test_event_or_and);
    RUN_TEST(test_ntask_flow_and_args);
    RUN_TEST(test_hunger_enable_disable);
#if XF_TASK_USER_DATA_IS_ENABLE
    RUN_TEST(test_user_data_api);
#endif
    return UNITY_END();
}
