#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define K_MSEC(ms) (ms)
#define BT_ID_DEFAULT 0
#define BT_CONN_ROLE_PERIPHERAL 1
#define BT_ADDR_LE_STR_LEN 24
#define ZMK_ADV_CONN_NAME 0
#define BT_LE_ADV_CONN_DIR_LOW_DUTY(addr) (*(addr))

typedef int atomic_t;
static int atomic_get(atomic_t *a) { return *a; }
static void atomic_set(atomic_t *a, int value) { *a = value; }
static void atomic_clear(atomic_t *a) { *a = 0; }
static bool atomic_cas(atomic_t *a, int old_value, int new_value) {
    if (*a != old_value) { return false; }
    *a = new_value;
    return true;
}

struct k_work { void (*handler)(struct k_work *); bool queued; };
struct k_work_delayable { struct k_work work; bool scheduled; int64_t due; };
#define K_WORK_DEFINE(name, fn) struct k_work name = {.handler = fn}
#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.work.handler = fn}
static int64_t now;
static int k_work_schedule(struct k_work_delayable *work, int delay) {
    if (work->scheduled) { return 0; }
    work->scheduled = true;
    work->due = now + delay;
    return 1;
}
static int k_work_submit(struct k_work *work) { work->queued = true; return 1; }

typedef int bt_addr_le_t;
struct bt_conn { int role; bt_addr_le_t peer; int refs; };
struct bt_conn_info { int role; };
struct profile { bt_addr_le_t peer; bool open; bool connected; };
static struct profile profiles[2];
static int active_profile;
static bool zmk_ble_active_profile_is_open(void) { return profiles[active_profile].open; }
static bool zmk_ble_active_profile_is_connected(void) { return profiles[active_profile].connected; }
static bt_addr_le_t *zmk_ble_active_profile_addr(void) { return &profiles[active_profile].peer; }
static int bt_addr_le_cmp(const bt_addr_le_t *a, const bt_addr_le_t *b) { return *a - *b; }
static void bt_addr_le_to_str(const bt_addr_le_t *a, char *dest, size_t size) {
    snprintf(dest, size, "%d", *a);
}
static const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn) { return &conn->peer; }
static void bt_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info) {
    info->role = conn->role;
}
static struct bt_conn *bt_conn_lookup_addr_le(int id, bt_addr_le_t *addr) { return NULL; }
static void bt_conn_unref(struct bt_conn *conn) { conn->refs--; }
static int zmk_ble_ad[1];

enum advertising_type { ZMK_ADV_NONE, ZMK_ADV_DIR, ZMK_ADV_CONN } advertising_status;
#define CURR_ADV(adv) ((adv) << 4)
static int start_calls, stop_calls, start_error, stop_error;
static int resource_refs;
static bool advertising_on;
static void (*during_start)(void);
static int bt_le_adv_start(int parameters, const int *ad, size_t ad_count,
                           const void *sd, size_t sd_count) {
    start_calls++;
    if (during_start) { void (*fn)(void) = during_start; during_start = NULL; fn(); }
    if (resource_refs) { return -ENOMEM; }
    if (start_error) { return start_error; }
    assert(!advertising_on);
    advertising_on = true;
    return 0;
}
static int bt_le_adv_stop(void) {
    stop_calls++;
    if (stop_error) { return stop_error; }
    advertising_on = false;
    return 0;
}
static int profile_events;
static void profile_changed(struct k_work *work) { profile_events++; }
K_WORK_DEFINE(raise_profile_changed_event_work, profile_changed);
