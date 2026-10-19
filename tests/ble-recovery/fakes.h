#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#define CONFIG_ZMK_BLE_KEYBOARD_REPORT_QUEUE_SIZE 3
#define CONFIG_ZMK_BLE_CONSUMER_REPORT_QUEUE_SIZE 3
#define CONFIG_ZMK_BLE_MOUSE_REPORT_QUEUE_SIZE 3
#define CONFIG_ZMK_POINTING 1
#define CONFIG_ZMK_USB 1
#define CONFIG_ZMK_BLE 1
#define CONFIG_ZMK_UNRESPONSE_FIX_TX_STALL_MS 30000
#define IS_ENABLED(x) (x)
#define K_NO_WAIT 0
#define K_MSEC(x) (x)
#define K_SECONDS(x) ((x)*1000)
#define LOG_DBG(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define BT_CONN_STATE_CONNECTED 1
#define BT_CONN_STATE_DISCONNECTING 2
#define BT_SECURITY_L2 2
#define BT_HCI_ERR_REMOTE_USER_TERM_CONN 0x13
#define BT_ID_DEFAULT 0
#define ZMK_TRANSPORT_USB 0
#define ZMK_TRANSPORT_BLE 1
#define USB_DC_CONFIGURED 1
struct zmk_hid_keyboard_report_body { uint8_t keys; };
struct zmk_hid_consumer_report_body { uint8_t keys; };
struct zmk_hid_mouse_report_body { uint8_t keys; };
struct bt_conn { int id, state, refs; bool encrypted; };
struct bt_conn_info { int state; };
typedef int bt_addr_le_t;
static const bt_addr_le_t any = -1;
#define BT_ADDR_LE_ANY (&any)
struct zmk_endpoint_instance { int transport; struct { int profile_index; } ble; };
static struct zmk_endpoint_instance selected = {1, {0}};
static bt_addr_le_t addresses[] = {0,1};
static struct bt_conn connections[3];
static struct bt_conn *lookup[2];
static int64_t now;
static int notify_error, failures_left, security_calls, disconnect_calls, disconnect_error;
static int sent_count, sent_keys[2048], sent_host[2048], sent_attr[2048], attempts;
static int usb_status, feed_count;
static void (*notify_probe)(void);
struct bt_gatt_attr { bool encrypted; };
static struct { struct bt_gatt_attr attrs[15]; } hog_svc = {
    .attrs = {[6]={true}, [10]={true}, [14]={true}}
};
struct bt_gatt_notify_params { const struct bt_gatt_attr *attr; const void *data; size_t len; };
static int bt_addr_le_cmp(const bt_addr_le_t *a, const bt_addr_le_t *b) { return *a != *b; }
static struct zmk_endpoint_instance zmk_endpoints_selected(void) { return selected; }
static bt_addr_le_t *zmk_ble_profile_address(int i) { assert(i < 2); return &addresses[i]; }
static struct bt_conn *bt_conn_lookup_addr_le(int id, const bt_addr_le_t *addr) {
    struct bt_conn *c = lookup[*addr]; if(c) c->refs++; return c;
}
static struct bt_conn *zmk_ble_active_profile_conn(void) {
    return bt_conn_lookup_addr_le(0, &addresses[selected.ble.profile_index]);
}
static bool zmk_ble_active_profile_is_connected(void) {
    struct bt_conn *c = lookup[selected.ble.profile_index];
    return c && c->state == BT_CONN_STATE_CONNECTED;
}
static void bt_conn_unref(struct bt_conn *c) { assert(c && c->refs > 0); c->refs--; }
static int bt_conn_get_info(struct bt_conn *c, struct bt_conn_info *i) { i->state=c->state; return 0; }
static int bt_gatt_notify_cb(struct bt_conn *c, struct bt_gatt_notify_params *p) {
    attempts++;
    assert(p->attr == &hog_svc.attrs[6] || p->attr == &hog_svc.attrs[10] || p->attr == &hog_svc.attrs[14]);
    if (notify_probe) notify_probe();
    if (p->attr->encrypted && !c->encrypted) return -EPERM;
    if (failures_left) { if(failures_left > 0) failures_left--; return notify_error; }
    assert(c->state == 1 && sent_count < 2048);
    sent_keys[sent_count]=*(const uint8_t *)p->data;
    sent_attr[sent_count]=(int)(p->attr-hog_svc.attrs);
    sent_host[sent_count++]=c->id; return 0;
}
static int bt_conn_set_security(struct bt_conn *c, int level) { security_calls++; return 0; }
static int bt_conn_disconnect(struct bt_conn *c, int reason) {
    disconnect_calls++;
    if(disconnect_error) return disconnect_error;
    /* The real Zephyr API changes state before returning; completion is later. */
    c->state=BT_CONN_STATE_DISCONNECTING; return 0;
}
static int64_t k_uptime_get(void) { return now; }
static uint32_t k_uptime_get_32(void) { return (uint32_t)now; }
struct k_work { void (*handler)(struct k_work *); bool queued; };
struct k_work_q { int unused; };
static struct k_work_q hog_work_q;
struct k_work_delayable { struct k_work work; bool scheduled; int64_t due; };
#define K_WORK_DEFINE(n, f) struct k_work n = {.handler=f}
#define K_WORK_DELAYABLE_DEFINE(n, f) struct k_work_delayable n = {.work={.handler=f}}
static int k_work_schedule_for_queue(struct k_work_q *q, struct k_work_delayable *w, int delay) {
    if(w->scheduled) return 0;
    w->scheduled=true; w->due=now+delay; return 1;
}
static int k_work_reschedule_for_queue(struct k_work_q *q, struct k_work_delayable *w, int delay) {
    w->scheduled=true; w->due=now+delay; return 1;
}
static int k_work_submit_to_queue(struct k_work_q *q, struct k_work *w) { w->queued=true; return 1; }
static int k_work_schedule(struct k_work_delayable *w, int delay) {
    return k_work_schedule_for_queue(&hog_work_q,w,delay);
}
struct k_spinlock { bool locked; };
typedef int k_spinlock_key_t;
static int k_spin_lock(struct k_spinlock *l) { assert(!l->locked); l->locked=true; return 0; }
static void k_spin_unlock(struct k_spinlock *l, int key) { assert(l->locked); l->locked=false; }
struct k_msgq { size_t size; int count, capacity; unsigned char bytes[4096]; };
#define K_MSGQ_DEFINE(n, sz, cap, align) struct k_msgq n = {.size=sz, .capacity=cap}
static int k_msgq_put(struct k_msgq *q, const void *p, int wait) {
    if(q->count==q->capacity) return wait == K_NO_WAIT ? -ENOMSG : -EAGAIN;
    memcpy(q->bytes+q->count++*q->size,p,q->size); return 0;
}
static int k_msgq_get(struct k_msgq *q, void *p, int wait) {
    assert(wait==K_NO_WAIT); if(!q->count) return -ENOMSG;
    memcpy(p,q->bytes,q->size); q->count--; memmove(q->bytes,q->bytes+q->size,q->count*q->size); return 0;
}
static int wdt_channel=0;
static void *wdt;
static void wdt_feed(void *dev,int channel) { feed_count++; }
static int zmk_usb_get_status(void) { return usb_status; }
